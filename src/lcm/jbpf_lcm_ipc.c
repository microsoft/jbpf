// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sys/select.h>
#include <errno.h>

#include "jbpf_lcm_ipc.h"
#include "jbpf_logging.h"
#include "jbpf_io_utils.h"

struct jbpf_lcm_ipc_server_ctx
{
    struct sockaddr_un un_addr;
    int sockfd;
    atomic_bool is_running;
    jbpf_codeletset_load_cb load_cb;
    jbpf_codeletset_unload_cb unload_cb;
};

static int
jbpf_lcm_ipc_send_req(jbpf_lcm_ipc_address_t* address, jbpf_lcm_ipc_req_msg_s* msg)
{

    jbpf_lcm_ipc_resp_msg_s resp = {0};
    struct sockaddr_un server_addr = {0};
    int sockfd;
    int res = JBPF_LCM_IPC_REQ_FAIL;

    if (!address || !msg) {
        jbpf_logger(JBPF_ERROR, "Invalid address or message\n");
        return JBPF_LCM_IPC_REQ_FAIL;
    }

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sockfd == -1) {
        jbpf_logger(JBPF_ERROR, "Error creating socket: %s\n", strerror(errno));
        goto out;
    }

    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, address->path, sizeof(server_addr.sun_path) - 1);
    server_addr.sun_path[sizeof(server_addr.sun_path) - 1] = '\0';

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_un)) == -1) {
        jbpf_logger(JBPF_ERROR, "Error connecting to %s: %s\n", server_addr.sun_path, strerror(errno));
        goto out;
    }

    if (send_all(sockfd, msg, sizeof(jbpf_lcm_ipc_req_msg_s), 0) != sizeof(jbpf_lcm_ipc_req_msg_s)) {
        jbpf_logger(JBPF_ERROR, "Error sending message to %s: %s\n", server_addr.sun_path, strerror(errno));
        goto out;
    }

    if (recv_all(sockfd, &resp, sizeof(jbpf_lcm_ipc_resp_msg_s), MSG_WAITALL) != sizeof(jbpf_lcm_ipc_resp_msg_s)) {
        jbpf_logger(JBPF_ERROR, "Error receiving response from %s: %s\n", server_addr.sun_path, strerror(errno));
        goto out;
    }

    res = resp.outcome;

out:
    close(sockfd);
    return res;
}

jbpf_lcm_ipc_server_ctx_t
jbpf_lcm_ipc_server_init(jbpf_lcm_ipc_server_config_t* config)
{
    struct jbpf_lcm_ipc_server_ctx* server;

    if (!config) {
        jbpf_logger(JBPF_ERROR, "Invalid server configuration\n");
        return NULL;
    }

    server = calloc(1, sizeof(struct jbpf_lcm_ipc_server_ctx));

    if (!server) {
        jbpf_logger(JBPF_ERROR, "Failed to allocate memory for server context\n");
        return NULL;
    }

    server->is_running = ATOMIC_VAR_INIT(false);
    server->load_cb = config->load_cb;
    server->unload_cb = config->unload_cb;

    server->un_addr.sun_family = AF_UNIX;
    strncpy(server->un_addr.sun_path, config->address.path, sizeof(server->un_addr.sun_path) - 1);
    server->un_addr.sun_path[sizeof(server->un_addr.sun_path) - 1] = '\0';

    server->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server->sockfd == -1) {
        jbpf_logger(JBPF_ERROR, "Failed to create socket\n");
        goto socket_error;
    }

    unlink(server->un_addr.sun_path);

    if (bind(server->sockfd, (struct sockaddr*)&server->un_addr, sizeof(struct sockaddr_un)) == -1) {
        jbpf_logger(JBPF_ERROR, "Failed to bind socket\n");
        goto bind_error;
    }

    listen(server->sockfd, JBPF_LCM_IPC_REQ_BACKLOG);

    return server;

bind_error:
    close(server->sockfd);
socket_error:
    free(server);
    return NULL;
}

void
jbpf_lcm_ipc_server_destroy(jbpf_lcm_ipc_server_ctx_t server_ctx)
{
    close(server_ctx->sockfd);
    free(server_ctx);
}

void
jbpf_lcm_ipc_server_stop(jbpf_lcm_ipc_server_ctx_t server_ctx)
{

    if (!server_ctx) {
        return;
    }

    atomic_store(&server_ctx->is_running, false);
}

int
jbpf_lcm_ipc_server_start(jbpf_lcm_ipc_server_ctx_t server_ctx)
{

    int client_fd, outcome, select_result;
    fd_set readfds;
    struct timeval timeout;
    jbpf_lcm_ipc_req_msg_s req_msg = {0};
    jbpf_lcm_ipc_resp_msg_s resp_msg = {0};

    if (!server_ctx) {
        return -1;
    }

    server_ctx->is_running = true;

    jbpf_logger(JBPF_INFO, "Started LCM IPC server\n");

    while (atomic_load(&server_ctx->is_running)) {

        FD_ZERO(&readfds);
        FD_SET(server_ctx->sockfd, &readfds);

        // Select should timeout once every second
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        select_result = select(server_ctx->sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (select_result == -1) {
            jbpf_logger(JBPF_ERROR, "jbpf_lcm_ipc_server select failed. Terminating interface...\n");
            return -1;
        } else if (select_result == 0) {
            continue;
        }

        if ((client_fd = accept(server_ctx->sockfd, NULL, NULL)) == -1) {
            jbpf_logger(JBPF_ERROR, "Error receiving LCM API from a client\n");
        }

        if (recv_all(client_fd, &req_msg, sizeof(jbpf_lcm_ipc_req_msg_s), MSG_WAITALL) !=
            sizeof(jbpf_lcm_ipc_req_msg_s)) {
            jbpf_logger(JBPF_ERROR, "Received malformed request from client with fd %\n", client_fd);
        }

        memset(&resp_msg, 0, sizeof(jbpf_lcm_ipc_resp_msg_s));

        switch (req_msg.msg_type) {
        case JBPF_LCM_IPC_CODELETSET_LOAD:
            outcome = server_ctx->load_cb(&req_msg.msg.load_req_msg.req, &resp_msg.err_msg);
            break;

        case JBPF_LCM_IPC_CODELETSET_UNLOAD:
            outcome = server_ctx->unload_cb(&req_msg.msg.unload_req_msg.req, &resp_msg.err_msg);
            break;
        default:
            jbpf_logger(
                JBPF_ERROR,
                "Received unknown request message type (type %d), from client with fd %\n",
                req_msg.msg_type,
                client_fd);
            outcome = -1;
            break;
        }

        resp_msg.outcome = outcome == 0 ? JBPF_LCM_IPC_REQ_SUCCESS : JBPF_LCM_IPC_REQ_FAIL;

        if (send_all(client_fd, &resp_msg, sizeof(jbpf_lcm_ipc_resp_msg_s), 0) != sizeof(jbpf_lcm_ipc_resp_msg_s)) {
            jbpf_logger(
                JBPF_WARN,
                "Response failed to be sent to client with socket fd %d. Tearing down the connection\n",
                client_fd);
            close(client_fd);
        }
    }

    return 0;
}

int
jbpf_lcm_ipc_send_codeletset_load_req(jbpf_lcm_ipc_address_t* address, jbpf_codeletset_load_req_s* load_req)
{

    jbpf_lcm_ipc_req_msg_s msg = {0};

    if (!address || !load_req) {
        jbpf_logger(JBPF_ERROR, "Invalid address or load request\n");
        return -1;
    }

    msg.msg_type = JBPF_LCM_IPC_CODELETSET_LOAD;
    msg.msg.load_req_msg.req = *load_req;

    return jbpf_lcm_ipc_send_req(address, &msg);
}

int
jbpf_lcm_ipc_send_codeletset_unload_req(jbpf_lcm_ipc_address_t* address, jbpf_codeletset_unload_req_s* unload_req)
{

    jbpf_lcm_ipc_req_msg_s msg = {0};

    if (!address || !unload_req) {
        jbpf_logger(JBPF_ERROR, "Invalid address or unload request\n");
        return -1;
    }

    msg.msg_type = JBPF_LCM_IPC_CODELETSET_UNLOAD;
    msg.msg.unload_req_msg.req = *unload_req;

    return jbpf_lcm_ipc_send_req(address, &msg);
}
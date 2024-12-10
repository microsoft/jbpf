#ifndef JBPF_LCM_IPC_H_
#define JBPF_LCM_IPC_H_

#include "jbpf_lcm_ipc_msg.h"

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#define JBPF_LCM_IPC_NAME_LEN (512U)
#define JBPF_LCM_IPC_ADDRESS_LEN (1024U)
#define JBPF_LCM_IPC_REQ_BACKLOG (40U)
#define JBPF_DEFAULT_LCM_SOCKET "jbpf_lcm_ipc"

    typedef struct jbpf_lcm_ipc_server_ctx* jbpf_lcm_ipc_server_ctx_t;

    typedef int (*jbpf_codeletset_load_cb)(
        struct jbpf_codeletset_load_req* load_req, jbpf_codeletset_load_error_s* err);
    typedef int (*jbpf_codeletset_unload_cb)(
        struct jbpf_codeletset_unload_req* unload_req, jbpf_codeletset_load_error_s* err);

    typedef struct jbpf_lcm_ipc_address
    {
        char path[JBPF_LCM_IPC_ADDRESS_LEN];
    } jbpf_lcm_ipc_address_t;

    typedef struct jbpf_lcm_ipc_server_config
    {
        jbpf_lcm_ipc_address_t address;
        jbpf_codeletset_load_cb load_cb;
        jbpf_codeletset_unload_cb unload_cb;
    } jbpf_lcm_ipc_server_config_t;

    jbpf_lcm_ipc_server_ctx_t
    jbpf_lcm_ipc_server_init(jbpf_lcm_ipc_server_config_t* config);
    void
    jbpf_lcm_ipc_server_destroy(jbpf_lcm_ipc_server_ctx_t server_ctx);
    void
    jbpf_lcm_ipc_server_stop(jbpf_lcm_ipc_server_ctx_t server_ctx);

    int
    jbpf_lcm_ipc_server_start(jbpf_lcm_ipc_server_ctx_t server_ctx);

    int
    jbpf_lcm_ipc_send_codeletset_load_req(jbpf_lcm_ipc_address_t* address, jbpf_codeletset_load_req_s* load_req);
    int
    jbpf_lcm_ipc_send_codeletset_unload_req(jbpf_lcm_ipc_address_t* path, jbpf_codeletset_unload_req_s* unload_req);

#pragma once
#ifdef __cplusplus
}
#endif

#endif

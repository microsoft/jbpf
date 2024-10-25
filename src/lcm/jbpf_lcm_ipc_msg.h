// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_LCM_IPC_MSG_H_
#define JBPF_LCM_IPC_MSG_H_

#include "jbpf_lcm_api.h"

/**
 * @brief JBPF LCM IPC request message type
 * @ingroup lcm
 */
typedef enum jbpf_lcm_ipc_req_msg_type
{
    JBPF_LCM_IPC_CODELETSET_LOAD = 0,
    JBPF_LCM_IPC_CODELETSET_UNLOAD,
} jbpf_lcm_ipc_req_msg_type_e;

/**
 * @brief JBPF LCM IPC request outcome
 * @ingroup lcm
 */
typedef enum jbpf_lcm_ipc_req_outcome
{
    JBPF_LCM_IPC_REQ_SUCCESS = 0,
    JBPF_LCM_IPC_REQ_FAIL,
} jbpf_lcm_ipc_req_outcome_e;

/**
 * @brief JBPF LCM IPC codeletset load request message
 * @ingroup lcm
 */
typedef struct __attribute__((packed)) jbpf_lcm_ipc_codeletset_load_req_msg
{
    jbpf_codeletset_load_req_s req;
} jbpf_lcm_ipc_codeletset_load_req_msg_s;

/**
 * @brief JBPF LCM IPC codeletset unload request message
 * @ingroup lcm
 */
typedef struct __attribute__((packed)) jbpf_lcm_ipc_codeletset_unload_req_msg
{
    jbpf_codeletset_unload_req_s req;
} jbpf_lcm_ipc_codeletset_unload_req_msg_s;

/**
 * @brief JBPF LCM IPC request message
 * @ingroup lcm
 */
typedef struct __attribute__((packed)) jbpf_lcm_ipc_req_msg
{
    jbpf_lcm_ipc_req_msg_type_e msg_type;
    union
    {
        jbpf_lcm_ipc_codeletset_load_req_msg_s load_req_msg;
        jbpf_lcm_ipc_codeletset_unload_req_msg_s unload_req_msg;
    } msg;
} jbpf_lcm_ipc_req_msg_s;

/**
 * @brief JBPF LCM IPC response message
 * @ingroup lcm
 */
typedef struct __attribute__((packed)) jbpf_lcm_ipc_resp_msg
{
    jbpf_lcm_ipc_req_outcome_e outcome;
    jbpf_codeletset_load_error_s err_msg;
} jbpf_lcm_ipc_resp_msg_s;

#endif
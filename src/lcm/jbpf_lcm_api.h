// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_LCM_API_H_
#define JBPF_LCM_API_H_

#include "jbpf_io_defs.h"
#include "jbpf_common_types.h"

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief jbpf IO stream ID type.
     * @ingroup lcm
     */
    typedef struct jbpf_io_stream_id jbpf_io_stream_id_t;

#define JBPF_PATH_LEN (256U)
    /**
     * @brief File path type for jbpf.
     * @ingroup lcm
     */
    typedef char jbpf_file_path_t[JBPF_PATH_LEN];

#define JBPF_CODELETSET_NAME_LEN (256U)
    /**
     * @brief Codelet set name type for jbpf.
     * @ingroup lcm
     */
    typedef char jbpf_codeletset_name_t[JBPF_CODELETSET_NAME_LEN];

#define JBPF_CODELET_NAME_LEN (256U)
    /**
     * @brief Codelet name type for jbpf.
     * @ingroup lcm
     */
    typedef char jbpf_codelet_name_t[JBPF_CODELET_NAME_LEN];

#define JBPF_MAP_NAME_LEN (256U)
    /**
     * @brief Map name type for jbpf.
     * @ingroup lcm
     */
    typedef char jbpf_map_name_t[JBPF_MAP_NAME_LEN];

#define JBPF_MAX_ERR_MSG_SIZE (1024U)
    /**
     * @brief Error message type for jbpf.
     * @ingroup lcm
     */
    typedef char jbpf_err_msg_t[JBPF_MAX_ERR_MSG_SIZE];

#define JBPF_MAX_IO_CHANNEL (5U)
#define JBPF_MAX_LINKED_MAPS (10U)
#define JBPF_MAX_CODELETSET_DIGEST_LEN (1024U)
#define JBPF_MAX_CODELETS_IN_CODELETSET (16U)
#define JBPF_DIGEST_MAX_LEN (1024U)

#define JBPF_CODELET_LOAD_SUCCESS (0)
#define JBPF_CODELET_HOOK_NOT_EXIST (-1)
#define JBPF_CODELET_CREATION_FAIL (-2)
#define JBPF_CODELET_LOAD_FAIL (-3)
#define JBPF_CODELET_PARAM_INVALID (-4)

#define JBPF_CODELET_UNLOAD_SUCCESS (0)
#define JBPF_CODELET_UNLOAD_FAIL (-1)

#define JBPF_STREAM_ID_LEN (16U)

    /**
     * @brief Descriptor for linked maps in jbpf.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_linked_map_descriptor
    {
        jbpf_map_name_t map_name;                /**< Name of the map. */
        jbpf_codelet_name_t linked_codelet_name; /**< Name of the linked codelet. */
        jbpf_map_name_t linked_map_name;         /**< Name of the linked map. */
    } jbpf_linked_map_descriptor_s;

    /**
     * @brief Serialization/deserialization structure for jbpf IO.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_io_serde
    {
        jbpf_file_path_t file_path; /**< File path for the serialization/deserialization. */
    } jbpf_io_serde_s;

    /**
     * @brief Descriptor for jbpf IO channel.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_io_channel_desc
    {
        jbpf_io_channel_name_t name;   /**< Name of the IO channel. */
        jbpf_io_stream_id_t stream_id; /**< Stream ID for the IO channel. */
        bool has_serde;                /**< Indicator if serialization/deserialization is present. */
        jbpf_io_serde_s serde;         /**< Serialization/deserialization details. */
    } jbpf_io_channel_desc_s;

    /**
     * @brief Descriptor for a jbpf codelet.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_codelet_descriptor
    {
        jbpf_codelet_name_t codelet_name;                               /**< Name of the codelet. */
        jbpf_hook_name_t hook_name;                                     /**< Name of the associated hook. */
        jbpf_file_path_t codelet_path;                                  /**< File path to the codelet binary. */
        jbpf_codelet_priority_t priority;                               /**< Priority of the codelet. */
        jbpf_runtime_threshold_t runtime_threshold;                     /**< Runtime threshold for the codelet. */
        int num_in_io_channel;                                          /**< Number of input IO channels.
                                                                         *   @min 0
                                                                         *   @max JBPF_MAX_IO_CHANNEL
                                                                         *   @default 0
                                                                         */
        jbpf_io_channel_desc_s in_io_channel[JBPF_MAX_IO_CHANNEL];      /**< Descriptors for input IO channels. */
        int num_out_io_channel;                                         /**< Number of output IO channels.
                                                                         *   @min 0
                                                                         *   @max JBPF_MAX_IO_CHANNEL
                                                                         *   @default 0
                                                                         */
        jbpf_io_channel_desc_s out_io_channel[JBPF_MAX_IO_CHANNEL];     /**< Descriptors for output IO channels. */
        int num_linked_maps;                                            /**< Number of linked maps.
                                                                         *   @min 0
                                                                         *   @max JBPF_MAX_LINKED_MAPS
                                                                         *   @default 0
                                                                         */
        jbpf_linked_map_descriptor_s linked_maps[JBPF_MAX_LINKED_MAPS]; /**< Descriptors for linked maps. */
    } jbpf_codelet_descriptor_s;

    /**
     * @brief Identifier for a jbpf codelet set.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_codeletset_id
    {
        jbpf_codeletset_name_t name; /**< Name of the codelet set. */
    } jbpf_codeletset_id_t;

    /**
     * @brief Request structure for loading a jbpf codelet set.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_codeletset_load_req
    {
        jbpf_codeletset_id_t codeletset_id; /**< Identifier for the codelet set. */
        int num_codelet_descriptors;        /**< Number of codelet descriptors.
                                             *   @min 1
                                             *   @max JBPF_MAX_CODELETS_IN_CODELETSET
                                             *   @default 1
                                             */
        jbpf_codelet_descriptor_s
            codelet_descriptor[JBPF_MAX_CODELETS_IN_CODELETSET]; /**< Array of codelet descriptors. */
    } jbpf_codeletset_load_req_s;

    /**
     * @brief Request structure for unloading a jbpf codelet set.
     * @ingroup lcm
     */
    typedef struct __attribute__((packed)) jbpf_codeletset_unload_req
    {
        jbpf_codeletset_id_t codeletset_id; /**< Identifier for the codelet set. */
    } jbpf_codeletset_unload_req_s;

    /**
     * @brief Structure for holding an error message when loading a jbpf codelet set fails.
     * @ingroup lcm
     */
    typedef struct jbpf_codeletset_load_error
    {
        jbpf_err_msg_t err_msg; /**< Error message describing the failure. */
    } jbpf_codeletset_load_error_s;

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_LCM_API_H_ */

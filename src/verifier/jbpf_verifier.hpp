// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include "platform.hpp"

#define ERROR_MSG_LEN 1024

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief JBPF verifier result
     * @param verification_pass whether the verification passed
     * @param runtime_seconds the runtime of the verification
     * @param max_instruction_count Maximum instruction count of the program
     * @param err_msg Error message if any
     * @ingroup verifier
     */
    typedef struct jbpf_verifier_result
    {
        bool verification_pass;
        float runtime_seconds;
        unsigned long max_instruction_count;
        char err_msg[ERROR_MSG_LEN];
    } jbpf_verifier_result_t;

    /**
     * @brief Verify a JBPF program.
     * @param filename Filename
     * @param section Section
     * @param asmfile Asmfile
     * @return jbpf_verifier_result_t
     * @ingroup verifier
     */
    typedef jbpf_verifier_result_t (*jbpf_verify_func_t)(
        const char* filename, const char* section, const char* asmfile);

    jbpf_verifier_result_t
    jbpf_verify(const char* filename, const char* section, const char* asmfile);

#ifdef __cplusplus
}
#endif

/**
 * @brief Register a helper function.
 * @param helper_id Helper ID
 * @param prototype Prototype
 * @return void
 * @ingroup verifier
 */
void
jbpf_verifier_register_helper(int helper_id, struct EbpfHelperPrototype prototype);

/**
 * @brief Register a program type.
 * @param prog_type_id Program type ID
 * @param program_type Program type
 * @return void
 * @ingroup verifier
 */
void
jbpf_verifier_register_program_type(int prog_type_id, EbpfProgramType program_type);

/**
 * @brief Register a map type.
 * @param map_id Map ID
 * @param map_type Map type
 * @return void
 * @ingroup verifier
 */
int
jbpf_verifier_register_map_type(int map_id, EbpfMapType map_type);

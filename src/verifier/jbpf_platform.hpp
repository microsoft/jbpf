// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

extern const ebpf_platform_t g_ebpf_platform_jbpf;

/**
 * Get the platform for the JIT compiler.
 * return Pointer to the platform.
 */
EbpfHelperPrototype
jbpf_verifier_get_helper_prototype(int32_t n);

/**
 * Check if a helper is usable.
 * param n The helper number.
 * return True if the helper is usable.
 */
bool
jbpf_verifier_is_helper_usable(int32_t n);

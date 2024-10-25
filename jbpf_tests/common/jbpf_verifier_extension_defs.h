#ifndef JBPF_VERIFIER_EXTENSION_DEFS_H
#define JBPF_VERIFIER_EXTENSION_DEFS_H

#include "jbpf_defs.h"

// Context definiton extension
struct my_new_jbpf_ctx
{
    uint64_t data;
    uint64_t data_end;
    uint64_t meta_data;
    uint32_t static_field1;
    uint16_t static_field2;
    uint8_t static_field3;
};

#endif
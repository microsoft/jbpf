// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_LOGGING_H
#define JBPF_LOGGING_H

#include <stddef.h>
#include <stdarg.h>

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#define LOGGING_BUFFER_LEN 8192

#define JBPF_LOG_WRAP(s) \
    "["s                 \
    "]: "

#define FOREACH_LOG_LEVEL(LOG_LEVEL) \
    LOG_LEVEL(JBPF_DEBUG)            \
    LOG_LEVEL(JBPF_INFO)             \
    LOG_LEVEL(JBPF_WARN)             \
    LOG_LEVEL(JBPF_ERROR)            \
    LOG_LEVEL(JBPF_CRITICAL)

#define GENERATE_ENUM_LOG(ENUM) ENUM,
#define GENERATE_STRING(STRING) JBPF_LOG_WRAP(#STRING),

    typedef enum
    {
        FOREACH_LOG_LEVEL(GENERATE_ENUM_LOG)
    } jbpf_logging_level;

    typedef void (*jbpf_logger_cb)(jbpf_logging_level, const char*, ...);
    typedef void (*jbpf_va_logger_cb)(jbpf_logging_level, const char*, va_list arg);
    extern void (*jbpf_va_logger_func)(jbpf_logging_level level, const char* s, va_list arg);

    void
    jbpf_set_logging_level(jbpf_logging_level level);
    jbpf_logging_level
    jbpf_get_logging_level(void);

    void
    set_va_logging_function(jbpf_va_logger_cb func);

    void
    jbpf_logger(jbpf_logging_level, const char*, ...);
    void
    jbpf_va_logger(jbpf_logging_level, const char*, va_list arg);

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_LOGGING_H */

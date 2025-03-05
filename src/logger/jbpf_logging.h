// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_LOGGING_H
#define JBPF_LOGGING_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
    LOG_LEVEL(DEBUG)                 \
    LOG_LEVEL(INFO)                  \
    LOG_LEVEL(WARN)                  \
    LOG_LEVEL(ERROR)                 \
    LOG_LEVEL(CRITICAL)

#define _STR(x) _VAL(x)
#define _VAL(x) #x

    static inline const char*
    get_file_name(const char* file)
    {
        const char* p = strrchr(file, '/');
        return p ? p + 1 : file;
    }

    static inline char*
    get_domain(const char* file)
    {
        const char* p = strstr(file, "src/");
        if (p) {
            p += 4; // Move past "src/"
            const char* q = strchr(p, '/');
            if (q) {
                size_t len = q - p;
                char* domain = (char*)malloc(len + 1);
                if (!domain) {
                    return NULL; // Indicate failure
                }
                memcpy(domain, p, len);
                domain[len] = '\0';
                // make it uppercase
                for (size_t i = 0; i < len; i++) {
                    domain[i] = toupper(domain[i]);
                }
                return domain;
            }
        }
        return NULL;
    }

    // Function to generate the log prefix dynamically
    static inline const char*
    get_log_prefix(const char* file, const char* func, int line, const char* level)
    {
        static char buffer[256];
        char* domain = get_domain(file);
        // we may want to cache this, but for now we will just check the env var each time
        int JBPF_VERBOSE_LOGGING = getenv("JBPF_VERBOSE_LOGGING") != NULL;
        if (JBPF_VERBOSE_LOGGING) {
            if (domain) {
                snprintf(buffer, sizeof(buffer), "[JBPF][%s]:%s:%s:%d", domain, get_file_name(file), func, line);
            } else {
                snprintf(buffer, sizeof(buffer), "[JBPF]:%s:%s:%d", get_file_name(file), func, line);
            }
            free(domain);
            return buffer;
        } else {
            if (domain) {
                snprintf(buffer, sizeof(buffer), "[JBPF][%s]", domain);
            } else {
                snprintf(buffer, sizeof(buffer), "[JBPF]");
            }
            free(domain);
            return buffer;
        }
    }

#define JBPF_CRITICAL get_log_prefix(__FILE__, __func__, __LINE__, "CRITICAL"), CRITICAL
#define JBPF_ERROR get_log_prefix(__FILE__, __func__, __LINE__, "ERROR"), ERROR
#define JBPF_WARN get_log_prefix(__FILE__, __func__, __LINE__, "WARN"), WARN
#define JBPF_INFO get_log_prefix(__FILE__, __func__, __LINE__, "INFO"), INFO
#define JBPF_DEBUG get_log_prefix(__FILE__, __func__, __LINE__, "DEBUG"), DEBUG

#define GENERATE_ENUM_LOG(ENUM) ENUM,
#define GENERATE_STRING(STRING) JBPF_LOG_WRAP(#STRING),

    typedef enum
    {
        FOREACH_LOG_LEVEL(GENERATE_ENUM_LOG)
    } jbpf_logging_level;

    typedef void (*jbpf_logger_cb)(const char*, jbpf_logging_level, const char*, ...);
    typedef void (*jbpf_va_logger_cb)(const char*, jbpf_logging_level, const char*, va_list arg);
    extern void (*jbpf_va_logger_func)(const char* domain, jbpf_logging_level level, const char* s, va_list arg);

    void
    jbpf_set_logging_level(jbpf_logging_level level);

    jbpf_logging_level
    jbpf_get_logging_level(void);

    void
    set_va_logging_function(jbpf_va_logger_cb func);

    void
    jbpf_logger(const char*, jbpf_logging_level, const char*, ...);

    void
    jbpf_va_logger(const char*, jbpf_logging_level, const char*, va_list arg);

#pragma once
#ifdef __cplusplus
}
#endif

#endif /* JBPF_LOGGING_H */

// Copyright (c) Microsoft Corporation. All rights reserved.

#include "jbpf_logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char* LOG_LEVEL_STR[] = {FOREACH_LOG_LEVEL(GENERATE_STRING)};

jbpf_logging_level jbpf_logger_level = JBPF_DEBUG;

void
jbpf_default_logging(jbpf_logging_level level, const char* s, ...);

void
jbpf_default_va_logging(jbpf_logging_level level, const char* s, va_list arg);

jbpf_va_logger_cb jbpf_va_logger_func = jbpf_default_va_logging;

void
jbpf_default_va_logging(jbpf_logging_level level, const char* s, va_list arg)
{
    if (level >= jbpf_logger_level) {
        char output[LOGGING_BUFFER_LEN];

        // Get current time in milliseconds
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // Format the timestamp
        char timestamp[64];
        struct tm* tm_info = gmtime(&tv.tv_sec);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);
        snprintf(timestamp + strlen(timestamp), sizeof(timestamp) - strlen(timestamp), ".%06ldZ", tv.tv_usec);

        // Add timestamp and log level
        snprintf(output, LOGGING_BUFFER_LEN, "[%s] %s%s", timestamp, LOG_LEVEL_STR[level], s);

        FILE* where = level >= JBPF_INFO ? stderr : stdout;
        vfprintf(where, output, arg);
        fflush(where);
    }
}

void
jbpf_default_logging(jbpf_logging_level level, const char* s, ...)
{
    if (level >= jbpf_logger_level) {

        char output[LOGGING_BUFFER_LEN];
        snprintf(output, LOGGING_BUFFER_LEN, "%s%s", LOG_LEVEL_STR[level], s);
        va_list ap;
        va_start(ap, s);
        FILE* where = level >= JBPF_INFO ? stderr : stdout;
        vfprintf(where, output, ap);
        va_end(ap);
        fflush(where);
    }
}

void
jbpf_set_logging_level(jbpf_logging_level level)
{
    jbpf_logger_level = level;
}

jbpf_logging_level
jbpf_get_logging_level()
{
    return jbpf_logger_level;
}

void
jbpf_set_va_logging_function(jbpf_va_logger_cb func)
{
    jbpf_va_logger_func = func;
}

void
jbpf_logger(jbpf_logging_level logging_level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    jbpf_va_logger(logging_level, format, args);
    va_end(args);
}

void
jbpf_va_logger(jbpf_logging_level logging_level, const char* format, va_list arg)
{
    jbpf_va_logger_func(logging_level, format, arg);
}

// Copyright (c) Microsoft Corporation. All rights reserved.

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include "jbpf_logging.h"
#include "jbpf_test_def.h"
#include "jbpf.h"

static int sig_handler_called = 0;

static const char* LOG_LEVEL_STR[] = {FOREACH_LOG_LEVEL(GENERATE_STRING)};

void
jbpf_agent_logger(jbpf_logging_level level, const char* s, ...)
{
    if (level >= jbpf_get_logging_level()) {
        char output[LOGGING_BUFFER_LEN];
        snprintf(output, LOGGING_BUFFER_LEN, "%s%s", LOG_LEVEL_STR[level], s);
        va_list ap;
        va_start(ap, s);
        FILE* where = level >= ERROR ? stderr : stdout;
        vfprintf(where, output, ap);
        va_end(ap);
    }
}

void
sig_handler(int signo)
{
    if (sig_handler_called == 1) {
        return;
    }
    sig_handler_called = 1;
    jbpf_logger(JBPF_INFO, "Exiting jbpf...\n");
    jbpf_stop();
    jbpf_logger(JBPF_INFO, "OK! Goodbye!\n");
    exit(EXIT_SUCCESS);
}

int
handle_signal(void)
{
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        return 0;
    }
    if (signal(SIGTERM, sig_handler) == SIG_ERR) {
        return 0;
    }
    return -1;
}

// Fisher-Yates algorithm, which guarantees an unbiased shuffle
void
shuffle(int* array, size_t n)
{
    if (n > 1) {
        srand(time(NULL));
        size_t i;
        for (i = 0; i < n - 1; i++) {
            // Generate a random index
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            // Swap array[i] and array[j]
            int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }
}
# Copyright (c) Microsoft Corporation. All rights reserved.
ifeq ($(BUILD_TYPE),Debug)
    DEBUG_CFLAGS = -g
	DEBUG_LDFLAGS = -lgcov
else ifeq ($(BUILD_TYPE),AddressSanitizer)
	DEBUG_CFLAGS = -fsanitize=address
endif

AGENT_NAME := example_app
CODELET_NAME := example_codelet.o
INCLUDES := -I${JBPF_OUT_DIR}/inc -I/usr/include/aarch64-linux-gnu -I/usr/include/x86_64-linux-gnu
LDFLAGS := -L${JBPF_OUT_DIR}/lib -ljbpf -lck -lubpf -lmimalloc -lpthread -ldl -lrt ${DEBUG_LDFLAGS}
AGENT_FILE := example_app.cpp
CODELET_FILE := example_codelet.c
CODELET_CC := clang

CODELET_CFLAGS := -O2 -target bpf -Wall -DJBPF_DEBUG_ENABLED -D__x86_64__

.PHONY: all clean

all: clean codelet agent

codelet: ${CODELET_FILE}
	${CODELET_CC} ${CODELET_CFLAGS} ${INCLUDES} -c ${CODELET_FILE} -o ${CODELET_NAME} 

agent:
	g++ -std=c++17 $(INCLUDES) -o ${AGENT_NAME} $(AGENT_FILE) ${DEBUG_CFLAGS} ${LDFLAGS}

clean:
	rm -f ${AGENT_NAME} ${CODELET_NAME}


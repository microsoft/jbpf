# Copyright (c) Microsoft Corporation. All rights reserved.
ifeq ($(BUILD_TYPE),Debug)
    DEBUG_CFLAGS = -g
	DEBUG_LDFLAGS = -lgcov
else ifeq ($(BUILD_TYPE),AddressSanitizer)
	DEBUG_CFLAGS = -fsanitize=address
endif

AGENT_NAME := example_app
SERDE_NAME := libserde_io_lib_example.so
RECEIVER_NAME := json_udp_receiver
CODELET_NAME := example_codelet.o
INCLUDES := -I${JBPF_OUT_DIR}/inc -I/usr/include/aarch64-linux-gnu -I/usr/include/x86_64-linux-gnu
LDFLAGS := -L${JBPF_OUT_DIR}/lib -ljbpf -lck -lubpf -lmimalloc -lpthread -ldl -lrt ${DEBUG_LDFLAGS}
AGENT_FILE := example_app.cpp
RECEIVER_FILE := json_udp_receiver.cpp
SERDE_FILE := serde_io_lib_example.cpp
CODELET_FILE := example_codelet.c
CODELET_CC := clang

CODELET_CFLAGS := -O2 -target bpf -Wall -DJBPF_DEBUG_ENABLED -D__x86_64__

.PHONY: all clean

all: clean codelet agent receiver serde

codelet: ${CODELET_FILE}
	${CODELET_CC} ${CODELET_CFLAGS} ${INCLUDES} -c ${CODELET_FILE} -o ${CODELET_NAME} 

agent:
	g++ -std=c++17 $(INCLUDES) -o ${AGENT_NAME} ${AGENT_FILE} ${DEBUG_CFLAGS} ${LDFLAGS}

receiver:
	g++ -o ${RECEIVER_NAME} ${RECEIVER_FILE}

serde:
	g++ -o $(INCLUDES) -o ${SERDE_NAME} ${SERDE_FILE} -fPIC -shared

clean:
	rm -f ${AGENT_NAME} ${SERVER_NAME} ${CODELET_NAME} ${SERDE_NAME}


# Copyright (c) Microsoft Corporation. All rights reserved.
ifeq ($(BUILD_TYPE),Debug)
    DEBUG_CFLAGS = -g
	DEBUG_LDFLAGS = -lgcov
else ifeq ($(BUILD_TYPE),AddressSanitizer)
	DEBUG_CFLAGS = -fsanitize=address
endif

AGENT_NAME := example_app
PRIMARY_NAME := example_collect_control
CODELET_NAME := example_codelet.o
INCLUDES := -I${JBPF_OUT_DIR}/inc -I${JBPF_PATH}/src/common
AGENT_LDFLAGS := -L${JBPF_OUT_DIR}/lib -ljbpf -lck -lubpf -lmimalloc -lpthread -ldl -lrt ${DEBUG_LDFLAGS}
PRIMARY_LDFLAGS := -L${JBPF_OUT_DIR}/lib -ljbpf_io -lck -lmimalloc -lpthread -ldl -lrt ${DEBUG_LDFLAGS}
AGENT_FILE := example_app.cpp
PRIMARY_FILE := example_collect_control.cpp
CODELET_FILE := example_codelet.c
CODELET_CC := clang

CODELET_CFLAGS := -O2 -target bpf -Wall -DJBPF_DEBUG_ENABLED -D__x86_64__

.PHONY: all clean

all: clean codelet agent primary

codelet: ${CODELET_FILE}
	${CODELET_CC} ${CODELET_CFLAGS} ${INCLUDES} -c ${CODELET_FILE} -o ${CODELET_NAME} 

agent:
	g++ -std=c++17 $(INCLUDES) -o ${AGENT_NAME} $(AGENT_FILE) ${DEBUG_CFLAGS} ${AGENT_LDFLAGS}

primary:
	g++ -std=c++17 $(INCLUDES) -o ${PRIMARY_NAME} $(PRIMARY_FILE) ${DEBUG_CFLAGS} ${PRIMARY_LDFLAGS}

clean:
	rm -f ${AGENT_NAME} ${PRIMARY_NAME} ${CODELET_NAME}


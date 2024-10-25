
CC = clang
INC= -I${JBPF_PATH}/src/common -I${JBPF_PATH}/jbpf_tests/common -I${JBPF_PATH}/src/core 
CFLAGS = -O2 -target bpf -Wall -DJBPF_DEBUG_ENABLED -D__x86_64__
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

all: $(OBJECTS) 

$(OBJECTS): %.o : %.c
	$(CC) $(INC) $(CFLAGS) -c $<

clean :
	rm -f *.o 

cleanall : clean
	rm -f *.pb.h *_pb2.py *.json *_ut.xml 
	rm -rf tmp*

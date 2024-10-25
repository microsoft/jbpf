#ifndef JBPF_IO_IPC_INT_H
#define JBPF_IO_IPC_INT_H

// Parser for the IPC address
// The address should be in the format <protocol>://<address>, where protocol is one of "vsock" or "unix" (case
// insensitive). If <protocol> is unix, then <address> is a string with the name of the socket. If <protocol> is vsock,
// then <address> has the format cid:port. If CID is a name or 0, then VMADDR_CID_ANY is used instead, while if the port
// is omitted, JBPF_IO_IPC_VSOCK_DEFAULT_PORT is used instead return 0 on success or -1 if failed.
int
_jbpf_io_ipc_parse_addr(const char* jbpf_run_path, const char* addr, struct jbpf_io_ipc_proto_addr* dipc_primary_addr);

#endif
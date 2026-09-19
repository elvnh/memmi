#pragma once

#if OS_LINUX
    typedef int ipc_Socket;
#elif OS_WIN32
#    define _WINSOCK_DEPRECATED_NO_WARNINGS
#    include <winsock2.h>

    typedef SOCKET ipc_Socket;
#else
#    error Unsupported operating system
#endif

#define IPC_TIMEOUT_NONE 0

typedef struct {
    ipc_Socket server_socket;
    ipc_Socket client_socket; // Either ourselves, or the client from the servers point of view
    size_t message_size;
    bool ok;
} Ipc;

typedef enum {
    IPC_RECEIVE_OK,
    IPC_RECEIVE_DONE,
    IPC_RECEIVE_TIMEOUT,
    IPC_RECEIVE_ERROR,
} IpcReceiveResult;

Ipc              ipc_accept(int32_t port, size_t message_size);
Ipc              ipc_connect(int32_t port, size_t message_size);
bool             ipc_ok(Ipc ipc);
void             ipc_destroy(Ipc ipc);
bool             ipc_send(Ipc ipc, void *msg);
IpcReceiveResult ipc_receive_with_timeout(Ipc ipc, void *msg, uint32_t timeout_ms);

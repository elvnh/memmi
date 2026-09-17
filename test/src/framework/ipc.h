#pragma once

/*
  TODO:
  - Send/receive in a loop
  - Timeout for send?
*/

#define IPC_TIMEOUT_NONE 0

typedef struct {
    void *data;
    size_t message_size;
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

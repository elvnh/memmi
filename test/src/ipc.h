#pragma once

/*
  TODO:
  - Send/receive in a loop
  - Timeout for send?
*/

#define IPC_TIMEOUT_NONE 0

typedef struct {
    void *data;
} Ipc;

typedef enum {
    IPC_RECEIVE_OK,
    IPC_RECEIVE_TIMEOUT,
    IPC_RECEIVE_ERROR,
} IpcReceiveResult;

Ipc              ipc_accept(int port);
Ipc              ipc_connect(int port);
bool             ipc_ok(Ipc ipc);
void             ipc_destroy(Ipc ipc);
bool             ipc_send(Ipc ipc, void *buf, size_t buf_size);
IpcReceiveResult ipc_receive(Ipc ipc, void *buf, size_t buf_size, size_t *bytes_received, uint32_t timeout_ms);

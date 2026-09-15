#pragma once

/*
  TODO:
  - Timeout on send/receive
 */

typedef struct {
    void *data;
} Ipc;

Ipc    ipc_accept(int port);
Ipc    ipc_connect(int port);
bool   ipc_ok(Ipc ipc);
void   ipc_destroy(Ipc ipc);
bool   ipc_send(Ipc ipc, void *buf, size_t buf_size);
bool   ipc_receive(Ipc ipc, void *buf, size_t buf_size, size_t *bytes_received);

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ipc.c"
#include "test_common.h"
#include "debugger.h"

Ipc launch_debuggee(const char *path)
{
    // TODO: launch the program
    Ipc result = {0};

    // TODO: can you block while connecting?
    while (!ipc_ok(result)) {
        result = ipc_connect(TEST_IPC_PORT, sizeof(Message));
    }

    return result;
}

static Response receive_response(Ipc ipc, uint32_t timeout_ms)
{
    Message response_msg = {0};
    IpcReceiveResult receive_result = ipc_receive_with_timeout(ipc, &response_msg, timeout_ms);

    Response result = {0};

    switch (receive_result) {
        case IPC_RECEIVE_OK: {
            result = response_msg.response;
        } break;

        case IPC_RECEIVE_DONE: {
            result.kind = RES_EXITED;
        } break;

        case IPC_RECEIVE_TIMEOUT: {
            result.kind = RES_TIMEOUT;
        } break;

        case IPC_RECEIVE_ERROR: {
            result.kind = RES_ERROR;
        } break;

        default: {
            assert(0);
        } break;
    }

    return result;
}

Response send_command_with_timeout(Ipc ipc, Command command, uint32_t timeout_ms)
{
    Response result = {0};

    Message cmd_message = {0};
    cmd_message.command = command;

    bool send_result = ipc_send(ipc, &cmd_message);

    if (!send_result) {
        result.kind = RES_ERROR;
        assert(0);
    } else {
        result = receive_response(ipc, timeout_ms);
    }

    return result;
}

int main()
{
    Ipc ipc = launch_debuggee("./build/debuggee");

    Command cmd = {0};
    cmd.kind = CMD_DO_NOTHING;
    Response res = send_command_with_timeout(ipc, cmd, IPC_TIMEOUT_NONE);

    switch (res.kind) {
        case RES_ACK: {
            printf("RES_ACK\n");
        } break;

        case RES_ERROR: {
            printf("RES_ERROR\n");
        } break;

        case RES_EXITED: {
            printf("RES_EXITED\n");
        } break;

        case RES_TIMEOUT: {
            printf("RES_TIMEOUT\n");
        } break;
    }

    ipc_destroy(ipc);
}

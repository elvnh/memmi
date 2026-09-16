#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ipc.c"
#include "common.h"

static Command receive_command(Ipc ipc)
{
    Message message = {0};
    IpcReceiveResult recv_res = ipc_receive_with_timeout(ipc, &message, IPC_TIMEOUT_NONE);
    assert(recv_res == IPC_RECEIVE_OK);

    Command result = message.command;

    return result;
}

static bool send_response(Ipc ipc, Response response)
{
    Message message = {0};
    message.response = response;

    bool result = ipc_send(ipc, &message);

    return result;
}

static Ipc accept_debugger_connection(void)
{
    Ipc result = ipc_accept(TEST_IPC_PORT, sizeof(Message));

    if (!ipc_ok(result)) {
        assert(0);
    }

    return result;
}

int main()
{
    Ipc ipc = accept_debugger_connection();

    while (true) {
        Command cmd = receive_command(ipc);

        Response response = {0};

        switch (cmd.kind) {
            case CMD_DO_NOTHING: {
                printf("CMD_DO_NOTHING\n");
                response.kind = RES_ACK;
            } break;

            default: {
                assert(0);
            } break;
        }

        send_response(ipc, response);
    }

    ipc_destroy(ipc);
}

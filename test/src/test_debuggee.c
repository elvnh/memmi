#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ipc.c"
#include "test_common.h"

Command receive_command(Ipc ipc)
{
    Message message = {0};
    IpcReceiveResult recv_res = ipc_receive_with_timeout(ipc, &message, IPC_TIMEOUT_NONE);
    assert(recv_res == IPC_RECEIVE_OK);

    Command result = message.command;

    return result;
}

bool send_response(Ipc ipc, Response response)
{
    Message message = {0};
    message.response = response;

    bool result = ipc_send(ipc, &message);

    return result;
}

int main()
{
    Ipc ipc = ipc_accept(TEST_IPC_PORT, sizeof(Message));

    if (!ipc_ok(ipc)) {
        assert(0);

        return 1;
    }

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

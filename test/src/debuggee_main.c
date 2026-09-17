#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "command.h"
#include "response.h"
#include "common.h"
#include "ipc.c"

static Response handle_command(Command cmd);
static Ipc      accept_debugger_connection(void);
static Command  receive_command(Ipc ipc);
static bool     send_response(Ipc ipc, Response response);

int main()
{
    Ipc ipc = accept_debugger_connection();

    while (true) {
        Command cmd = receive_command(ipc);

        Response response = handle_command(cmd);

        send_response(ipc, response);
    }

    ipc_destroy(ipc);
}

static Response handle_command(Command cmd)
{
    Response result = {0};

    switch (cmd.kind) {
        case CMD_DO_NOTHING: {
            result = res_ack();
        } break;

        default: {
            assert(0);
        } break;
    }

    return result;
}

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


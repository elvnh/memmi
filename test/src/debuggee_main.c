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
static bool     receive_command(Ipc ipc, Command *cmd);
static bool     send_response(Ipc ipc, Response response);

int main()
{
    Ipc ipc = accept_debugger_connection();

    while (true) {
        Command cmd = {0};

        if (receive_command(ipc, &cmd)) {
            Response response = handle_command(cmd);

            send_response(ipc, response);
        } else {
            break;
        }
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

static bool receive_command(Ipc ipc, Command *cmd)
{
    bool result = false;

    Message message = {0};
    IpcReceiveResult recv_res = ipc_receive_with_timeout(ipc, &message, IPC_TIMEOUT_NONE);

    if (recv_res == IPC_RECEIVE_OK) {
        result = true;
        *cmd = message.command;
    }

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


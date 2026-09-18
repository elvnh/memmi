#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "command.h"
#include "response.h"
#include "common.h"
#include "ipc.c"

#define MAX_VARIABLE_COUNT 1024

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

/* Globals */
static struct {
    struct {
        VariableId next_id;
        Value values[MAX_VARIABLE_COUNT];
        ValueType types[MAX_VARIABLE_COUNT];
    } variables;
} g;

static VariableInfo get_variable(VariableId id)
{
    assert(id < MAX_VARIABLE_COUNT);

    VariableInfo info = {0};
    info.id = id;
    info.address = (uintptr_t)&g.variables.values[id];
    info.value = g.variables.values[id];
    info.type = g.variables.types[id];

    return info;
}

static VariableInfo set_variable(VariableId id, TypedValue typed_value)
{
    assert(id < MAX_VARIABLE_COUNT);

    g.variables.values[id] = typed_value.value;
    g.variables.types[id] = typed_value.type;

    VariableInfo result = get_variable(id);

    return result;
}

static VariableId declare_variable(TypedValue typed_value)
{
    assert(g.variables.next_id < MAX_VARIABLE_COUNT);

    VariableId id = g.variables.next_id++;
    set_variable(id, typed_value);

    return id;
}

static Response handle_command(Command cmd)
{
    Response result = {0};

    switch (cmd.kind) {
        case CMD_DO_NOTHING: {
            result = res_ack();
        } break;

        case CMD_GET_NEW_VARIABLE: {
            VariableId id = declare_variable(cmd.as.get_new_variable);
            VariableInfo info = set_variable(id, cmd.as.get_new_variable);
            result = res_variable_info(info);
        } break;

        case CMD_GET_VARIABLE: {
            VariableInfo info = get_variable(cmd.as.get_variable);

            result = res_variable_info(info);
        }
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


#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include "command.h"
#include "response.h"
#include "ipc.c"
#include "common.h"
#include "debugger.h"

#if !defined(DEBUGGEE_EXECUTABLE_NAME)
#    error Please define the filename of the debuggee executable in the build script.
#endif

static bool spawn_debuggee_process(Pid *pid);

#if defined(__linux__)
#    include "debugger_linux.c"
#else
#    error
#endif

Debuggee launch_debuggee()
{
    Pid pid = 0;
    bool launch_result = spawn_debuggee_process(&pid);
    assert(launch_result);

    Ipc ipc = {0};

    while (!ipc_ok(ipc)) {
        ipc = ipc_connect(TEST_IPC_PORT, sizeof(Message));
    }

    Debuggee result = {0};
    result.pid = pid;
    result.ipc = ipc;

    return result;
}

void destroy_debuggee(Debuggee debuggee)
{
    ipc_destroy(debuggee.ipc);
}

static Response receive_response(Debuggee debuggee, uint32_t timeout_ms)
{
    Message response_msg = {0};
    IpcReceiveResult receive_result = ipc_receive_with_timeout(debuggee.ipc, &response_msg, timeout_ms);

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

Response send_command_with_timeout(Debuggee debuggee, Command command, uint32_t timeout_ms)
{
    Response result = {0};

    Message cmd_message = {0};
    cmd_message.command = command;

    bool send_result = ipc_send(debuggee.ipc, &cmd_message);

    if (!send_result) {
        result.kind = RES_ERROR;
        assert(0);
    } else {
        result = receive_response(debuggee, timeout_ms);
    }

    return result;
}

Response send_command(Debuggee debuggee, Command command)
{
    Response result = send_command_with_timeout(debuggee, command, IPC_TIMEOUT_NONE);

    return result;
}

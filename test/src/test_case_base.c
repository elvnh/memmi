#include "ipc/ipc_all.c"

#include <string.h>
#include <memmi.c>

#if defined(__GNUC__)
#    define MAYBE_UNUSED __attribute__((unused))
#else
#    error MAYBE_UNUSED not defined for this compiler
#endif

#define REQUIRE(e)                                              \
    do {                                                        \
        if (!(e)) {                                             \
            fprintf(stderr, "\n*** TEST ASSERTION FAILED ***\n" \
                "Expression: '%s'\nTest case: %s\n%s:%d:\n\n",    \
                #e, __FILE__, __FILE__, __LINE__);              \
        } else {                                                \
            ++g__assertions_passed;                             \
        }                                                       \
        ++g__assertions_ran;                                    \
    } while (0)

/* Global variables */
static uint32_t g__assertions_passed;
static uint32_t g__assertions_ran;

void     test_case_main(Pid pid, Ipc ipc);
Response receive_response(Ipc ipc, uint32_t timeout_ms);
Response send_command_with_timeout(Ipc ipc, Command command, uint32_t timeout_ms);
Response send_command(Ipc ipc, Command command);
Pid      get_self_pid();

int main(int argc, char **argv)
{
    assert(argc > 1);
    // TODO: don't use atoi
    Pid pid = atoi(argv[1]);

    Ipc ipc = {0};

    while (!ipc_ok(ipc)) {
        ipc = ipc_connect(IPC_TEST_PORT, sizeof(Message));
    }

    test_case_main(pid, ipc);
    printf(IPC_TEST_OUTPUT_FMT_STRING, g__assertions_passed, g__assertions_ran);

    ipc_destroy(ipc);
}

Response receive_response(Ipc ipc, uint32_t timeout_ms)
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

Response send_command(Ipc ipc, Command command)
{
    Response result = send_command_with_timeout(ipc, command, IPC_TIMEOUT_NONE);

    return result;
}

#if defined(__linux__)
Pid get_self_pid()
{
    Pid result = getpid();
    return result;
}
#else
#    error Test case functions not yet defined for this OS.
#endif

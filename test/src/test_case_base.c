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

#if !defined(DEBUGGEE_EXECUTABLE_NAME)
#    error Please define the filename of the debuggee executable in the build script.
#endif

/* Global variables for keeping track of test statistics. */
static uint32_t g__assertions_passed;
static uint32_t g__assertions_ran;

static void     test_case_main();
static bool     spawn_debuggee_process(Pid *pid);
static Debuggee launch_debuggee();
static void     destroy_debuggee(Debuggee debuggee);
static Response receive_response(Debuggee debuggee, uint32_t timeout_ms);
Response        send_command_with_timeout(Debuggee debuggee, Command command, uint32_t timeout_ms);
Response        send_command(Debuggee debuggee, Command command);

int main()
{
    test_case_main();
    printf(IPC_TEST_OUTPUT_FMT_STRING, g__assertions_passed, g__assertions_ran);
}

Debuggee launch_debuggee()
{
    Pid pid = 0;
    bool launch_result = spawn_debuggee_process(&pid);
    assert(launch_result);

    Ipc ipc = {0};

    while (!ipc_ok(ipc)) {
        ipc = ipc_connect(IPC_TEST_PORT, sizeof(Message));
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

#if defined(__linux)
    static char *lnx_get_debuggee_path()
    {
        char *self_path = realpath("/proc/self/exe", 0);
        assert(self_path);

        ssize_t last_slash_index = strlen(self_path) - 1;

        while ((last_slash_index >= 0) && (self_path[last_slash_index] != '/')) {
            --last_slash_index;
        }

        assert(last_slash_index >= 0);

        char s[] = "../" DEBUGGEE_EXECUTABLE_NAME;
        size_t final_length = (last_slash_index + 1) + sizeof(s);
        char *result = realloc(self_path, final_length);

        strcpy(result + last_slash_index + 1, s);

        return result;
    }

    static bool spawn_debuggee_process(Pid *pid)
    {
        assert(pid);

        bool result = false;

        pid_t fork_result = fork();

        if (fork_result == -1) {
            result = false;
        } else if (fork_result == 0) {
            char *debuggee_path = lnx_get_debuggee_path();
            char *args[] = {debuggee_path, 0};

            execv(debuggee_path, args);
        } else {
            *pid = (Pid)fork_result;
            result = true;
        }


        return result;
    }
#else
#    error spawn_debuggee_process not yet defined for this OS
#endif

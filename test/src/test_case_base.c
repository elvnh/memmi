#include "ipc/ipc_all.c"

#include <string.h>

#define MEMMI_TEST_MODE
#include <memmi.c>

#if COMPILER_GCC
#    define MAYBE_UNUSED __attribute__((unused))
#elif COMPILER_MSVC
#    define MAYBE_UNUSED __pragma(warning(disable: 4189))
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

// The default timeout to use when checking whether the debuggee has hung.
#define TEST_DEFAULT_TIMEOUT_MS 100

/* Global variables */
MAYBE_UNUSED static uint32_t g__assertions_passed;
MAYBE_UNUSED static uint32_t g__assertions_ran;

/* Inter-process communication */
void     test_case_main(Pid pid, Ipc ipc);
Response receive_response(Ipc ipc, uint32_t timeout_ms);
Response send_command_with_timeout(Ipc ipc, Command command, uint32_t timeout_ms);
Response send_command(Ipc ipc, Command command);
void     send_command_async(Ipc ipc, Command command);

/* Platform functions */
Pid      get_self_pid();

/* Utilities for writing test cases */
void helper_test_write_breakpoint(Pid pid, Ipc ipc, uint32_t index, memmi_BreakpointLength length);

/* Main */
int main(int argc, char **argv)
{
    assert(argc > 1);
    // TODO: don't use atoi
    Pid debuggee_pid = atoi(argv[1]);

    Ipc ipc = {0};

    while (!ipc_ok(ipc)) {
        ipc = ipc_connect(IPC_TEST_PORT, sizeof(Message));
    }

    test_case_main(debuggee_pid, ipc);
    printf(IPC_TEST_OUTPUT_FMT_STRING, g__assertions_passed, g__assertions_ran);

    ipc_destroy(ipc);
}

/* Inter-process communication */
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
    send_command_async(ipc, command);

    Response result = receive_response(ipc, timeout_ms);

    return result;
}

Response send_command(Ipc ipc, Command command)
{
    Response result = send_command_with_timeout(ipc, command, IPC_TIMEOUT_NONE);

    return result;
}

void send_command_async(Ipc ipc, Command command)
{
    Message cmd_message = {0};
    cmd_message.command = command;

    bool send_result = ipc_send(ipc, &cmd_message);
    assert(send_result);
}

/* Platform functions */
Pid get_self_pid()
{
    #if OS_LINUX
    Pid result = getpid();
    #elif OS_WIN32
    Pid result = (Pid)GetCurrentProcessId();
    #endif
    return result;
}

/* Utilities for writing test cases */
void helper_test_write_breakpoint(Pid pid, Ipc ipc, uint32_t index, memmi_BreakpointLength length)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
    assert(res.kind == RES_VARIABLE_INFO);

    VariableInfo var = res.as.variable_info;

    memmi_attach_to_process(proc);

    memmi_Status set_breakpoint_status = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_WRITE, index, length);
    REQUIRE(set_breakpoint_status == MEMMI_OK);

    // Now that a breakpoint is set on the address of this variable, writing to it should trigger
    // the breakpoint.
    send_command_async(ipc, cmd_set_variable(var.id, val_int32(0)));

    memmi_DebugEvent event = memmi_wait_for_debug_event(
        proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_BREAKPOINT);
    REQUIRE(event.as.breakpoint.breakpoint_index == index);
}

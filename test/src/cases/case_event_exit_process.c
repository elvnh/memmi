#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;
    memmi_attach_to_process(proc);

    // Attaching suspends the process which is why we need to send the command asynchronously.
    send_command_async(ipc, cmd_exit_process(123));

    // First we should receive an event reporting that the main thread has exited.
    memmi_DebugEvent event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_THREAD_EXITED);
    REQUIRE(event.as.exit_code == 123);

    // Next, an event reporting that the entire process has exited should be reported.
    event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_PROCESS_EXITED);
    REQUIRE(event.as.exit_code == 123);
}

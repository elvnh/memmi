#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;
    memmi_attach_to_process(proc);

    send_command_async(ipc, cmd_exit_process(123));

    memmi_DebugEvent event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    assert(event.status == MEMMI_OK);

    // The process is now dead, so no debug events should be reported.
    event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    REQUIRE(event.status == MEMMI_NO_SUCH_PROCESS);
}

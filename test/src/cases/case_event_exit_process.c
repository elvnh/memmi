#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;
    memmi_attach_to_process(proc);

    // Attaching suspends the process which is why we need to send the command asynchronously.
    send_command_async(ipc, cmd_exit_process(123));
    memmi_DebugEvent event = memmi_wait_for_debug_event(proc, MEMMI_TIMEOUT_INFINITE);

    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_PROCESS_EXITED);
    REQUIRE(event.as.exit_code == 123);
}

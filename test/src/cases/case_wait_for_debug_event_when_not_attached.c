#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    memmi_DebugEvent event = memmi_wait_for_debug_event(
        proc, MEMMI_CONTINUE_UNHANDLED, TEST_DEFAULT_TIMEOUT_MS);
    REQUIRE(event.status == MEMMI_INSUFFICIENT_PERMISSIONS);
}

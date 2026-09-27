#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    for (size_t i = 0; i < 25; ++i) {
        memmi_Status attach_result = memmi_attach_to_process(proc);
        REQUIRE(attach_result == MEMMI_OK);

        memmi_DebugEvent event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_HANDLED, 25);
        REQUIRE(event.status == MEMMI_OK);
        REQUIRE(event.kind == MEMMI_DEBUG_EVENT_NONE);

        memmi_Status detach_result = memmi_detach_from_process(proc);
        REQUIRE(detach_result == MEMMI_OK);

        // Now that we are detached, the process should respond as normal.
        Response res = send_command(ipc, cmd_do_nothing());
        REQUIRE(res.kind == RES_ACK);
    }
}

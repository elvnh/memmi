#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid).process;
    memmi_attach_to_process(proc);

    send_command_async(ipc, cmd_exit_process(123));

    memmi_EventList events = memmi_wait_for_debug_events(proc, memmi_default_allocator());
    REQUIRE(events.status == MEMMI_OK);
    REQUIRE(events.first);
    assert((events.first == events.last) && "There should only be one event");

    memmi_DebugEvent event = *events.first;
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_PROCESS_EXITED);
}

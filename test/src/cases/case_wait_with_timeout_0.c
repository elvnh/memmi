#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
    assert(res.kind == RES_VARIABLE_INFO);

    VariableInfo var = res.as.variable_info;

    memmi_attach_to_process(proc);

    // Cause a hardware breakpoint to be triggered.
    uint32_t index = 0;
    memmi_set_hardware_breakpoint(proc, var.address, MEMMI_BREAKPOINT_WRITE, index, MEMMI_BREAKPOINT_4_BYTES);

    send_command_async(ipc, cmd_set_variable(var.id, val_int32(0)));

    memmi_DebugEvent event = {0};

    // Busy loop with a timeout of 0 until we get the event.
    do {
        event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, 0);
    } while (event.kind == MEMMI_DEBUG_EVENT_NONE);

    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_BREAKPOINT);
    REQUIRE(event.as.breakpoint.breakpoint_index == index);
}

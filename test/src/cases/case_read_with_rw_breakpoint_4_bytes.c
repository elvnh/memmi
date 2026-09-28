#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
    assert(res.kind == RES_VARIABLE_INFO);

    VariableInfo var = res.as.variable_info;

    memmi_attach_to_process(proc);

    uint32_t index = 0;

    memmi_Status set_breakpoint_status = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_READ_WRITE, index, MEMMI_BREAKPOINT_4_BYTES);
    REQUIRE(set_breakpoint_status == MEMMI_OK);

    /* Now that a breakpoint is set on the address of this variable, reading from it should trigger
       the breakpoint. */
    send_command_async(ipc, cmd_get_variable(var.id));

    memmi_DebugEvent event = memmi_wait_for_debug_event(
        proc, MEMMI_CONTINUE_UNHANDLED, MEMMI_TIMEOUT_INFINITE);
    REQUIRE(event.status == MEMMI_OK);
    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_BREAKPOINT);
    REQUIRE(event.as.breakpoint.breakpoint_index == index);
}

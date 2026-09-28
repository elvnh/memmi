#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
    assert(res.kind == RES_VARIABLE_INFO);
    VariableInfo var = res.as.variable_info;

    memmi_attach_to_process(proc);

    memmi_Status set_breakpoint_result = 0;

    set_breakpoint_result = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_EXECUTE, 0, MEMMI_BREAKPOINT_2_BYTES);
    REQUIRE(set_breakpoint_result == MEMMI_INVALID_ARGUMENTS);

    set_breakpoint_result = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_EXECUTE, 0, MEMMI_BREAKPOINT_4_BYTES);
    REQUIRE(set_breakpoint_result == MEMMI_INVALID_ARGUMENTS);

    set_breakpoint_result = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_EXECUTE, 0, MEMMI_BREAKPOINT_8_BYTES);
    REQUIRE(set_breakpoint_result == MEMMI_INVALID_ARGUMENTS);

    set_breakpoint_result = memmi_set_hardware_breakpoint(
        proc, var.address, MEMMI_BREAKPOINT_EXECUTE, 0, 10);
    REQUIRE(set_breakpoint_result == MEMMI_INVALID_ARGUMENTS);
}

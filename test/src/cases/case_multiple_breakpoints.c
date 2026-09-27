#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    // Declare 4 different variables
    VariableInfo variable_infos[4];

    for (size_t i = 0; i < ARRAY_COUNT(variable_infos); ++i) {
        Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
        assert(res.kind == RES_VARIABLE_INFO);

        variable_infos[i] = res.as.variable_info;
    }

    memmi_attach_to_process(proc);

    // Set breakpoints on all variables
    for (size_t i = 0; i < ARRAY_COUNT(variable_infos); ++i) {
        VariableInfo info = variable_infos[i];

        memmi_Status set_breakpoint_status = memmi_set_hardware_breakpoint(
            proc, info.address, MEMMI_BREAKPOINT_WRITE, i, MEMMI_BREAKPOINT_4_BYTES);
        REQUIRE(set_breakpoint_status == MEMMI_OK);
    }

    // Write to all variables, which should trigger the different breakpoints
    for (size_t i = 0; i < ARRAY_COUNT(variable_infos); ++i) {
        VariableInfo info = variable_infos[i];

        send_command_async(ipc, cmd_set_variable(info.id, val_int32(0)));

        memmi_DebugEvent event = memmi_wait_for_debug_event(
            proc, MEMMI_CONTINUE_HANDLED, MEMMI_TIMEOUT_INFINITE);

        REQUIRE(event.status == MEMMI_OK);
        REQUIRE(event.kind == MEMMI_DEBUG_EVENT_BREAKPOINT);
        REQUIRE(event.as.breakpoint.breakpoint_index == i);
    }
}

#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    test_rerun_count(FLAKY_TEST_DEFAULT_RERUN_COUNT);

    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response res = send_command(ipc, cmd_declare_variable(val_int32(123)));
    assert(res.kind == RES_VARIABLE_INFO);

    VariableInfo var = res.as.variable_info;

    memmi_attach_to_process(proc);

    uint32_t index = 0;
    memmi_set_hardware_breakpoint(proc, var.address, MEMMI_BREAKPOINT_WRITE, index, MEMMI_BREAKPOINT_4_BYTES);

    uint32_t counter = 0;
    uint32_t counter_target = 100; // At which loop iteration we should cause the breakpoint to be triggered

    memmi_DebugEvent event = {0};
    while (event.kind != MEMMI_DEBUG_EVENT_BREAKPOINT) {
        event = memmi_wait_for_debug_event(proc, MEMMI_CONTINUE_UNHANDLED, 10);

        if (event.status != MEMMI_OK
            || ((event.kind != MEMMI_DEBUG_EVENT_NONE)
                && (event.kind != MEMMI_DEBUG_EVENT_BREAKPOINT))) {
            REQUIRE(false);
            LOG("%d\n", event.kind);

            break;
        }

        if (counter == counter_target) {
            // Cause a hardware breakpoint to be triggered.
            send_command_async(ipc, cmd_set_variable(var.id, val_int32(0)));
        }


        ++counter;
    }

    REQUIRE(event.kind == MEMMI_DEBUG_EVENT_BREAKPOINT);
}

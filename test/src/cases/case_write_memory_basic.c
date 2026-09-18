#include "test_case_base.c"

void test_case_main()
{
    Debuggee d = launch_debuggee();
    memmi_Process proc = memmi_open_process((memmi_PID) {d.pid}).process;

    Response response = send_command(d, cmd_get_new_variable(val_int32(0)));
    assert(response.kind == RES_VARIABLE_INFO);
    assert(response.as.variable_info.value.int32 == 0);

    uintptr_t address = response.as.variable_info.address;

    int32_t new_value = 123;
    memmi_WriteMemory write_result = memmi_write_memory(proc, address, &new_value, sizeof(new_value));
    REQUIRE(write_result.status == MEMMI_OK);

    Response response2 = send_command(d, cmd_get_variable(response.as.variable_info.id));
    assert(response2.kind == RES_VARIABLE_INFO);

    REQUIRE(response2.as.variable_info.value.int32 == 123);

}

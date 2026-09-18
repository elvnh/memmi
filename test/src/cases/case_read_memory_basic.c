#include "test_case_base.c"

void test_case_main()
{
    Debuggee d = launch_debuggee();
    memmi_Process proc = memmi_open_process((memmi_PID) {d.pid}).process;

    Response response = send_command(d, cmd_get_new_variable(val_int32(123)));
    assert(response.kind == RES_VARIABLE_INFO);

    memmi_ReadMemory read_mem_result = memmi_read_memory(
        proc,
        response.as.variable_info.address,
        sizeof(int32_t),
        memmi_default_allocator()
    );

    REQUIRE(read_mem_result.status == MEMMI_OK);

    int32_t value = 0;
    memcpy(&value, read_mem_result.memory, sizeof(int32_t));

    REQUIRE(value == 123);
}

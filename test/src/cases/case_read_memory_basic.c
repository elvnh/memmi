#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid).process;

    Response response = send_command(ipc, cmd_get_new_variable(val_int32(123)));
    assert(response.kind == RES_VARIABLE_INFO);

    int32_t value = 0;
    memmi_ReadMemory read_mem_result = memmi_read_memory(
        proc,
        &value,
        response.as.variable_info.address,
        sizeof(int32_t)
    );

    REQUIRE(read_mem_result.status == MEMMI_OK);
    REQUIRE(value == 123);
}

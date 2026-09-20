#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid).process;
    memmi_attach_to_process(proc);

    memmi_resume_process(proc);

    memmi_Status suspend_result = memmi_suspend_process(proc);
    REQUIRE(suspend_result == MEMMI_OK);

    Response res = send_command_with_timeout(ipc, cmd_do_nothing(), TEST_DEFAULT_TIMEOUT_MS);
    REQUIRE(res.kind == RES_TIMEOUT);

    memmi_Status resume_result = memmi_resume_process(proc);
    REQUIRE(resume_result == MEMMI_OK);

    res = send_command(ipc, cmd_do_nothing());
    REQUIRE(res.kind == RES_ACK);
}

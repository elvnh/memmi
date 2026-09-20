#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid).process;
    memmi_attach_to_process(proc);

    // Since attaching suspends the process, any command sent should time out.
    Response res = send_command_with_timeout(ipc, cmd_do_nothing(), TEST_DEFAULT_TIMEOUT_MS);
    REQUIRE(res.kind == RES_TIMEOUT);

    memmi_resume_process(proc);

    res = send_command(ipc, cmd_do_nothing());
    REQUIRE(res.kind == RES_ACK);
}

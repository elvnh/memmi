#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_PID memmi_pid = {pid};
    memmi_OpenProcess proc_opt = memmi_open_process(memmi_pid);
    REQUIRE(proc_opt.status == MEMMI_OK);

    memmi_Process proc = proc_opt.process;
    REQUIRE(proc.pid.value == pid);

    memmi_close_process(proc);
}

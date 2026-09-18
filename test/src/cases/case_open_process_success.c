#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_OpenProcess proc_opt = memmi_open_process(pid);
    REQUIRE(proc_opt.status == MEMMI_OK);

    memmi_Process proc = proc_opt.process;
    REQUIRE(proc.pid == pid);

    memmi_close_process(proc);
}

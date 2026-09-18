#include "test_case.c"

void test_case_main()
{
    Debuggee d = launch_debuggee();

    memmi_PID pid = {d.pid};
    memmi_OpenProcess proc_opt = memmi_open_process(pid);
    REQUIRE(proc_opt.status == MEMMI_OK);

    memmi_Process proc = proc_opt.process;
    REQUIRE(proc.pid.value == pid.value);

    memmi_close_process(proc);
}

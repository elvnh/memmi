#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_PID memmi_pid = {-1};
    memmi_OpenProcess proc = memmi_open_process(memmi_pid);
    REQUIRE(proc.status == MEMMI_NO_SUCH_PROCESS);
}

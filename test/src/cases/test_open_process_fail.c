#include "test_case.c"

void test_case_main()
{
    memmi_PID pid = {-1};
    memmi_OpenProcess proc = memmi_open_process(pid);
    REQUIRE(proc.status == MEMMI_NO_SUCH_PROCESS);
}

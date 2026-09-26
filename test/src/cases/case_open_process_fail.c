#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_OpenProcess proc = memmi_open_process(-1, memmi_default_allocator());
    REQUIRE(proc.status == MEMMI_NO_SUCH_PROCESS);
}

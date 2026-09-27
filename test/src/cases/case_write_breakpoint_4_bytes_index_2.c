#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    helper_test_write_breakpoint(pid, ipc, 2, MEMMI_BREAKPOINT_4_BYTES);
}

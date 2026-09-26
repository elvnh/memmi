#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;
    memmi_Status attach_result = memmi_attach_to_process(proc);
    REQUIRE(attach_result == MEMMI_OK);

    memmi_Status detach_result = memmi_detach_from_process(proc);
    REQUIRE(detach_result == MEMMI_OK);
}

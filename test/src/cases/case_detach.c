#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;
    memmi_attach_to_process(proc);
    memmi_detach_from_process(proc);

    // Now that we are detached, the process should respond as normal.
    Response res = send_command(ipc, cmd_do_nothing());
    REQUIRE(res.kind == RES_ACK);
}

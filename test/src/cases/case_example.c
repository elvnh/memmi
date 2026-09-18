#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    Response res = send_command(ipc, cmd_do_nothing());

    REQUIRE(res.kind == RES_ACK);
}

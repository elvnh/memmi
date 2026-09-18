#include "test_case_base.c"

void test_case_main()
{
    Debuggee d = launch_debuggee();
    Response res = send_command(d, cmd_do_nothing());

    REQUIRE(res.kind == RES_ACK);
}

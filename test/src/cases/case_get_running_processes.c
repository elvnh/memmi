#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_ProcessList procs = memmi_get_running_processes(memmi_default_allocator());
    REQUIRE(procs.status == MEMMI_OK);

    Pid self_pid = get_self_pid();

    bool found_self = false;
    bool found_debuggee = false;

    for (size_t i = 0; i < procs.count; ++i) {
        memmi_ProcessInfo info = procs.data[i];

        if (info.pid == self_pid) {
            assert(!found_self);
            found_self = true;
        } else if (info.pid == pid) {
            assert(!found_debuggee);
            found_debuggee = true;
        }
    }

    REQUIRE(found_self);
    REQUIRE(found_debuggee);
}

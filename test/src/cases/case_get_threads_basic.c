#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid).process;
    memmi_ThreadList threads = memmi_get_process_threads(proc, memmi_default_allocator());
    REQUIRE(threads.status == MEMMI_OK);
    REQUIRE(threads.count >= 1);

    bool found_main_thread = false;
    (void)found_main_thread;

    for (size_t i = 0; i < threads.count; ++i) {
        REQUIRE(threads.data[i] != 0);

        if (threads.data[i] == pid) {
            assert(!found_main_thread);
            found_main_thread = true;
        }
    }

    #if OS_LINUX
    // On Linux, the main thread has the same ID as the process.
    REQUIRE(found_main_thread);
    #endif
}

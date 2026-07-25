#include "process.h"
#include "utils.h"

int main(int argc, char **argv)
{

    ASSERT(argc > 2);

    int pid = atoi(argv[1]);
    memmi_OpenProcess proc_res = memmi_open_process((memmi_PID){pid}, memmi_default_allocator());
    ASSERT(proc_res.status == MEMMI_OK);

    memmi_Process proc = proc_res.process;

    memmi_Status attach_res = memmi_attach_to_process(proc);
    ASSERT(attach_res == MEMMI_OK);

    memmi_resume_process(proc);

    //uint64_t addr = (uint64_t)atoll(argv[2]);

    //memmi_ResumeStatus resume = memmi_resume_process(proc);
    /* ASSERT(resume == MEMMI_RESUME_OK); */

    /* memmi_set_hardware_breakpoint(proc, addr, MEMMI_BREAKPOINT_WRITE, 0, MEMMI_BREAKPOINT_4_BYTES); */
    memmi_Status sus = memmi_suspend_process(proc);
    ASSERT(sus == MEMMI_OK);


    while (true) {
        memmi_EventList events = memmi_wait_for_debug_events(proc, memmi_default_allocator());

        for (memmi_DebugEvent *event = events.first; event; event = event->next) {
            switch (event->kind) {
                case MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED: {
                    printf("new thread: %d\n", (int)event->as.new_thread.id.value);
                } break;

                case MEMMI_DEBUG_EVENT_THREAD_SUSPENDED: {
                    printf("thread suspended\n");
                } break;

                /* case MEMMI_DEBUG_EVENT_THREAD_STOPPED: { */
                /*     printf("thread stopped\n"); */
                /* } break; */

                case MEMMI_DEBUG_EVENT_THREAD_EXITED: {
                    printf("thread exited with code: %d\n", event->as.thread_exited.exit_code);
                } break;
                default: {

                } break;
            }
        }
    }

    memmi_Status detached = memmi_detach_from_process(proc);
    ASSERT(detached == MEMMI_OK);
}

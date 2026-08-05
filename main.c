#include "process.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{

#if 1
    memmi_ProcessList procs = memmi_get_running_processes(memmi_default_allocator());
    ASSERT(procs.status == MEMMI_OK);
    ASSERT(procs.count > 0);

    bool found = false;
    memmi_PID pid = {0};

    // TODO: processinfo name should be null terminated
    // TODO: we're finding very few processes
    for (size_t i = 0; i < procs.count; ++i) {
        memmi_ProcessInfo info = procs.data[i];

        //printf("%.*s\n", info.name.count, info.name.data);
        if (strncmp(info.name.data, "test.exe", info.name.count) == 0) {
            found = true;
            pid = info.pid;
            break;
        }
    }

    ASSERT(found);

    memmi_OpenProcess proc_opt = memmi_open_process(pid, memmi_default_allocator());
    ASSERT(proc_opt.status == MEMMI_OK);
    memmi_Process proc = proc_opt.process;

    memmi_Status suspend_result = memmi_suspend_process(proc);
    ASSERT(suspend_result == MEMMI_OK);

    memmi_Status resume_result = memmi_resume_process(proc);
    ASSERT(resume_result == MEMMI_OK);

    return 0;
    memmi_Status attach_res = memmi_attach_to_process(proc);
    ASSERT(attach_res == MEMMI_OK);

    memmi_Status detach_res = memmi_detach_from_process(proc);
    ASSERT(detach_res == MEMMI_OK);

    memmi_MemoryRegions regions = memmi_get_process_memory_regions(proc, memmi_default_allocator());
    ASSERT(regions.status == MEMMI_OK);

    size_t total_memory_size = 0;
    for (size_t i = 0; i < regions.count; ++i) {
        memmi_MemoryRegion region = regions.data[i];

        printf("%p: %lu (%x)\n", (void *)region.base_address, region.size, region.permissions);

        total_memory_size += region.size;
    }

    printf("\n\ntotal_memory_size: %zu\n", total_memory_size);

    memmi_ThreadList threads = memmi_get_process_threads(proc, memmi_default_allocator());
    ASSERT(threads.status == MEMMI_OK);

#if 0
    memmi_MemoryRegion first = regions.data[0];
    memmi_ReadMemory read_result = memmi_read_memory(proc, first.base_address, first.size, memmi_default_allocator());
    ASSERT(read_result.status == MEMMI_OK);

    memset(read_result.memory, 0, read_result.bytes_read);

    memmi_WriteMemory write_result = memmi_write_memory(proc, first.base_address, read_result.memory, read_result.bytes_read);
    ASSERT(write_result.status == MEMMI_OK);
#endif

    memmi_close_process(proc, memmi_default_allocator());


#else
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
#endif
}

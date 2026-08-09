#include "memmi.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#define ASSERT(e) assert(e)

int main(int argc, char **argv)
{

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

    memmi_OpenProcess proc_opt = memmi_open_process(pid);
    ASSERT(proc_opt.status == MEMMI_OK);
    memmi_Process proc = proc_opt.process;

    memmi_Status attach_res = memmi_attach_to_process(proc);
    ASSERT(attach_res == MEMMI_OK);


    memmi_Status suspend_result = memmi_suspend_process(proc);
    ASSERT(suspend_result == MEMMI_OK);

    memmi_ThreadList threads = memmi_get_process_threads(proc, memmi_default_allocator());
    ASSERT(threads.status == MEMMI_OK);

    memmi_Registers regs = memmi_get_thread_registers(threads.data[0]);
    ASSERT(regs.status == MEMMI_OK);

    /* while (true) { */
    /*     memmi_EventList events = memmi_wait_for_debug_events(proc, memmi_default_allocator()); */

    /*     for (memmi_DebugEvent *e = events.first; e; e = e->next) { */
    /*         printf("%p\n", e); */
    /*     } */

    /*     memmi_continue_after_debug_events(proc, events); */
    /* } */

    memmi_Status detach_res = memmi_detach_from_process(proc);
    ASSERT(detach_res == MEMMI_OK);

    memmi_MemoryRegions regions = memmi_get_process_memory_regions(proc, memmi_default_allocator());
    ASSERT(regions.status == MEMMI_OK);

    size_t total_memory_size = 0;
    for (size_t i = 0; i < regions.count; ++i) {
        memmi_MemoryRegion region = regions.data[i];

        printf("%p: %zu (%x)\n", (void *)region.base_address, region.size, region.permissions);

        total_memory_size += region.size;
    }

    printf("\n\ntotal_memory_size: %zu\n", total_memory_size);


#if 0
    memmi_MemoryRegion first = regions.data[0];
    memmi_ReadMemory read_result = memmi_read_memory(proc, first.base_address, first.size, memmi_default_allocator());
    ASSERT(read_result.status == MEMMI_OK);

    memset(read_result.memory, 0, read_result.bytes_read);

    memmi_WriteMemory write_result = memmi_write_memory(proc, first.base_address, read_result.memory, read_result.bytes_read);
    ASSERT(write_result.status == MEMMI_OK);
#endif

    memmi_close_process(proc);
}

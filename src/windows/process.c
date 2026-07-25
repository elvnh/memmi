#include "process.h"

#include "utils.h"

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    
    return (memmi_ProcessList){0};
}

memmi_OpenProcess memmi_open_process(memmi_PID pid, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    
    return (memmi_OpenProcess){0};
}

void memmi_close_process(memmi_Process process, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
}

memmi_ReadMemory memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_ReadMemory){0};
}

memmi_WriteMemory memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_WriteMemory){0};
}

memmi_MemoryRegions memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_MemoryRegions){0};
}

memmi_ThreadList memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_ThreadList){0};
}

memmi_Status memmi_attach_to_process(memmi_Process process)
{
    ASSERT(0 && "Unimplemented");
    return 0;
}

memmi_Status memmi_detach_from_process(memmi_Process process)
{
    ASSERT(0 && "Unimplemented");
    return 0;
}

memmi_Status memmi_resume_process(memmi_Process process)
{
    ASSERT(0 && "Unimplemented");
    return 0;
    
}

memmi_Status memmi_suspend_process(memmi_Process process)
{
    ASSERT(0 && "Unimplemented");
    return 0;
    
}

memmi_EventList memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_EventList){0};
    
}

memmi_Registers memmi_get_thread_registers(memmi_TID tid)
{
    ASSERT(0 && "Unimplemented");
    return (memmi_Registers){0};
    
}

memmi_Status memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value)
{
    ASSERT(0 && "Unimplemented");
    return 0;
}
#include "process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include "utils.h"

#include <stdlib.h>

// TODO: some structs are common between windows and linux
typedef struct {
    memmi_ProcessInfo *data;
    size_t count;
    size_t capacity;
} ProcessDynArray;

/***************************/
/* Common helper functions */
/***************************/
memmi_Status windows_error_to_memmi_status(DWORD error_code)
{
    memmi_Status result = 0;
    
    switch (error_code) {
        default: {
            ASSERT(0);
        } break;
    }
    
    return result;
}

/**********************/
/* API implementation */
/**********************/
// TODO: move to utils file
static memmi_String str_copy(memmi_String str, memmi_Allocator allocator)
{
    memmi_String result = {0};
    
    result.data = allocate(allocator, char, str.count);
    
    if (result.data) {
        memcpy(result.data, str.data, str.count);
        result.count = str.count;
    }
    
    return result;
}
static memmi_String get_process_name(DWORD pid, memmi_Allocator allocator)
{
    memmi_String result = {0};
    
    HANDLE proc_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    
    // If we can't open the process, that probably means that we don't have the permissions 
    // to do so due to not running as administrator.
    if (proc_handle) {
        char proc_name_buf[MAX_PATH];
        
        HMODULE module = 0;
        DWORD bytes_stored = 0;
        
        BOOL enum_modules_result = EnumProcessModules(
            proc_handle, &module, sizeof(module), &bytes_stored);
            
        if (enum_modules_result) {
            DWORD module_name_chars_written = GetModuleBaseNameA(
                proc_handle, module, proc_name_buf, ARRAY_COUNT(proc_name_buf));
                
            if (module_name_chars_written == 0) {
                ASSERT(0 && "TODO: how to handle?");
            } else {
                memmi_String proc_name = (memmi_String){proc_name_buf, module_name_chars_written};
                result = str_copy(proc_name, allocator);
            }
        }
        
    }
     
    CloseHandle(proc_handle);
     
    return result;
}

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator)
{
    memmi_ProcessList result = {0};
    
    ProcessDynArray procs = {0};
    
    DWORD pids_count = 1024;
    DWORD *pids = allocate(allocator, DWORD, pids_count);
    
    if (!pids) {
        result.status = MEMMI_ALLOCATION_FAILED;
    } else {    
        BOOL enum_procs_result = FALSE;
        bool done = false;
        
        while (!done) {
            DWORD pids_size_in_bytes = pids_count * sizeof(*pids);
            
            DWORD bytes_returned = 0;
            enum_procs_result = EnumProcesses(pids, pids_size_in_bytes, &bytes_returned);    
            
            if (!enum_procs_result) {
                result.status = windows_error_to_memmi_status(GetLastError());
                done = true;
            } else if (bytes_returned == pids_size_in_bytes) {
                // The array may have been too small to contain all pids,
                // retry again with a larger array.
                DWORD new_pids_count = pids_count * 2;
                DWORD *new_pids = reallocate(allocator, pids, pids_count, new_pids_count);
                
                if (!new_pids) {
                    result.status = MEMMI_ALLOCATION_FAILED;
                    done = true;
                } else {
                    pids_count = new_pids_count;
                    pids = new_pids;
                }
            } else {
                ASSERT(bytes_returned > 0);
            
                DWORD processes_returned = (DWORD)(bytes_returned / (DWORD)sizeof(*pids));
            
                for (uint32_t i = 0; i < processes_returned; ++i) {
                    memmi_String proc_name = get_process_name(pids[i], allocator);
                
                    if (proc_name.data) {
                        memmi_ProcessInfo proc_info = {0};
                        proc_info.name = proc_name;
                        proc_info.pid = (memmi_PID){pids[i]};
                    
                        // TODO: check that dyn_arr_push doesn't fail
                        dyn_arr_push(&procs, proc_info, allocator);
                    }
                }
                
                done = true;
            }
        }
    }

    result.data = procs.data;
    result.count = procs.count;
    
    deallocate(allocator, pids, pids_count);
    
    return result;
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
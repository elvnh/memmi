#include "process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>

#include <stdlib.h>

/***************************/
/* Common helper functions */
/***************************/
// TODO: use win32 prefix
static memmi_Status windows_error_to_memmi_status(DWORD error_code)
{
    memmi_Status result = zero_enum(memmi_Status);

    // TODO: fill out more of these errors
    switch (error_code) {
        case ERROR_NOACCESS: {
            result = MEMMI_INSUFFICIENT_PERMISSIONS;
        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}

static DWORD win32_get_native_pid(memmi_Process proc)
{
    DWORD result = (DWORD)proc.pid.value;
    
    return result;
}

static HANDLE win32_get_process_handle(memmi_Process proc)
{
    HANDLE result = (HANDLE)proc.data;
    
    return result;
}

typedef struct {
    HANDLE data;
    memmi_Status status;
} Win32Handle;

Win32Handle win32_open_thread_handle(DWORD tid)
{
    Win32Handle result = zero_struct(Win32Handle);

    HANDLE handle = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);

    if (!handle) {
        ASSERT(0);
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        result.data = handle;
    }

    return result;
}

typedef enum {
    FOR_EACH_THREAD_RES_CONTINUE,
    FOR_EACH_THREAD_RES_BREAK,
} ForEachThreadResult;

typedef ForEachThreadResult (*ForEachThreadFn)(void *user_data, DWORD tid);

static memmi_Status for_each_thread(DWORD pid, void *user_data, ForEachThreadFn callback)
{
    memmi_Status result = zero_enum(memmi_Status);

    HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (!handle) {
        result = windows_error_to_memmi_status(GetLastError());
    } else {
        THREADENTRY32 thread_entry = {0};
        thread_entry.dwSize = sizeof(thread_entry);

        if (!Thread32First(handle, &thread_entry)) {
            result = windows_error_to_memmi_status(GetLastError());
        } else {
            do {
                const size_t owner_pid_end_offset = offsetof(THREADENTRY32, th32OwnerProcessID)
                    + sizeof(thread_entry.th32OwnerProcessID);

                // According to https://devblogs.microsoft.com/oldnewthing/20060223-14/?p=32173
                // this check has to be performed.
                if (thread_entry.dwSize >= owner_pid_end_offset) {
                    // CreateToolhelp32Snapshot will enumerate all threads in the system,
                    // so we'll have to check that the thread actually belongs to the
                    // process provided by the user.
                    if (thread_entry.th32OwnerProcessID == pid) {
                        ForEachThreadResult cb_result = callback(user_data, thread_entry.th32ThreadID);

                        if (cb_result == FOR_EACH_THREAD_RES_BREAK) {
                            break;
                        }
                    }
                }

                thread_entry.dwSize = sizeof(thread_entry);
            } while (Thread32Next(handle, &thread_entry));

            // If we reached the end of the thread list, GetLastError will report
            // ERROR_NO_MORE_FILES. If we received some other error, something must
            // have gone wrong.
            DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) {
                result = windows_error_to_memmi_status(error);
            }
        }
    }

    CloseHandle(handle);

    return result;
}

static bool process_exists(DWORD pid)
{
    bool result = false;

    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    result = handle != 0;

    CloseHandle(handle);

    return result;
}

static memmi_RegisterValue win32_load_context_struct_register_value(CONTEXT *context, memmi_Register reg)
{
    memmi_RegisterValue result = 0;

    switch (reg) {
        #if MEMMI_X64
        case MEMMI_REG_RAX: { result = context->Rax; } break;                                
        case MEMMI_REG_RCX: { result = context->Rcx; } break;                                
        case MEMMI_REG_RDX: { result = context->Rdx; } break;                                
        case MEMMI_REG_RSI: { result = context->Rsi; } break;                                
        case MEMMI_REG_RDI: { result = context->Rdi; } break;                                
        case MEMMI_REG_RSP: { result = context->Rsp; } break;                                
        case MEMMI_REG_RBP: { result = context->Rbp; } break;                                
        case MEMMI_REG_RBX: { result = context->Rbx; } break;                                
        case MEMMI_REG_RIP: { result = context->Rip; } break;
        #elif MEMMI_X86
        case MEMMI_REG_EAX: { result = context->Eax; } break;                                
        case MEMMI_REG_ECX: { result = context->Ecx; } break;                                
        case MEMMI_REG_EDX: { result = context->Edx; } break;                                
        case MEMMI_REG_ESI: { result = context->Esi; } break;                                
        case MEMMI_REG_EDI: { result = context->Edi; } break;                                
        case MEMMI_REG_ESP: { result = context->Esp; } break;                                
        case MEMMI_REG_EBP: { result = context->Ebp; } break;                                
        case MEMMI_REG_EBX: { result = context->Ebx; } break;                                
        case MEMMI_REG_EIP: { result = context->Eip; } break;
        #endif
        
        #if MEMMI_X64
        case MEMMI_REG_R8: { result   = context->R8; } break;                  
        case MEMMI_REG_R9: { result   = context->R9; } break;                  
        case MEMMI_REG_R10: { result  = context->R10; } break;                 
        case MEMMI_REG_R11: { result  = context->R11; } break;                 
        case MEMMI_REG_R12: { result  = context->R12; } break;                 
        case MEMMI_REG_R13: { result  = context->R13; } break;                 
        case MEMMI_REG_R14: { result  = context->R14; } break;                 
        case MEMMI_REG_R15: { result  = context->R15; } break;
        #endif

        case MEMMI_REG_CS: { result = context->SegCs; } break;                  
        case MEMMI_REG_SS: { result = context->SegSs; } break;                  
        case MEMMI_REG_DS: { result = context->SegDs; } break;                  
        case MEMMI_REG_ES: { result = context->SegEs; } break;                  
        case MEMMI_REG_FS: { result = context->SegFs; } break;                  
        case MEMMI_REG_GS: { result = context->SegGs; } break;                  
        
        case MEMMI_REG_DR0: { result = context->Dr0; } break;                  
        case MEMMI_REG_DR1: { result = context->Dr1; } break;                  
        case MEMMI_REG_DR2: { result = context->Dr2; } break;                  
        case MEMMI_REG_DR3: { result = context->Dr3; } break;                  
        case MEMMI_REG_DR6: { result = context->Dr6; } break;                  
        case MEMMI_REG_DR7: { result = context->Dr7; } break;                  
       
        #if MEMMI_X64
        case MEMMI_REG_RFLAGS: { result = context->EFlags; } break;                         
        #elif MEMMI_X86
        case MEMMI_REG_EFLAGS: { result = context->EFlags; } break;                  
        #endif

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}

static void win32_set_context_struct_register_value(CONTEXT *context, memmi_Register reg, memmi_RegisterValue value)
{
    switch (reg) {
        #if MEMMI_X64
        case MEMMI_REG_RAX: { context->Rax = value; } break;                                
        case MEMMI_REG_RCX: { context->Rcx = value; } break;                                
        case MEMMI_REG_RDX: { context->Rdx = value; } break;                                
        case MEMMI_REG_RSI: { context->Rsi = value; } break;                                
        case MEMMI_REG_RDI: { context->Rdi = value; } break;                                
        case MEMMI_REG_RSP: { context->Rsp = value; } break;                                
        case MEMMI_REG_RBP: { context->Rbp = value; } break;                                
        case MEMMI_REG_RBX: { context->Rbx = value; } break;                                
        case MEMMI_REG_RIP: { context->Rip = value; } break;
        #elif MEMMI_X86
        case MEMMI_REG_EAX: { context->Eax = value; } break;                                
        case MEMMI_REG_ECX: { context->Ecx = value; } break;                                
        case MEMMI_REG_EDX: { context->Edx = value; } break;                                
        case MEMMI_REG_ESI: { context->Esi = value; } break;                                
        case MEMMI_REG_EDI: { context->Edi = value; } break;                                
        case MEMMI_REG_ESP: { context->Esp = value; } break;                                
        case MEMMI_REG_EBP: { context->Ebp = value; } break;                                
        case MEMMI_REG_EBX: { context->Ebx = value; } break;                                
        case MEMMI_REG_EIP: { context->Eip = value; } break;
        #endif
        
        #if MEMMI_X64
        case MEMMI_REG_R8:  { context->R8 = value; } break;                  
        case MEMMI_REG_R9:  { context->R9 = value; } break;                  
        case MEMMI_REG_R10: { context->R10 = value; } break;                 
        case MEMMI_REG_R11: { context->R11 = value; } break;                 
        case MEMMI_REG_R12: { context->R12 = value; } break;                 
        case MEMMI_REG_R13: { context->R13 = value; } break;                 
        case MEMMI_REG_R14: { context->R14 = value; } break;                 
        case MEMMI_REG_R15: { context->R15 = value; } break;
        #endif
        
        case MEMMI_REG_CS:  { context->SegCs = (WORD)value; } break;                  
        case MEMMI_REG_SS:  { context->SegSs = (WORD)value; } break;                  
        case MEMMI_REG_DS:  { context->SegDs = (WORD)value; } break;                  
        case MEMMI_REG_ES:  { context->SegEs = (WORD)value; } break;                  
        case MEMMI_REG_FS:  { context->SegFs = (WORD)value; } break;                  
        case MEMMI_REG_GS:  { context->SegGs = (WORD)value; } break;                  
        
        case MEMMI_REG_DR0: { context->Dr0 = value; } break;                  
        case MEMMI_REG_DR1: { context->Dr1 = value; } break;                  
        case MEMMI_REG_DR2: { context->Dr2 = value; } break;                  
        case MEMMI_REG_DR3: { context->Dr3 = value; } break;                  
        case MEMMI_REG_DR6: { context->Dr6 = value; } break;                  
        case MEMMI_REG_DR7: { context->Dr7 = value; } break;  

        #if MEMMI_X64
        case MEMMI_REG_RFLAGS: { context->EFlags = (DWORD)value; } break;                         
        #elif MEMMI_X86
        case MEMMI_REG_EFLAGS: { context->EFlags = (DWORD)value; } break;                  
        #endif

        default: {
            ASSERT(0);
        } break;
    }
}

typedef struct {
    memmi_Status status;
    CONTEXT data;
} Win32Context;

static Win32Context win32_get_thread_context(HANDLE handle)
{
    Win32Context result = zero_struct(Win32Context);

    CONTEXT context = zero_struct(CONTEXT);
    context.ContextFlags = CONTEXT_FULL;

    BOOL get_context_result = GetThreadContext(handle, &context);

    if (!get_context_result) {
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        result.data = context;
    }

    return result;
}

/**********************/
/* API implementation */
/**********************/
static memmi_String memmi_win32_get_module_name(HMODULE module, memmi_Allocator allocator)
{
    memmi_String result = zero_struct(memmi_String);

    DWORD module_name_chars_written = GetModuleBaseNameA(
        proc_handle, module, proc_name_buf, ARRAY_COUNT(proc_name_buf));

    ASSERT((module_name_chars_written > 0) && "TODO: how to handle this? Just ignore?");

    if (module_name_chars_written > 0) {
        memmi_String proc_name = {proc_name_buf, module_name_chars_written};
        result = str_copy(proc_name, allocator);
    }

    return result;
}

static memmi_String get_process_name(DWORD pid, memmi_Allocator allocator)
{
    memmi_String result = zero_struct(memmi_String);

    // TODO: is PROCESS_VM_READ really needed here?
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
            // TODO: use GetProcessImageFilenameA or QueryFullProcessImageName?
            result = memmi_win32_get_module_name(module, allocator);
        }
    }

    CloseHandle(proc_handle);

    return result;
}

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator)
{
    memmi_ProcessList result = zero_struct(memmi_ProcessList);

    ProcessDynArray procs = zero_struct(ProcessDynArray);

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
                        proc_info.pid.value = pids[i];

                        DynArray new_procs = dyn_arr_push(&procs, proc_info, allocator);
                        
                        if (!new_procs.data) {
                            result.status = MEMMI_ALLOCATION_FAILED;
                            break;
                        } else {
                            dyn_arr_assign(&procs, new_procs);
                        }
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

memmi_OpenProcess memmi_open_process(memmi_PID pid)
{
    memmi_OpenProcess result = zero_struct(memmi_OpenProcess);

    DWORD access =
    /*   PROCESS_QUERY_INFORMATION */
    /* | PROCESS_VM_READ */
    /* | PROCESS_VM_WRITE */
    /* | PROCESS_VM_OPERATION */
        PROCESS_ALL_ACCESS;

    HANDLE handle = OpenProcess(access, FALSE, (DWORD)pid.value);

    if (!handle) {
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        result.process.pid = pid;
        result.process.data = handle;
    }

    return result;
}

void memmi_close_process(memmi_Process process)
{
    // TODO: also detach from process just in case
    HANDLE handle = win32_get_process_handle(process);
    CloseHandle(handle);
}

memmi_ReadMemory memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator)
{
    memmi_ReadMemory result = zero_struct(memmi_ReadMemory);

    void *buffer = (void *)allocate(allocator, uint8_t, size);

    if (!buffer) {
        result.status = MEMMI_ALLOCATION_FAILED;
    } else {
        HANDLE handle = win32_get_process_handle(process);
        
        SIZE_T bytes_read = 0;
        BOOL read_memory_result = ReadProcessMemory(
            handle,
            (void *)address,
            buffer,
            size,
            &bytes_read
        );

        if (!read_memory_result) {
            result.status = windows_error_to_memmi_status(GetLastError());
        } else {
            if (bytes_read < size) {
                result.status = MEMMI_PARTIAL_READ_OR_WRITE;
            }

            result.memory = (char *)buffer;
            result.bytes_read = bytes_read;
        }
    }

    return result;
}

memmi_WriteMemory memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size)
{
    memmi_WriteMemory result = zero_struct(memmi_WriteMemory);

    HANDLE handle = win32_get_process_handle(process);
    SIZE_T bytes_written = 0;

    BOOL write_memory_result = WriteProcessMemory(
        handle,
        (void *)dst,
        src,
        src_size,
        &bytes_written
    );

    if (!write_memory_result) {
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        if (bytes_written < src_size) {
            result.status = MEMMI_PARTIAL_READ_OR_WRITE;
        }

        result.bytes_written = bytes_written;
    }

    return result;
}

static memmi_MemoryRegionPermission page_protection_to_memmi_permissions(DWORD protect)
{
    memmi_MemoryRegionPermission result = zero_enum(memmi_MemoryRegionPermission);

    // Mask out modifiers in the region protection.
    // TODO: should we handle any of these in a special way?
    DWORD protection_without_modifiers = protect & (DWORD)(~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE));
    
    switch (protection_without_modifiers) {
        case PAGE_EXECUTE: {
            result = MEMMI_REGION_PERMISSION_EXECUTE;
        } break;

        case PAGE_EXECUTE_READ: {
            set_flag(result, 
              MEMMI_REGION_PERMISSION_EXECUTE 
            | MEMMI_REGION_PERMISSION_READ);
        } break;

        case PAGE_EXECUTE_READWRITE: {
            set_flag(result,
              MEMMI_REGION_PERMISSION_EXECUTE
            | MEMMI_REGION_PERMISSION_READ
            | MEMMI_REGION_PERMISSION_WRITE);
        } break;

        case PAGE_EXECUTE_WRITECOPY: {
            set_flag(result,
              MEMMI_REGION_PERMISSION_EXECUTE
            | MEMMI_REGION_PERMISSION_WRITE);
        } break;

        case PAGE_READONLY: {
            result = MEMMI_REGION_PERMISSION_READ;
        }break;

        case PAGE_READWRITE: {
            set_flag(result,
              MEMMI_REGION_PERMISSION_READ
            | MEMMI_REGION_PERMISSION_WRITE);
        } break;

        case PAGE_WRITECOPY: {
            result = MEMMI_REGION_PERMISSION_WRITE;
        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}

memmi_MemoryRegions memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator)
{
    memmi_MemoryRegions result = zero_struct(memmi_MemoryRegions);
    RegionDynArray regions = zero_struct(RegionDynArray);

    HANDLE handle = win32_get_process_handle(process);

    // TODO: allow user to customize which regions they are interested in
    // by specifying which permissions the page can/must have
    // TODO: how to specify that user wants pages that are readable AND writable?

    size_t current_base_address = 0;
    bool done = false;

    while (!done) {
        MEMORY_BASIC_INFORMATION info = {0};

        size_t query_result = VirtualQueryEx(handle, (void *)current_base_address, &info, sizeof(info));

        if (query_result == 0) {
            // If there is no range of pages at the address we provided, we have reached
            // the end of the memory the process has mapped, and the function fails with
            // ERROR_INVALID_PARAMETER. If it failed with another error code, something
            // else has gone wrong.
            DWORD error = GetLastError();

            if (error != ERROR_INVALID_PARAMETER) {
                result.status = windows_error_to_memmi_status(error);
            }

            done = true;
        } else {
            ASSERT((size_t)info.BaseAddress == current_base_address);

            // We are only interested in pages that have actually been commited by
            // the process, not just reserved. Additionally, we'll skip pages which are
            // PAGE_NOACCESS as they can't be used in any way by the process.
            // TODO: make it so that Linux skips non-commited pages too if possible?
            if ((info.State == MEM_COMMIT) && (info.Protect != PAGE_NOACCESS)) {
                memmi_MemoryRegion region = zero_struct(memmi_MemoryRegion);
                region.base_address = (uintptr_t)info.BaseAddress;
                region.size = info.RegionSize;
                region.permissions = page_protection_to_memmi_permissions(info.Protect);

                // TODO: check that dyn_arr_push doesn't fail
                DynArray new_regions = dyn_arr_push(&regions, region, allocator);
                
                if (!new_regions.data) {
                    result.status = MEMMI_ALLOCATION_FAILED;
                    break;
                } else { 
                    dyn_arr_assign(&regions, new_regions);
                }
            }

            current_base_address += info.RegionSize;
        }
    }

    result.data = regions.data;
    result.count = regions.count;

    return result;
}

typedef struct {
    memmi_Status statuses;
    ThreadDynArray threads;
    memmi_Allocator allocator;
} CollectThreadsContext;

ForEachThreadResult collect_threads_cb(void *user_data, DWORD tid)
{
    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;
    
    CollectThreadsContext *context = (CollectThreadsContext *)user_data;

    memmi_TID thread = {tid};
    DynArray new_threads = dyn_arr_push(&context->threads, thread, context->allocator);
    
    if (!new_threads.data) {
        set_flag(context->statuses, MEMMI_ALLOCATION_FAILED);
        result = FOR_EACH_THREAD_RES_BREAK;
     } else { 
        dyn_arr_assign(&context->threads, new_threads);
     }
     
    return result;
}

memmi_ThreadList memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator)
{
    memmi_ThreadList result = zero_struct(memmi_ThreadList);

    DWORD pid = win32_get_native_pid(process);

    CollectThreadsContext cb_context = zero_struct(CollectThreadsContext);
    cb_context.allocator = allocator;

    memmi_Status for_each_thread_result = for_each_thread(pid, &cb_context, collect_threads_cb);
    set_flag(result.status, for_each_thread_result | cb_context.statuses);
    
    if (result.status == MEMMI_OK) {
        result.data = cb_context.threads.data;
        result.count = cb_context.threads.count;
    }

    return result;
}

memmi_Status memmi_attach_to_process(memmi_Process process)
{
    memmi_Status result = zero_enum(memmi_Status);

    DWORD pid = win32_get_native_pid(process);
    BOOL attach_result = DebugActiveProcess(pid);

    if (!attach_result) {
        result = windows_error_to_memmi_status(GetLastError());
    } else {
        // We probably don't want to kill the debuggee when we exit.
        // TODO: make this a parameter so the user can choose
        BOOL set_kill_on_exit_result = DebugSetProcessKillOnExit(FALSE);

        if (!set_kill_on_exit_result) {
            result = windows_error_to_memmi_status(GetLastError());
        }
    }

    return result;
}

memmi_Status memmi_detach_from_process(memmi_Process process)
{
    memmi_Status result = zero_enum(memmi_Status);
    
    DWORD pid = win32_get_native_pid(process);
    BOOL detach_result = DebugActiveProcessStop(pid);

    if (!detach_result) {
        result = windows_error_to_memmi_status(GetLastError());
    }

    return result;
}

typedef struct {
    memmi_Status statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, DWORD pid)
{
    ResumeThreadsContext *context = (ResumeThreadsContext *)user_data;

    Win32Handle handle = win32_open_thread_handle(pid);

    set_flag(context->statuses, handle.status);

    if (handle.status == MEMMI_OK) {
        DWORD prev_suspend_count = 0;

        do {
            prev_suspend_count = ResumeThread(handle.data);
        } while (prev_suspend_count > 0);

        if (prev_suspend_count == (DWORD)-1) {
            set_flag(context->statuses, windows_error_to_memmi_status(GetLastError()));
        }
    }

    CloseHandle(handle.data);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_resume_process(memmi_Process process)
{
    memmi_Status result = zero_enum(memmi_Status);

    DWORD pid = win32_get_native_pid(process);

    ResumeThreadsContext cb_context = zero_struct(ResumeThreadsContext);
    memmi_Status for_each_thread_result = for_each_thread(pid, &cb_context, resume_thread_cb);

    if (for_each_thread_result != MEMMI_OK) {
        result = for_each_thread_result;
    } else {
        result = cb_context.statuses;
    }

    return result;
}

static memmi_Status suspend_thread(DWORD tid)
{
    memmi_Status result = zero_enum(memmi_Status);

    Win32Handle handle = win32_open_thread_handle(tid);

    if (handle.status != MEMMI_OK) {
        result = handle.status;
    } else {
        DWORD prev_suspend_count = SuspendThread(handle.data);

        if (prev_suspend_count < 0) {
            result = windows_error_to_memmi_status(GetLastError());
        }
    }

    CloseHandle(handle.data);

    return result;
}

typedef struct {
    memmi_Status statuses;
    int32_t suspended_thread_count;
} SuspendThreadsContext;

static ForEachThreadResult suspend_thread_cb(void *user_data, DWORD tid)
{
    SuspendThreadsContext *context = (SuspendThreadsContext *)user_data;

    memmi_Status suspend_result = suspend_thread(tid);

    if (suspend_result == MEMMI_OK) {
        ++context->suspended_thread_count;
    }

    set_flag(context->statuses, suspend_result);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_suspend_process(memmi_Process process)
{
    // TODO: this function is very similar to the linux implementation
    memmi_Status result = zero_enum(memmi_Status);

    DWORD pid = win32_get_native_pid(process);

    int32_t last_suspended_thread_count = 0;
    SuspendThreadsContext cb_context = {0};
    bool suspended_thread_count_is_stable = false;

    while (!suspended_thread_count_is_stable && (result == MEMMI_OK)) {
        // Keep trying to suspend threads until the number of suspended threads in the process
        // has stabilized.
        cb_context.suspended_thread_count = 0;

        memmi_Status for_each_result = for_each_thread(pid, &cb_context, suspend_thread_cb);

        if (for_each_result != MEMMI_OK) {
            result = for_each_result;
        } else {
            if (cb_context.suspended_thread_count <= last_suspended_thread_count) {
                suspended_thread_count_is_stable = true;
            }

            last_suspended_thread_count = cb_context.suspended_thread_count;

            if (last_suspended_thread_count == 0) {
                result = cb_context.statuses;
                ASSERT(result != MEMMI_OK);
            } else {
                uint32_t statuses_excluding_no_such_process =
                    (uint32_t)((uint32_t)cb_context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS);

                result = (memmi_Status)statuses_excluding_no_such_process;
            }
        }
    }

    return result;

}

typedef struct {
    memmi_Status status;
    bool should_ignore;
    memmi_DebugEvent event;
} Win32EventResult;

// TODO: we may want to pass the event via pointer in case the struct is very large
static Win32EventResult win32_event_to_memmi_event(DEBUG_EVENT win32_event)
{
    Win32EventResult result = zero_struct(Win32EventResult);

    switch (win32_event.dwDebugEventCode) {
        case EXCEPTION_DEBUG_EVENT: {
            switch(win32_event.u.Exception.ExceptionRecord.ExceptionCode) {
                case EXCEPTION_ACCESS_VIOLATION: {
                    // segfault
                    ASSERT(0 && "Unimplemented");
                } break;

                case EXCEPTION_BREAKPOINT: {
                    result.event.kind = MEMMI_DEBUG_EVENT_BREAKPOINT;
                    
                    memmi_TID tid = {win32_event.dwThreadId};
                    
                    memmi_Registers regs = memmi_get_thread_registers(tid);
                    
                    int32_t breakpoint_index = get_dr6_breakpoint_index(regs.values[MEMMI_REG_DR6]);
 
                    if (regs.status != MEMMI_OK) {
                        result.status = regs.status;
                    } else if (breakpoint_index == -1) {
                        ASSERT(0 && "Should never happen");
                        result.status  = MEMMI_OTHER_ERROR;
                    } else {
                        memmi_Register ip_register = 
                            #if MEMMI_X64
                                MEMMI_REG_RIP
                            #elif MEMMI_X86
                                MEMMI_REG_EIP
                            #endif
                        ;
                        result.event.as.breakpoint.breakpoint_index = (uint32_t)breakpoint_index;
                        result.event.as.breakpoint.ip_register = regs.values[ip_register];
                    }
                } break;

                case EXCEPTION_DATATYPE_MISALIGNMENT: {
                    // TODO: is there something similar for Linux?
                    ASSERT(0 && "Unimplemented");
                } break;

                case EXCEPTION_SINGLE_STEP: {
                    // TODO: how to report?
                    ASSERT(0 && "Unimplemented");
                } break;

                case DBG_CONTROL_C: {
                    result.event.kind = MEMMI_DEBUG_EVENT_THREAD_STOPPED;
                } break;

                default: {
                    ASSERT(0);
                    result.should_ignore = true;
                } break;
            }
        } break;

        case CREATE_THREAD_DEBUG_EVENT: {
            // created thread
            // TODO: should we close the handles we receive?
            result.event.kind = MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED;

            HANDLE thread_handle = win32_event.u.CreateThread.hThread;
            ASSERT(thread_handle);
            DWORD tid = GetThreadId(thread_handle);
            ASSERT(tid != 0);

            result.event.as.new_thread.id.value = (int64_t)tid;

            CloseHandle(thread_handle);
        } break;

        case CREATE_PROCESS_DEBUG_EVENT: {
            // is this only created when first attaching? do we even need to handle it?
            result.should_ignore = true;
        } break;

        case EXIT_THREAD_DEBUG_EVENT: {
            result.event.kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
            result.event.as.exit_code = (int)win32_event.u.ExitThread.dwExitCode;
        } break;

        case EXIT_PROCESS_DEBUG_EVENT: {
            result.event.kind = MEMMI_DEBUG_EVENT_PROCESS_EXITED;
            result.event.as.exit_code = (int)win32_event.u.ExitProcess.dwExitCode;
        } break;

        // TODO: Investigate if we can implement SO loading events for Linux. If so,
        // make these part of the librarys event types.
        case LOAD_DLL_DEBUG_EVENT: {
            result.should_ignore = true;
        } break;

        case UNLOAD_DLL_DEBUG_EVENT: {
            result.should_ignore = true;
        } break;

        case OUTPUT_DEBUG_STRING_EVENT: {
            result.should_ignore = true;
        } break;

        case RIP_EVENT: {
            // ???
            result.should_ignore = true;
        } break;

        default: {
            ASSERT(0);
            result.should_ignore = true;
        } break;
    }

    return result;
}

memmi_EventList memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator)
{
    // TODO: get_native_pid helper function
    // TODO: what is the EXCEPTION_BREAKPOINT being triggered?
    // TODO: allow timeouts

    memmi_EventList result = zero_struct(memmi_EventList);

    DWORD pid = win32_get_native_pid(process);

    if (!process_exists(pid)) {
        result.status = MEMMI_NO_SUCH_PROCESS;
    } else {
        DEBUG_EVENT win32_event = zero_struct(DEBUG_EVENT);
        BOOL wait_for_event_result = WaitForDebugEvent(&win32_event, INFINITE);

        if (!wait_for_event_result) {
            result.status = windows_error_to_memmi_status(GetLastError());
        } else {
            // We're only interested in this event if the thread that caused it
            // belongs to the traced process.
            if (pid == win32_event.dwProcessId) {
                result.id_of_affected_thread.value = win32_event.dwThreadId;

                Win32EventResult event_result = win32_event_to_memmi_event(win32_event);

                if (event_result.status != MEMMI_OK) {
                    result.status = event_result.status;
                } else if (!event_result.should_ignore) {
                    memmi_DebugEvent *event_node = allocate(allocator, memmi_DebugEvent, 1);
                    *event_node = event_result.event;
                    
                    sl_push_back(&result, event_node);
                }
            } else {
                ASSERT(0 && "Can this happen?");
            }
        }
    }

    return result;
}

memmi_Status memmi_continue_after_debug_events(memmi_Process process, memmi_EventList events)
{
    memmi_Status result = zero_enum(memmi_Status);

    DWORD pid = win32_get_native_pid(process);
    BOOL continue_result = ContinueDebugEvent(
        pid, (DWORD)events.id_of_affected_thread.value, DBG_CONTINUE);

    if (!continue_result) {
        result = windows_error_to_memmi_status(GetLastError());
        ASSERT(0);
    }

    return result;
}

memmi_Registers memmi_get_thread_registers(memmi_TID tid)
{
    memmi_Registers result = zero_struct(memmi_Registers);

    DWORD native_tid = (DWORD)tid.value;
    Win32Handle handle = win32_open_thread_handle(native_tid);

    if (handle.status != MEMMI_OK) {
        result.status = handle.status;
    } else {
        Win32Context context = win32_get_thread_context(handle.data);

        if (context.status != MEMMI_OK) {
            result.status = windows_error_to_memmi_status(GetLastError());
        } else {
            for (memmi_Register reg = zero_enum(memmi_Register); reg < MEMMI_REG_COUNT; inc_enum(reg)) {
                result.values[reg] = win32_load_context_struct_register_value(&context.data, reg);
            }
        }
    }
    
    CloseHandle(handle.data);

    return result;
}

memmi_Status memmi_set_thread_register(memmi_TID tid, memmi_Register reg, memmi_RegisterValue value)
{
    memmi_Status result = zero_enum(memmi_Status);

    DWORD native_tid = (DWORD)tid.value;

    Win32Handle handle = win32_open_thread_handle(native_tid);

    if (handle.status != MEMMI_OK) {
        result = handle.status;
    } else {
        Win32Context context = win32_get_thread_context(handle.data);

        if (context.status != MEMMI_OK) {
            result = windows_error_to_memmi_status(GetLastError());
        } else {
            win32_set_context_struct_register_value(&context.data, reg, value);

            // TODO: "A 64-bit application can set the context of a WOW64 thread using the Wow64SetThreadContext function."

            BOOL set_context_result = SetThreadContext(handle.data, &context.data);

            if (set_context_result != MEMMI_OK) {
                result = windows_error_to_memmi_status(GetLastError());
            }
        }
    }

    CloseHandle(handle.data);

    return result;
}

typedef struct {
    uintptr_t address;
    uint32_t index;
    memmi_BreakpointCondition cond;
    memmi_BreakpointLength length;
    memmi_Status statuses;
} SetBreakpointContext;

static ForEachThreadResult set_hardware_breakpoint_on_thread_cb(void *user_data, DWORD tid)
{
    memmi_Status status = zero_enum(memmi_Status);

    SetBreakpointContext *cb_context = (SetBreakpointContext *)user_data;

    Win32Handle handle = win32_open_thread_handle(tid);
    Win32Context context = win32_get_thread_context(handle.data);

    if (context.status != MEMMI_OK) {
        status = windows_error_to_memmi_status(GetLastError());
    } else {
        uintptr_t address = cb_context->address;
        uint32_t index = cb_context->index;
        memmi_BreakpointCondition cond = cb_context->cond;
        memmi_BreakpointLength length = cb_context->length;

        memmi_Register debug_reg = debug_register_from_index(index);

        memmi_RegisterValue old_dr7_value = win32_load_context_struct_register_value(&context.data, MEMMI_REG_DR7);
        memmi_RegisterValue new_dr7_value = dr7_set_breakpoint_value(old_dr7_value, index, cond, length);

        win32_set_context_struct_register_value(&context.data, debug_reg, address);
        win32_set_context_struct_register_value(&context.data, MEMMI_REG_DR7, new_dr7_value);

        BOOL set_context_result = SetThreadContext(handle.data, &context.data);

        if (set_context_result != MEMMI_OK) {
            status = windows_error_to_memmi_status(GetLastError());
        }
    }

    set_flag(cb_context->statuses, status);

    CloseHandle(handle.data);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
    memmi_BreakpointCondition condition, uint32_t index, memmi_BreakpointLength length)
{
    memmi_Status result = zero_enum(memmi_Status);

    DWORD pid = win32_get_native_pid(process);

    SetBreakpointContext cb_context = zero_struct(SetBreakpointContext);
    cb_context.address = address;
    cb_context.index = index;
    cb_context.cond = condition;
    cb_context.length = length;

    memmi_Status for_each_thread_result = for_each_thread(pid, &cb_context, set_hardware_breakpoint_on_thread_cb);
    set_flag(result, for_each_thread_result | cb_context.statuses);
    
    return result;
}

memmi_ObjectList memmi_get_loaded_objects(memmi_Process proc, memmi_Allocator allocator)
{
    memmi_ObjectList result = zero_struct(memmi_ObjectList);
    memmi_ObjectDynArray objects = zero_struct(memmi_ObjectDynArray);

    HANDLE handle = win32_get_process_handle(proc);

    DWORD module_count = 512;
    HMODULE *modules = allocate(allocator, HMODULE, module_count);

    if (!modules) {
        result.status = MEMMI_ALLOCATION_FAILED;
    } else {
        DWORD bytes_needed = 0;
        BOOL enum_modules_result = false;

        do {
            enum_modules_result = EnumProcessModulesEx(
                handle,
                modules,
                modules_size * sizeof(*modules),
                &bytes_needed,
                LIST_MODULES_ALL
            );

            size_t modules_size_in_bytes = modules_count * sizeof(*modules);

            if (!enum_modules_result) {
                result = win32_error_to_memmi_status(GetLastError());
            } else if (bytes_needed > modules_size_in_bytes) {
                // The array was too small, try again.
                DWORD new_module_count = bytes_needed / sizeof(*modules);
                HMODULE *new_modules = reallocate(allocator, modules, module_count, new_module_count);

                if (!new_modules) {
                    result.status = MEMMI_ALLOCATION_FAILED;
                } else {
                    module_count = new_module_count;
                    modules = new_modules;
                }
            } else {
                for (size_t i = 0; i < modules_count; ++i) {
                    MODULEINFO module_info = zero_struct(MODULEINFO);

                    if (GetModuleInformation(handle, modules[i], &module_info, sizeof(module_info))) {
                        memmi_Object object = zero_struct(memmi_Object);
                        object.path = memmi_win32_get_module_name(modules[i], allocator);

                        if (object.path.data) {
                            ASSERT(module_info.lpBaseOfDll == (DWORD)modules[i]);
                            object.base_address = (uintptr_t)module_info.lpBaseOfDll;
                            object.size = module_info.SizeOfImage;

                            if (objects.count == 0) {
                                // The executable itself is always the first module.
                                object.kind = MEMMI_OBJECT_EXECUTABLE;
                            } else {
                                object.kind = MEMMI_OBJECT_DYNAMIC_LIBRARY;
                            }
                        }

                        DynArray new_objects = dyn_arr_push(&objects, object);

                        if (!new_objects.data) {
                            result.status = MEMMI_ALLOCATION_FAILED;
                        } else {
                            dyn_arr_assign(&objects, new_objects);
                        }
                    }
                }
            }
        } while ((result.status == MEMMI_OK) && enum_modules_result && (bytes_needed > modules_size_in_bytes));
    }

    result.data = objects.data;
    result.count = objects.count;

    return result;
}

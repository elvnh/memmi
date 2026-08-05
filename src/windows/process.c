#include "process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "utils.h"

#include <stdlib.h>

// TODO: some structs are common between windows and linux
typedef struct {
    memmi_ProcessInfo *data;
    size_t count;
    size_t capacity;
} ProcessDynArray;

typedef struct {
    memmi_MemoryRegion *data;
    size_t count;
    size_t capacity;
} MemoryRegionDynArray;

typedef struct {
    memmi_TID *data;
    size_t count;
    size_t capacity;
} ThreadDynArray;

// TODO: just store PID directly in memmi_Process, and use data field for win32 handle
typedef struct {
    HANDLE handle;
    memmi_PID pid;
} memmi_ProcessImpl;

/***************************/
/* Common helper functions */
/***************************/
static memmi_Status windows_error_to_memmi_status(DWORD error_code)
{
    memmi_Status result = 0;

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

static memmi_ProcessImpl *get_platform_process_handle(memmi_Process process)
{
    memmi_ProcessImpl *result = process.data;

    return result;
}

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

typedef enum {
    FOR_EACH_THREAD_RES_CONTINUE,
    FOR_EACH_THREAD_RES_BREAK,
} ForEachThreadResult;

typedef ForEachThreadResult (*ForEachThreadFn)(void *user_data, DWORD tid);

static memmi_Status for_each_thread(DWORD pid, void *user_data, ForEachThreadFn callback)
{
    memmi_Status result = 0;

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

/**********************/
/* API implementation */
/**********************/
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
            // TODO: use GetProcessImageFilenameA or QueryFullProcessImageName?
            DWORD module_name_chars_written = GetModuleBaseNameA(
                proc_handle, module, proc_name_buf, ARRAY_COUNT(proc_name_buf));

            ASSERT((module_name_chars_written > 0) && "TODO: how to handle this? Just ignore?");

            if (module_name_chars_written > 0) {
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
    memmi_OpenProcess result = {0};

    DWORD access =
      PROCESS_QUERY_INFORMATION
    | PROCESS_VM_READ
    | PROCESS_VM_WRITE
    | PROCESS_VM_OPERATION;

    HANDLE handle = OpenProcess(access, FALSE, (DWORD)pid.value);

    if (!handle) {
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        memmi_ProcessImpl *process = allocate(allocator, memmi_ProcessImpl, 1);

        if (!process) {
            result.status = MEMMI_ALLOCATION_FAILED;
        } else {
            process->pid = pid;
            process->handle = handle;

            result.process.data = process;
        }
    }

    return result;
}

void memmi_close_process(memmi_Process process, memmi_Allocator allocator)
{
    memmi_ProcessImpl *impl = get_platform_process_handle(process);
    CloseHandle(impl->handle);

    deallocate(allocator, impl, 1);
}

memmi_ReadMemory memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator)
{
    memmi_ReadMemory result = {0};

    memmi_ProcessImpl *impl = get_platform_process_handle(process);

    void *buffer = (void *)allocate(allocator, uint8_t, size);

    if (!buffer) {
        result.status = MEMMI_ALLOCATION_FAILED;
    } else {
        SIZE_T bytes_read = 0;
        BOOL read_memory_result = ReadProcessMemory(
            impl->handle,
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

            result.memory = buffer;
            result.bytes_read = bytes_read;
        }
    }

    return result;
}

memmi_WriteMemory memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size)
{
    memmi_WriteMemory result = {0};

    memmi_ProcessImpl *impl = get_platform_process_handle(process);

    SIZE_T bytes_written = 0;

    BOOL write_memory_result = WriteProcessMemory(
        impl->handle,
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
    memmi_MemoryRegionPermission result = 0;

    // Mask out modifiers in the region protection.
    // TODO: should we handle any of these in a special way?
    DWORD protection_without_modifiers = protect
        & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);

    switch (protection_without_modifiers) {
        case PAGE_EXECUTE: {
            result = MEMMI_REGION_PERMISSION_EXECUTE;
        } break;

        case PAGE_EXECUTE_READ: {
            result =
              MEMMI_REGION_PERMISSION_EXECUTE
            | MEMMI_REGION_PERMISSION_READ;
        } break;

        case PAGE_EXECUTE_READWRITE: {
            result =
              MEMMI_REGION_PERMISSION_EXECUTE
            | MEMMI_REGION_PERMISSION_READ
            | MEMMI_REGION_PERMISSION_WRITE;
        } break;

        case PAGE_EXECUTE_WRITECOPY: {
            result =
              MEMMI_REGION_PERMISSION_EXECUTE
            | MEMMI_REGION_PERMISSION_WRITE;
        } break;

        case PAGE_READONLY: {
            result = MEMMI_REGION_PERMISSION_READ;
        }break;

        case PAGE_READWRITE: {
            result =
              MEMMI_REGION_PERMISSION_READ
            | MEMMI_REGION_PERMISSION_WRITE;
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
    MemoryRegionDynArray regions = {0};
    memmi_MemoryRegions result = {0};

    memmi_ProcessImpl *impl = get_platform_process_handle(process);

    // TODO: allow user to customize which regions they are interested in
    // by specifying which permissions the page can/must have
    // TODO: how to specify that user wants pages that are readable AND writable?

    size_t current_base_address = 0;
    bool done = false;

    while (!done) {
        MEMORY_BASIC_INFORMATION info = {0};

        size_t query_result = VirtualQueryEx(impl->handle, (void *)current_base_address, &info, sizeof(info));

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
                memmi_MemoryRegion region = {0};
                region.base_address = (uintptr_t)info.BaseAddress;
                region.size = info.RegionSize;
                region.permissions = page_protection_to_memmi_permissions(info.Protect);

                // TODO: check that dyn_arr_push doesn't fail
                dyn_arr_push(&regions, region, allocator);
            }

            current_base_address += info.RegionSize;
        }
    }

    result.data = regions.data;
    result.count = regions.count;

    return result;
}

typedef struct {
    ThreadDynArray threads;
    memmi_Allocator allocator;
} CollectThreadsContext;

ForEachThreadResult collect_threads_cb(void *user_data, DWORD tid)
{
    CollectThreadsContext *context = user_data;

    memmi_TID thread = {tid};
    dyn_arr_push(&context->threads, thread, context->allocator);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_ThreadList memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator)
{
    memmi_ThreadList result = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;

    CollectThreadsContext cb_context = {0};
    cb_context.allocator = allocator;

    result.status = for_each_thread((DWORD)pid.value, &cb_context, collect_threads_cb);

    if (result.status == MEMMI_OK) {
        result.data = cb_context.threads.data;
        result.count = cb_context.threads.count;
    }

    return result;
}

memmi_Status memmi_attach_to_process(memmi_Process process)
{
    memmi_Status result = 0;

    memmi_ProcessImpl *impl = get_platform_process_handle(process);
    BOOL attach_result = DebugActiveProcess((DWORD)impl->pid.value);

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
    memmi_Status result = 0;

    memmi_ProcessImpl *impl = get_platform_process_handle(process);
    BOOL detach_result = DebugActiveProcessStop((DWORD)impl->pid.value);

    if (!detach_result) {
        result = windows_error_to_memmi_status(GetLastError());
    }

    return result;
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

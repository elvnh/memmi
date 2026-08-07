#include "process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>

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
// TODO: use win32 prefix
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

typedef struct {
    HANDLE data;
    memmi_Status status;
} Win32Handle;

Win32Handle get_thread_handle(DWORD tid)
{
    Win32Handle result = {0};

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

static bool process_exists(DWORD pid)
{
    bool result = false;

    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    result = handle != 0;

    CloseHandle(handle);

    return result;
}

/**********************/
/* API implementation */
/**********************/
static memmi_String get_process_name(DWORD pid, memmi_Allocator allocator)
{
    memmi_String result = {0};

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
    /*   PROCESS_QUERY_INFORMATION */
    /* | PROCESS_VM_READ */
    /* | PROCESS_VM_WRITE */
    /* | PROCESS_VM_OPERATION */
        PROCESS_ALL_ACCESS;

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

typedef struct {
    memmi_Status statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, DWORD pid)
{
    ResumeThreadsContext *context = user_data;

    Win32Handle handle = get_thread_handle(pid);

    context->statuses |= handle.status;

    if (handle.status == MEMMI_OK) {
        DWORD prev_suspend_count = 0;

        do {
            prev_suspend_count = ResumeThread(handle.data);
        } while (prev_suspend_count > 0);

        if (prev_suspend_count == -1) {
            context->statuses |= windows_error_to_memmi_status(GetLastError());
        }
    }

    CloseHandle(handle.data);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_resume_process(memmi_Process process)
{
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    DWORD native_pid = (DWORD)pid.value;

    ResumeThreadsContext cb_context = {0};
    memmi_Status for_each_thread_result = for_each_thread(native_pid, &cb_context, resume_thread_cb);

    if (for_each_thread_result != MEMMI_OK) {
        result = for_each_thread_result;
    } else {
        result = cb_context.statuses;
    }

    return result;
}

static memmi_Status suspend_thread(DWORD tid)
{
    memmi_Status result = 0;

    Win32Handle handle = get_thread_handle(tid);

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
    SuspendThreadsContext *context = user_data;

    memmi_Status suspend_result = suspend_thread(tid);

    if (suspend_result == MEMMI_OK) {
        ++context->suspended_thread_count;
    }

    context->statuses |= suspend_result;

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_suspend_process(memmi_Process process)
{
    // TODO: this function is very similar to the linux implementation
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    DWORD native_pid = (DWORD)pid.value;

    int32_t last_suspended_thread_count = 0;
    SuspendThreadsContext cb_context = {0};
    bool suspended_thread_count_is_stable = false;

    while (!suspended_thread_count_is_stable && (result == MEMMI_OK)) {
        // Keep trying to suspend threads until the number of suspended threads in the process
        // has stabilized.
        cb_context.suspended_thread_count = 0;

        memmi_Status for_each_result = for_each_thread(native_pid, &cb_context, suspend_thread_cb);

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
                    cb_context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS;

                result = statuses_excluding_no_such_process;
            }
        }
    }

    return result;

}

// TODO: we may want to pass the event via pointer in case the struct is very large
static memmi_DebugEvent *win32_event_to_memmi_event(DEBUG_EVENT win32_event, memmi_Allocator allocator)
{
    // TODO: make allocate macro zero initialize. Don't put that functionality in
    // the allocator function itself.

    memmi_DebugEvent *result = allocate(allocator, memmi_DebugEvent, 1);

    *result = (memmi_DebugEvent){0};

    bool should_ignore = false;

    switch (win32_event.dwDebugEventCode) {
        case EXCEPTION_DEBUG_EVENT: {
            switch(win32_event.u.Exception.ExceptionRecord.ExceptionCode) {
                case EXCEPTION_ACCESS_VIOLATION: {
                    // segfault
                    ASSERT(0 && "Unimplemented");
                } break;

                case EXCEPTION_BREAKPOINT: {
                    // breakpoint, report PC register
                    ASSERT(0 && "TODO: breakpoint");
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
                    // TODO: how to report?
                    ASSERT(0 && "Unimplemented");
                } break;

                default: {
                    ASSERT(0);
                    should_ignore = true;
                } break;
            }
        } break;

        case CREATE_THREAD_DEBUG_EVENT: {
            // created thread
            // TODO: should we close the handles we receive?
            result->kind = MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED;

            HANDLE thread_handle = win32_event.u.CreateThread.hThread;
            ASSERT(thread_handle);
            DWORD tid = GetThreadId(thread_handle);
            ASSERT(tid != 0);

            result->as.new_thread.id = (memmi_TID){(uint64_t)tid};

            CloseHandle(thread_handle);
        } break;

        case CREATE_PROCESS_DEBUG_EVENT: {
            // is this only created when first attaching? do we even need to handle it?
            should_ignore = true;
        } break;

        case EXIT_THREAD_DEBUG_EVENT: {
            // thread exit
            result->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
            result->as.thread_exited.exit_code = win32_event.u.ExitThread.dwExitCode;
        } break;

        case EXIT_PROCESS_DEBUG_EVENT: {
            // process exit
            ASSERT(0 && "TODO: we'll need to extend the public API to also include PROCESS_EXIT_PROCESS_DEBUG_EVENT");
        } break;

        // TODO: Investigate if we can implement SO loading events for Linux. If so,
        // make these part of the librarys event types.
        case LOAD_DLL_DEBUG_EVENT: {
            should_ignore = true;
        } break;

        case UNLOAD_DLL_DEBUG_EVENT: {
            should_ignore = true;
        } break;

        case OUTPUT_DEBUG_STRING_EVENT: {
            should_ignore = true;
        } break;

        case RIP_EVENT: {
            // ???
            should_ignore = true;
        } break;

        default: {
            ASSERT(0);
            should_ignore = true;
        } break;
    }

    if (should_ignore) {
        deallocate(allocator, result, 1);
        result = 0;
    }

    return result;
}

memmi_EventList memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator)
{
    // TODO: get_native_pid helper function
    // TODO: what is the EXCEPTION_BREAKPOINT being triggered?
    // TODO: allow timeouts

    memmi_EventList result = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;
    DWORD native_pid = (DWORD)pid.value;

    if (!process_exists(native_pid)) {
        result.status = MEMMI_NO_SUCH_PROCESS;
    } else {
        DEBUG_EVENT win32_event = {0};
        BOOL wait_for_event_result = WaitForDebugEvent(&win32_event, INFINITE);

        if (!wait_for_event_result) {
            result.status = windows_error_to_memmi_status(GetLastError());
        } else {
            // We're only interested in this event if the thread that caused it
            // belongs to the traced process.
            if (native_pid == win32_event.dwProcessId) {
                result.id_of_affected_thread = (memmi_TID){win32_event.dwThreadId};

                memmi_DebugEvent *event = win32_event_to_memmi_event(win32_event, allocator);

                if (event) {
                    sl_push_back(&result, event);
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
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    BOOL continue_result = ContinueDebugEvent(
        (DWORD)pid.value, (DWORD)events.id_of_affected_thread.value, DBG_CONTINUE);

    if (!continue_result) {
        result = windows_error_to_memmi_status(GetLastError());
        ASSERT(0);
    }

    return result;
}

typedef struct {
    void *address;
    size_t size;
} Win32ContextMember;

static Win32ContextMember win32_context_member_ptr_from_register(CONTEXT *context, memmi_Register reg)
{
    #define CONTEXT_MEMBER(memmi_name, member_name)         \
        case memmi_name: {                                  \
            result.address = &context->member_name;         \
            result.size = sizeof(context->member_name);     \
        } break

    Win32ContextMember result = {0};

    switch (reg) {
        CONTEXT_MEMBER(MEMMI_REG_RAX, Rax);
        CONTEXT_MEMBER(MEMMI_REG_RCX, Rcx);
        CONTEXT_MEMBER(MEMMI_REG_RDX, Rdx);
        CONTEXT_MEMBER(MEMMI_REG_RSI, Rsi);
        CONTEXT_MEMBER(MEMMI_REG_RDI, Rdi);
        CONTEXT_MEMBER(MEMMI_REG_RSP, Rsp);
        CONTEXT_MEMBER(MEMMI_REG_RBP, Rbp);
        CONTEXT_MEMBER(MEMMI_REG_RBX, Rbx);

        CONTEXT_MEMBER(MEMMI_REG_R8, R8);
        CONTEXT_MEMBER(MEMMI_REG_R9, R9);
        CONTEXT_MEMBER(MEMMI_REG_R10, R10);
        CONTEXT_MEMBER(MEMMI_REG_R11, R11);
        CONTEXT_MEMBER(MEMMI_REG_R12, R12);
        CONTEXT_MEMBER(MEMMI_REG_R13, R13);
        CONTEXT_MEMBER(MEMMI_REG_R14, R14);
        CONTEXT_MEMBER(MEMMI_REG_R15, R15);

        CONTEXT_MEMBER(MEMMI_REG_RIP, Rip);

        CONTEXT_MEMBER(MEMMI_REG_CS, SegCs);
        CONTEXT_MEMBER(MEMMI_REG_EFLAGS, EFlags);

        CONTEXT_MEMBER(MEMMI_REG_SS, SegSs);
        CONTEXT_MEMBER(MEMMI_REG_DS, SegDs);
        CONTEXT_MEMBER(MEMMI_REG_ES, SegEs);
        CONTEXT_MEMBER(MEMMI_REG_FS, SegFs);
        CONTEXT_MEMBER(MEMMI_REG_GS, SegGs);

        case MEMMI_REG_FS_BASE: {

        } break;

        case MEMMI_REG_GS_BASE: {

        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;

    #undef CONTEXT_MEMBER
}

typedef struct {
    memmi_Status status;
    CONTEXT data;
} Win32Context;

Win32Context win32_get_thread_context(HANDLE handle)
{
    Win32Context result = {0};

    CONTEXT context = {0};
    context.ContextFlags = CONTEXT_FULL;

    BOOL get_context_result = GetThreadContext(handle, &context);

    if (!get_context_result) {
        result.status = windows_error_to_memmi_status(GetLastError());
    } else {
        result.data = context;
    }

    return result;
}

memmi_Registers memmi_get_thread_registers(memmi_TID tid)
{
    memmi_Registers result = {0};

    DWORD native_tid = (DWORD)tid.value;
    Win32Handle handle = get_thread_handle(native_tid);

    if (handle.status != MEMMI_OK) {
        result.status = handle.status;
    } else {
        Win32Context context = win32_get_thread_context(handle.data);

        BOOL get_context_result = GetThreadContext(handle.data, &context.data);

        if (!get_context_result) {
            result.status = windows_error_to_memmi_status(GetLastError());
        } else {
            for (memmi_Register reg = 0; reg < MEMMI_REG_COUNT; ++reg) {
                Win32ContextMember member = win32_context_member_ptr_from_register(&context.data, reg);

                memcpy(&result.values[reg], member.address, member.size);
            }
        }
    }

    return result;

}

memmi_Status memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value)
{
    ASSERT(0 && "Unimplemented");
    return 0;
}

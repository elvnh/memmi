#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include "process.h"

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>

#include "utils.h"

typedef struct {
    memmi_String value;
    bool ok;
} ProcessName;

typedef struct {
    memmi_ProcessInfo *data;
    size_t count;
    size_t capacity;
} ProcessDynArray;

typedef struct {
    memmi_MemoryRegion *data;
    size_t count;
    size_t capacity;
} RegionDynArray;

typedef struct {
    memmi_TID *data;
    size_t count;
    size_t capacity;
} ThreadDynArray;

typedef struct {
    memmi_PID pid;
} memmi_ProcessImpl;

static ProcessName get_process_name(int proc_dir_fd, memmi_Allocator allocator)
{
    ProcessName result = {0};

    size_t name_buffer_size = PATH_MAX + 1;
    char *name_buffer = allocate(allocator, char, name_buffer_size);

    ssize_t bytes_written = readlinkat(proc_dir_fd, "exe", name_buffer, name_buffer_size);

    // Continue for as long as the name was truncated, trying a bigger buffer each time
    while ((bytes_written > 0) && ((size_t)bytes_written == name_buffer_size)) {
        size_t new_buf_size = name_buffer_size * 2;
        name_buffer = reallocate(allocator, name_buffer, name_buffer_size, new_buf_size);
        name_buffer_size = new_buf_size;

        bytes_written = readlinkat(proc_dir_fd, "exe", name_buffer, name_buffer_size);
    }

    ASSERT(bytes_written < 0 || bytes_written > 0);

    if (bytes_written <= 0) {
        // Either the process has died or is a kernel process, either way ignore it
        deallocate(allocator, name_buffer, name_buffer_size);
    } else {
        result.value = (memmi_String) {name_buffer, (size_t)bytes_written};
        result.value.data[bytes_written] = '\0';

        result.ok = true;
    }

    return result;
}

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator)
{
    ProcessDynArray processes = {0};

    // TODO: use get_process_directory_fd
    DIR *proc_dir = opendir("/proc");
    int proc_dir_fd = dirfd(proc_dir); // Automatically closed by closedir

    if (proc_dir) {
        struct dirent *subdir_entry = 0;

        while ((subdir_entry = readdir(proc_dir))) {
            int subdir_fd = openat(proc_dir_fd, subdir_entry->d_name, O_RDONLY);

            if (subdir_fd != -1) {
                struct stat subdir_info = {0};
                int stat_result = fstat(subdir_fd, &subdir_info);

                if ((stat_result == 0) && S_ISDIR(subdir_info.st_mode)) {
                    memmi_String dir_name = str_from_c_str(subdir_entry->d_name);
                    MaybeS64 number_opt = str_to_s64(dir_name, NUM_BASE_DEC);

                    if (number_opt.ok) {
                        ProcessName proc_name = get_process_name(subdir_fd, allocator);

                        if (proc_name.ok) {
                            int64_t pid_value = number_opt.value;

                            memmi_ProcessInfo proc = {proc_name.value, {pid_value}};
                            dyn_arr_push(&processes, proc, allocator);
                        }
                    }
                }
            }

            close(subdir_fd);
        }
    }

    closedir(proc_dir);

    memmi_GetProcsStatus status = 0;

    // We can't possibly have succeeded if no processes were found, since we should at the very
    // least find this process.
    if (processes.count > 0) {
        status = MEMMI_GET_PROCS_OK;
    } else {
        status = MEMMI_GET_PROCS_FAIL;
    }

    memmi_ProcessList result = {
        .status = status,
        .data = processes.data,
        .count = processes.count
    };

    return result;
}

static int get_process_directory_fd(memmi_PID pid)
{
    DIR *proc_dir = opendir("/proc");
    int proc_dir_fd = dirfd(proc_dir);

    char buf[64];
    snprintf(buf, ARRAY_COUNT(buf), "%ld", pid.value);

    int result = openat(proc_dir_fd, buf, O_RDONLY);

    closedir(proc_dir);

    return result;
}

static bool pid_exists(memmi_PID pid)
{
    bool result = false;

    DIR *proc_dir = opendir("/proc");
    int proc_dir_fd = dirfd(proc_dir);

    if (proc_dir_fd != -1) {
        char pid_str[64];
        int chars_written = snprintf(pid_str, ARRAY_COUNT(pid_str), "%ld", pid.value);
        ASSERT(chars_written < (int)ARRAY_COUNT(pid_str));

        struct stat stat_buf = {0};
        int stat_result = fstatat(proc_dir_fd, pid_str, &stat_buf, 0);

        if (stat_result != -1) {
            result = true;
        }

    }

    closedir(proc_dir);

    return result;
}

memmi_OpenProcess memmi_open_process(memmi_PID pid, memmi_Allocator allocator)
{
    memmi_OpenProcess result = {0};

    if (!pid_exists(pid)) {
        result.status = MEMMI_OPEN_PROC_NO_SUCH_PID;
    } else {
        memmi_ProcessImpl *data = allocate(allocator, memmi_ProcessImpl, 1);

        if (!data) {
            result.status = MEMMI_OPEN_PROC_ALLOCATION_FAILED;
        } else {
            result.status = MEMMI_OPEN_PROC_OK;

            data->pid = pid;
            result.process.data = data;
        }
    }

    return result;
}

void memmi_close_process(memmi_Process process, memmi_Allocator allocator)
{
    deallocate(allocator, process.data, sizeof(memmi_ProcessImpl));
}

static memmi_ProcessImpl *get_platform_process_handle(memmi_Process proc)
{
    memmi_ProcessImpl *result = proc.data;

    return result;
}

memmi_ReadMemory memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator)
{
    memmi_ProcessImpl *proc = get_platform_process_handle(process);

    memmi_ReadMemory result = {0};

    if (size > (size_t)SSIZE_MAX) {
        result.status = MEMMI_READ_MEM_READ_TOO_LARGE;
    } else {
        result.memory = allocate(allocator, char, size);

        if (!result.memory) {
            result.status = MEMMI_READ_MEM_ALLOCATION_FAILURE;
        } else {
            struct iovec local_iov = {
                .iov_base = result.memory,
                .iov_len = size
            };

            struct iovec remote_iov = {
                .iov_base = (void *)address,
                .iov_len = size
            };

            ssize_t bytes_read = process_vm_readv((int)proc->pid.value, &local_iov, 1, &remote_iov, 1, 0);

            if (bytes_read == -1) {
                switch (errno) {
                    case EFAULT: {
                        result.status = MEMMI_READ_MEM_ACCESS_ERROR;
                    } break;

                    case ENOMEM: {
                        result.status = MEMMI_READ_MEM_ALLOCATION_FAILURE;
                    } break;

                    case EPERM: {
                        // Requires root or capability CAP_SYS_PTRACE.
                        result.status = MEMMI_READ_MEM_INSUFFICIENT_PERMISSIONS;
                    } break;

                    case ESRCH: {
                        result.status = MEMMI_READ_MEM_NO_SUCH_PROCESS;
                    } break;

                    default: {
                        ASSERT(false);
                    } break;
                }
            } else {
                result.bytes_read = (size_t)bytes_read;

                if (result.bytes_read < size) {
                    result.status = MEMMI_READ_MEM_PARTIAL_READ;
                } else {
                    result.status = MEMMI_READ_MEM_OK;
                }
            }
        }
    }

    return result;
}

memmi_WriteMemory memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;
    memmi_WriteMemory result = {0};

    if (src_size > (size_t)SSIZE_MAX) {
        result.status = MEMMI_WRITE_MEM_WRITE_TOO_LARGE;
    } else {
        struct iovec local_iov = {
            .iov_base = src,
            .iov_len = src_size
        };

        struct iovec remote_iov = {
            .iov_base = (void *)dst,
            .iov_len = src_size
        };

        ssize_t bytes_written = process_vm_writev((int)pid.value, &local_iov, 1, &remote_iov, 1, 0);

        if (bytes_written == -1) {
            switch (errno) {
                case EFAULT: {
                    result.status = MEMMI_WRITE_MEM_ACCESS_ERROR;
                } break;

                case ENOMEM: {
                    result.status = MEMMI_WRITE_MEM_ALLOCATION_FAILURE;
                } break;

                case EPERM: {
                    // Requires root or capability CAP_SYS_PTRACE.
                    result.status = MEMMI_WRITE_MEM_INSUFFICIENT_PERMISSIONS;
                } break;

                case ESRCH: {
                    result.status = MEMMI_WRITE_MEM_NO_SUCH_PROCESS;
                } break;

                default: {
                    ASSERT(false);
                } break;
            }
        } else {
            result.bytes_written = (size_t)bytes_written;

            if (result.bytes_written < src_size) {
                result.status = MEMMI_WRITE_MEM_PARTIAL_WRITE;
            } else {
                result.status = MEMMI_WRITE_MEM_OK;
            }
        }
    }

    return result;
}

static memmi_MemoryRegion parse_memory_region(memmi_String line)
{
    memmi_String fields[6];

    Cut cut = str_cut(line, str_lit(" "));

    for (size_t i = 0; i < ARRAY_COUNT(fields); ++i) {
        if (cut.head.count > 0) {
            fields[i] = cut.head;

            memmi_String trimmed_tail = str_trim_leading_whitespace(cut.tail);
            cut = str_cut(trimmed_tail, str_lit(" "));
        } else {
            break;
        }
    }

    const size_t address_index = 0;
    const size_t perms_index = 1;
    /* const size_t offset_index = 2; */
    /* const size_t dev_index = 3; */
    /* const size_t inode_index = 4; */
    /* const size_t pathname_index = 5; */

    Cut addresses = str_cut(fields[address_index], str_lit("-"));
    memmi_String base_address_str = addresses.head;
    memmi_String end_address_str = addresses.tail;

    memmi_String perms_str = fields[perms_index];

    MaybeU64 base_address_opt = str_to_u64(base_address_str, NUM_BASE_HEX);
    MaybeU64 end_address_opt = str_to_u64(end_address_str, NUM_BASE_HEX);
    ASSERT(base_address_opt.ok);
    ASSERT(end_address_opt.ok);

    uintptr_t base_address = base_address_opt.value;
    uintptr_t end_address = end_address_opt.value;

    memmi_MemoryRegionPermission permissions = 0;

    if (perms_str.data[0] == 'r') {
        permissions |= MEMMI_REGION_PERMISSION_READ;
    }

    if (perms_str.data[1] == 'w') {
        permissions |= MEMMI_REGION_PERMISSION_WRITE;
    }

    if (perms_str.data[2] == 'x') {
        permissions |= MEMMI_REGION_PERMISSION_EXECUTE;
    }

    size_t region_size = end_address - base_address;

    memmi_MemoryRegion result = {
        .base_address = base_address,
        .size = region_size,
        .permissions = permissions
    };

    return result;
}

memmi_GetMemoryRegions memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator)
{
    RegionDynArray regions = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;

    int proc_dir_fd = get_process_directory_fd(pid);
    int maps_fd = openat(proc_dir_fd, "maps", O_RDONLY);

    FILE *maps_file = fdopen(maps_fd, "r");

    memmi_GetMemoryRegions result = {0};

    if (!maps_file) {
        result.status = MEMMI_GET_REGIONS_FAIL;
    } else {
        char buffer[256];

        while (fgets(buffer, ARRAY_COUNT(buffer), maps_file)) {
            memmi_MemoryRegion region = parse_memory_region(str_from_c_str(buffer));
            dyn_arr_push(&regions, region, allocator);
        }

        result.data = regions.data;
        result.count = regions.count;
        result.status = MEMMI_GET_REGIONS_OK;
    }

    close(proc_dir_fd);
    fclose(maps_file);

    return result;
}

static memmi_AttachStatus ptrace_attach_result_to_memmi_status(long ptrace_result, int errno_value)
{
    memmi_AttachStatus result = 0;

    if (ptrace_result == -1) {
        switch (errno_value) {
            case EPERM: {
                result = MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS;
            } break;

            case ESRCH: {
                result = MEMMI_ATTACH_NO_SUCH_PROCESS;
            } break;

            default: {
                ASSERT(false);
            } break;
        }
    } else {
        result = MEMMI_ATTACH_OK;
    }

    return result;
}

static int get_signal_from_wait_status(int status)
{
    int result = 0;

    if (WIFEXITED(status)) {
        result = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result = WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        result = WSTOPSIG(status);
    } else {
        ASSERT(WIFCONTINUED(status));

        result = SIGCONT;
    }

    return result;
}

static memmi_AttachStatus attach_to_thread(memmi_TID tid)
{
    pid_t native_tid = (pid_t)tid.value;
    memmi_AttachStatus result = 0;

    long attach_result = ptrace(PTRACE_ATTACH, native_tid, 0, 0);

    if (attach_result == -1) {
        result = ptrace_attach_result_to_memmi_status(attach_result, errno);
    } else {
        while (true) {
            int status = 0;
            int wait_result = waitpid(native_tid, &status, __WALL);

            if (wait_result != native_tid) {
                // TODO: handle this
                ASSERT(0);
                break;
            } else if (WIFSTOPPED(status)) {
                // Thread successfully suspended.
                // TODO: Allow immediately resuming
                break;
            } else {
                // Thread received some other signal, inject it and try again next iteration.
                int signal = get_signal_from_wait_status(status);
                long resume_result = ptrace(PTRACE_CONT, native_tid, 0, signal);

                if (resume_result == -1) {
                    // Failed to inject signal, stop trying.
                    result = ptrace_attach_result_to_memmi_status(resume_result, errno);
                    break;
                }
            }
        }
    }

    return result;
}

static memmi_ResumeStatus resume_thread(memmi_TID tid)
{
    memmi_ResumeStatus result = MEMMI_RESUME_OK;

    // TODO: Do we need to waitpid for the signal to be received?
    long resume_result = ptrace(PTRACE_CONT, (pid_t)tid.value, 0, 0);

    if (resume_result == -1) {
        switch (errno) {
            case EPERM: {
                result = MEMMI_RESUME_INSUFFICIENT_PERMISSIONS;
            } break;

            case ESRCH: {
                result = MEMMI_RESUME_DEAD_OR_NOT_SUSPENDED;
            } break;

            default: {
                ASSERT(0);
            } break;
        }
    }

    return result;
}

typedef enum {
    FOR_EACH_THREAD_RES_CONTINUE,
    FOR_EACH_THREAD_RES_BREAK,
} ForEachThreadResult;

typedef ForEachThreadResult (*ForEachThreadFn)(void *user_data, memmi_TID tid);

// TODO: generalize this to iterating through all subdirs
// TODO: return error enum from this function, not a bool
static bool for_each_thread(memmi_PID pid, void *user_data, ForEachThreadFn fn)
{
    int proc_dir_fd = get_process_directory_fd(pid);
    int threads_dir_fd = openat(proc_dir_fd, "task", O_RDONLY);

    DIR *threads_dir = fdopendir(threads_dir_fd);

    bool result = false;

    if (threads_dir) {
        struct dirent *subdir_entry = 0;

        while ((subdir_entry = readdir(threads_dir))) {
            // Only count as a success if there was at least one thread,
            // as otherwise the process has been killed.
            result = true;

            // TODO: properly check if is dir with fstat
            ASSERT(subdir_entry->d_type == DT_DIR);

            memmi_String name = str_from_c_str(subdir_entry->d_name);
            MaybeS64 tid_opt = str_to_s64(name, NUM_BASE_DEC);

            if (tid_opt.ok) {
                memmi_TID tid = {tid_opt.value};

                ForEachThreadResult cb_result = fn(user_data, tid);

                if (cb_result == FOR_EACH_THREAD_RES_BREAK) {
                    break;
                }
            }
        }
    }

    close(proc_dir_fd);
    closedir(threads_dir);

    return result;
}

typedef struct {
    memmi_PID parent_pid;
    memmi_AttachStatus statuses;
} AttachThreadsContext;

static ForEachThreadResult attach_to_thread_cb(void *user_data, memmi_TID tid)
{
    AttachThreadsContext *context = user_data;

    if (context->parent_pid.value != tid.value) {
        memmi_AttachStatus thread_attach_result = attach_to_thread(tid);
        context->statuses |= thread_attach_result;
    }

    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

typedef struct {
    memmi_PID parent_pid;
    memmi_ResumeStatus statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, memmi_TID tid)
{
    ResumeThreadsContext *context = user_data;

    if (tid.value != context->parent_pid.value) {
        memmi_ResumeStatus resume_result = resume_thread(tid);
        context->statuses |= resume_result;
    }

    // TODO: maybe this return code isn't really needed
    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

memmi_AttachStatus memmi_attach_to_process(memmi_Process process)
{
    // TODO: make it so that new threads are automatically attached to

    memmi_AttachStatus result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_AttachStatus main_thread_attach_result = attach_to_thread((memmi_TID){native_pid});

    if (main_thread_attach_result != MEMMI_ATTACH_OK) {
        result = main_thread_attach_result;
    } else {
        // Since we attached to the main thread, a group stop will have been entered,
        // meaning all threads are now suspended. Therefore, we aren't racing against
        // thread creation when attaching to all threads.
        AttachThreadsContext attach_cb_context = {
            .parent_pid = pid,
        };

        bool for_each_result = for_each_thread(pid, &attach_cb_context, attach_to_thread_cb);

        if (!for_each_result) {
            // If the thread directory in procfs wasn't found, it probably means
            // that the process died inbetween us suspending the threads and
            // attaching to them.

            result = MEMMI_ATTACH_NO_SUCH_PROCESS;
        } else {
            // We succeeded in attaching to at least some of the child threads.
            uint32_t attach_errors =
                BIT(MEMMI_ATTACH_NO_SUCH_PROCESS) | BIT(MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS);
            bool partial_attach_success = attach_cb_context.statuses & attach_errors;

            ASSERT(!(attach_cb_context.statuses & BIT(MEMMI_ATTACH_SOME_THREADS_ATTACHED)));

            if (partial_attach_success) {
                // We only succeeded in attaching to some of the child threads,
                // so count this as a partial success.
                result = MEMMI_ATTACH_SOME_THREADS_ATTACHED;
            } else {
                // We did it!
                result = MEMMI_ATTACH_OK;
            }
        }
    }

    return result;
}

typedef struct {
    memmi_DetachStatus statuses;
} DetachContext;

static ForEachThreadResult detach_from_thread_cb(void *user_data, memmi_TID tid)
{
    DetachContext *context = user_data;
    pid_t native_tid = (pid_t)tid.value;

    long detach_result = ptrace(PTRACE_DETACH, native_tid, 0, 0);

    if (detach_result == -1) {
        memmi_DetachStatus status = 0;

        switch (errno) {
            case EPERM: {
                status = MEMMI_DETACH_INSUFFICIENT_PERMISSIONS;
            } break;

            case ESRCH: {
                status = MEMMI_DETACH_NO_SUCH_PROCESS;
            } break;
            default: {
                ASSERT(0);
            } break;
        }

        context->statuses |= status;
    }

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_DetachStatus memmi_detach_from_process(memmi_Process process)
{
    // NOTE: we assume that the function is suspended
    // TODO: clear breakpoints etc
    memmi_PID pid = get_platform_process_handle(process)->pid;
    DetachContext context = {0};

    bool for_each_result = for_each_thread(pid, &context, detach_from_thread_cb);

    memmi_DetachStatus result = 0;

    if (!for_each_result) {
        // If we failed to iterate the threads, we'll assume it was because the
        // process died.
        result = MEMMI_DETACH_NO_SUCH_PROCESS;
    } else {
        if (context.statuses == 0) {
            // No errors occurred.
            result = MEMMI_DETACH_OK;
        } else {
            // Some errors occurred, indicating that only some of the threads
            // were detached from.
            result = MEMMI_DETACH_SOME_THREADS_DETACHED;
        }
    }

    return result;
}

typedef struct {
    ThreadDynArray thread_list;
    memmi_Allocator allocator;
} CollectThreadsContext;

static ForEachThreadResult collect_threads(void *user_data, memmi_TID tid)
{
    CollectThreadsContext *context = user_data;
    dyn_arr_push(&context->thread_list, tid, context->allocator);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_ThreadList memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;

    CollectThreadsContext context = {
        .allocator = allocator
    };

    for_each_thread(pid, &context, collect_threads);

    memmi_ThreadList result = {
        .data = context.thread_list.data,
        .count = context.thread_list.count
    };

    return result;
}

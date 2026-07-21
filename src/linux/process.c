#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#include "process.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
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

static int get_process_directory_fd(pid_t pid)
{
    DIR *proc_dir = opendir("/proc");
    int proc_dir_fd = dirfd(proc_dir);

    char buf[64];
    snprintf(buf, ARRAY_COUNT(buf), "%d", pid);

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
    pid_t native_pid = (pid_t)pid.value;

    int proc_dir_fd = get_process_directory_fd(native_pid);
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

static memmi_ResumeStatus resume_thread(pid_t tid)
{
    memmi_ResumeStatus result = MEMMI_RESUME_OK;

    // TODO: Do we need to waitpid for the signal to be received?
    long resume_result = ptrace(PTRACE_CONT, tid, 0, 0);

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

// TODO: this isn't needed
typedef enum {
    FOR_EACH_THREAD_RES_CONTINUE,
    FOR_EACH_THREAD_RES_BREAK,
} ForEachThreadResult;

typedef ForEachThreadResult (*ForEachThreadFn)(void *user_data, pid_t tid);

// TODO: generalize this to iterating through all subdirs
// TODO: return error enum from this function, not a bool
static bool for_each_thread(pid_t pid, void *user_data, ForEachThreadFn fn)
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
                pid_t tid = (pid_t)tid_opt.value;

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
    pid_t parent_pid;
    memmi_ResumeStatus statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, pid_t tid)
{
    ResumeThreadsContext *context = user_data;

    if (tid != context->parent_pid) {
        memmi_ResumeStatus resume_result = resume_thread(tid);
        context->statuses |= BIT(resume_result);
    }

    // TODO: maybe this return code isn't really needed
    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

typedef struct {
    char data[256];
    size_t count;
} StatusEntry;

static StatusEntry get_proc_status_entry(pid_t tid, memmi_String row_name)
{
    StatusEntry result = {0};

    int proc_dir_fd = get_process_directory_fd(tid);
    int status_fd = openat(proc_dir_fd, "status", O_RDONLY);

    if ((proc_dir_fd != -1) && (status_fd != -1)) {
        FILE *status_file = fdopen(status_fd, "r");

        if (status_file) {
            while (fgets(result.data, ARRAY_COUNT(result.data), status_file)) {
                memmi_String line = str_from_c_str(result.data);

                if (str_starts_with(line, row_name)) {
                    Cut cut = str_cut(line, str_lit(":"));

                    if (cut.ok) {
                        memmi_String trimmed = str_trim_whitespace(cut.tail);
                        ASSERT(trimmed.count <= ARRAY_COUNT(result.data));

                        memmove(result.data, trimmed.data, trimmed.count);
                        result.count = trimmed.count;
                        break;
                    }
                }
            }
        }

        fclose(status_file);
    }

    close(proc_dir_fd);
    close(status_fd);

    return result;
}

// TODO: take pid_t as arg
static pid_t get_pid_of_tracing_process(pid_t tid)
{
    StatusEntry entry = get_proc_status_entry(tid, str_lit("TracerPid"));
    ASSERT(entry.data);
    ASSERT(entry.count);

    memmi_String str = str_from_span(entry);
    MaybeS64 pid_opt = str_to_s64(str, NUM_BASE_DEC);
    ASSERT(pid_opt.ok);
    pid_t result = (pid_t)pid_opt.value;

    return result;
}

static pid_t get_thread_group_id(pid_t tid)
{
    StatusEntry entry = get_proc_status_entry(tid, str_lit("Tgid"));
    ASSERT(entry.data);
    ASSERT(entry.count);

    memmi_String tgid_str = str_from_span(entry);
    MaybeS64 tgid_opt = str_to_s64(tgid_str, NUM_BASE_DEC);
    ASSERT(tgid_opt.ok);

    pid_t result = (pid_t)tgid_opt.value;

    return result;
}

static memmi_AttachStatus attach_to_thread(pid_t tid)
{
    memmi_AttachStatus result = 0;

    // TODO: trace fork?
    unsigned int ptrace_options = PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT;
    long seize_result = ptrace(PTRACE_SEIZE, tid, 0, (int)ptrace_options);

    if (seize_result == -1) {
        // We failed to seize the thread, which must either be because we lack
        // the permissions to do so, or because the thread has died.
        if (errno == ESRCH) {
            result = MEMMI_ATTACH_NO_SUCH_PROCESS;
        } else {
            ASSERT(errno == EPERM);
            result = MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS;
        }
    } else {
        long interrupt_result = ptrace(PTRACE_INTERRUPT, tid, 0, 0);

        if (interrupt_result == -1) {
            // We failed to interrupt the thread, most likely because the thread
            // died.  If we had the permissions to seize the process, I believe
            // we have the permissions to interrupt it. However, I suppose that
            // this process could technically lose those permissions inbetween
            // these two calls to ptrace.
            ASSERT(errno != EPERM && "Process lost permissions inbetween calls to ptrace");

            if (errno == ESRCH) {
                result = MEMMI_ATTACH_NO_SUCH_PROCESS;
            } else {
                ASSERT(errno == EPERM);
                result = MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS;
            }
        } else {
            // We successfully seized the thread and sent a stopping signal to
            // it, now we have to wait for it to actually receive the signal.
            while (true) {
                int status = 0;
                int waitpid_result = waitpid(tid, &status, __WALL);

                if (waitpid_result == -1) {
                    result = MEMMI_ATTACH_NO_SUCH_PROCESS;
                } else if (WIFSTOPPED(status)) {
                    // The thread was successfully suspended, we're done here.
                    result = MEMMI_ATTACH_OK;
                    break;
                } else {
                    // The thread received another signal, reinject it and try again.
                    int signal = get_signal_from_wait_status(signal);
                    long reinject_result = ptrace(PTRACE_CONT, tid, 0, signal);

                    if (reinject_result == -1) {
                        // Reinjecting the signal failed, either because the
                        // thread died or because we lost our permissions.
                        if (errno == ESRCH) {
                            result = MEMMI_ATTACH_NO_SUCH_PROCESS;
                        } else {
                            ASSERT(errno == EPERM);
                            result = MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS;
                        }

                        break;
                    }
                }
            }
        }
    }

    return result;
}

typedef struct {
    memmi_AttachStatus statuses;
    int32_t suspended_thread_count;
} AttachThreadsContext;

// TODO: take pid_t as arg
static ForEachThreadResult attach_to_thread_cb(void *user_data, pid_t tid)
{
    AttachThreadsContext *context = user_data;

    bool is_attached = false;

    // TODO: use process_is_traced_by_us
    pid_t tracer_pid = get_pid_of_tracing_process(tid);

    if (tracer_pid == getpid()) {
        // We are already attached to this process, nothing to do here.
        is_attached = true;
    } else {
        memmi_AttachStatus attach_result = attach_to_thread(tid);
        context->statuses |= BIT(attach_result);

        if (attach_result == MEMMI_ATTACH_OK) {
            is_attached = true;
        }
    }

    if (is_attached) {
        ++context->suspended_thread_count;
    }

    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

memmi_AttachStatus memmi_attach_to_process(memmi_Process process)
{
    memmi_AttachStatus result = 0;

    // TODO: only native_pid is needed now
    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_AttachStatus main_thread_attach_result = attach_to_thread(native_pid);

    if (main_thread_attach_result != MEMMI_ATTACH_OK) {
        result = main_thread_attach_result;
    } else {
        // We succeeded in attaching to the main thread, now start attaching to
        // the other threads in the process. From this point on, we won't care
        // if a thread dies while we are in the process of attaching to it,
        // since a thread dying can legitimately occur. We will however care if
        // we lack the permissions to attach to a thread. However, as we managed
        // to attach to the main thread, we will still count this as a partial success.
        int32_t last_attached_thread_count = 0;
        AttachThreadsContext cb_context = {0};
        bool suspended_thread_count_is_stable = false;

        // Attach to each thread in process until the number of attached threads
        // stabilizes. This is done in order to prevent races with thread creation.
        while (!suspended_thread_count_is_stable) {
            // Reset the context each iteration as outside the loop we only care
            // about the results from the last iteration.
            cb_context = (AttachThreadsContext){0};

            for_each_thread(native_pid, &cb_context, attach_to_thread_cb);

            if (cb_context.suspended_thread_count <= last_attached_thread_count) {
                // The number of threads we attached to has not grown since last
                // iteration, meaning no new threads can be spawned since all
                // threads in process are suspended. We are therefore done.  It
                // could also mean that we didn't attach to a single thread,
                // which we'll notice later on when checking
                // last_attached_thread_coutn.
                suspended_thread_count_is_stable = true;
            }

            last_attached_thread_count = cb_context.suspended_thread_count;
        }

        if (last_attached_thread_count == 0) {
            // If we didn't manage to attach to a single thread, count this as a
            // complete failure.
            if (cb_context.statuses & BIT(MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS)) {
                result = MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS;
            } else  {
                result = MEMMI_ATTACH_NO_SUCH_PROCESS;
            }
        } else if (cb_context.statuses & BIT(MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS)) {
            // If we failed to attach to some threads due to having insufficient
            // permissions, count this as a partial success. If we failed to
            // attach to some threads due to them dying, we'll count it as a
            // complete success, as threads dying can legitimately happen.
            result = MEMMI_ATTACH_SOME_THREADS_ATTACHED;
        } else {
            result = MEMMI_ATTACH_OK;
        }
    }

    return result;
}

typedef struct {
    memmi_DetachStatus statuses;
} DetachContext;

static ForEachThreadResult detach_from_thread_cb(void *user_data, pid_t tid)
{
    DetachContext *context = user_data;

    long detach_result = ptrace(PTRACE_DETACH, tid, 0, 0);

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
    pid_t native_pid = (pid_t)pid.value;

    DetachContext context = {0};

    bool for_each_result = for_each_thread(native_pid, &context, detach_from_thread_cb);

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

memmi_ResumeStatus memmi_resume_process(memmi_Process process)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_ResumeStatus result = 0;

    memmi_ResumeStatus main_thread_resume_result = resume_thread(native_pid);

    if (main_thread_resume_result != MEMMI_RESUME_OK) {
        // If we failed to resume the main thread, count this as a complete failure.
        result = main_thread_resume_result;
    } else {
        // Resuming the main thread succeeded, now try to resume the rest of the threads.
        ResumeThreadsContext resume_cb_context = {
            .parent_pid = native_pid,
        };

        bool for_each_result = for_each_thread(native_pid, &resume_cb_context, resume_thread_cb);

        if (!for_each_result) {
            // Process died before we had a chance to resume child threads.
            result = MEMMI_RESUME_DEAD_OR_NOT_SUSPENDED;
        } else {
            // Check if errors occured when resuming certain threads or if all succeeded.
            uint32_t resume_errors =
                BIT(MEMMI_RESUME_DEAD_OR_NOT_SUSPENDED) | BIT(MEMMI_RESUME_INSUFFICIENT_PERMISSIONS);
            bool partial_resume_success = resume_cb_context.statuses & resume_errors;

            if (partial_resume_success) {
                result = MEMMI_RESUME_PARTIAL_SUCCESS;
            } else {
                result = MEMMI_RESUME_OK;
            }
        }
    }

    return result;
}

static memmi_SuspendStatus suspend_thread(pid_t tid)
{
    memmi_SuspendStatus result = 0;

    long interrupt_result = ptrace(PTRACE_INTERRUPT, tid, 0, 0);

    if (interrupt_result == -1) {
        if (errno == EPERM) {
            result = MEMMI_SUSPEND_INSUFFICIENT_PERMISSIONS;
        } else {
            result = MEMMI_SUSPEND_NO_SUCH_PROCESS;
        }
    } else {
        int status = 0;
        // TODO: prevent hanging if process is already suspended
        int waitpid_result = waitpid(tid, &status, __WALL);

        if (waitpid_result == -1) {
            result = MEMMI_SUSPEND_NO_SUCH_PROCESS;
        } else {
            if (WIFSTOPPED(status)) {
                result = MEMMI_SUSPEND_OK;
            } else {
                // TODO: Can this occur? Do we need to resend stopping signal until it's seen or is
                // that only for PTRACE_ATTACH?
                ASSERT(0);
            }
        }
    }

    return result;
}

typedef struct {
    memmi_SuspendStatus statuses;
    int32_t suspended_thread_count;
} SuspendThreadsContext;

static ForEachThreadResult suspend_thread_cb(void *user_data, pid_t tid)
{
    DEBUG_BREAK;

    SuspendThreadsContext *context = user_data;

    bool is_suspended = false;
    // TODO: make get_proc_status_entry take tid as arg
    StatusEntry state_entry = get_proc_status_entry(tid, str_lit("State"));

    if (state_entry.count <= 0) {
        // Failed to get state for thread, probably because the thread died.
        // TODO: make sure that this is the case by checking errno
        ASSERT(state_entry.count == 0);
        context->statuses |= BIT(MEMMI_SUSPEND_NO_SUCH_PROCESS);
    } else {
        memmi_String state_entry_str = str_from_span(state_entry);

        is_suspended = str_starts_with(state_entry_str, str_lit("T"))
            || str_starts_with(state_entry_str, str_lit("t"));

        if (!is_suspended) {
            memmi_SuspendStatus suspend_result = suspend_thread(tid);
            context->statuses |= BIT(suspend_result);

            if (suspend_result == MEMMI_SUSPEND_OK) {
                is_suspended = true;
            }
        }
    }

    if (is_suspended) {
        ++context->suspended_thread_count;
    }

    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

memmi_SuspendStatus memmi_suspend_process(memmi_Process process)
{
    DEBUG_BREAK;

    memmi_SuspendStatus result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_SuspendStatus main_thread_suspend_result = suspend_thread(native_pid);

    // TODO: code duplication between this and memmi_attach_to_process
    if (main_thread_suspend_result != MEMMI_SUSPEND_OK) {
        // We failed to suspend the main thread, count this as a complete failure.
        result = main_thread_suspend_result;
    } else {
        int32_t last_suspended_thread_count = 0;
        SuspendThreadsContext cb_context = {0};
        bool suspended_thread_count_is_stable = false;

        while (!suspended_thread_count_is_stable) {
            // Keep trying to suspend threads until the number of suspended threads in the process
            // has stabilized.
            cb_context = (SuspendThreadsContext){0};

            for_each_thread(native_pid, &cb_context, suspend_thread_cb);

            if (cb_context.suspended_thread_count <= last_suspended_thread_count) {
                suspended_thread_count_is_stable = true;
            }

            last_suspended_thread_count = cb_context.suspended_thread_count;
        }

        if (last_suspended_thread_count == 0) {
            // If we didn't manage to suspend a single thread, count this as a complete failure.
            if (cb_context.statuses & BIT(MEMMI_SUSPEND_INSUFFICIENT_PERMISSIONS)) {
                result = MEMMI_SUSPEND_INSUFFICIENT_PERMISSIONS;
            } else  {
                result = MEMMI_SUSPEND_NO_SUCH_PROCESS;
            }
        } else if (cb_context.statuses & BIT(MEMMI_SUSPEND_INSUFFICIENT_PERMISSIONS)) {
            // If we failed to suspend some threads due to having insufficient permissions, count
            // this as a partial success. If we failed to suspend some threads due to them dying,
            // we'll count it as a complete success, as threads dying can legitimately happen.
            result = MEMMI_SUSPEND_PARTIAL_SUCCESS;
        } else {
            result = MEMMI_SUSPEND_OK;
        }
    }

    return result;
}

typedef struct {
    ThreadDynArray thread_list;
    memmi_Allocator allocator;
} CollectThreadsContext;

static ForEachThreadResult collect_threads(void *user_data, pid_t tid)
{
    CollectThreadsContext *context = user_data;
    dyn_arr_push(&context->thread_list, (memmi_TID){tid}, context->allocator);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_ThreadList memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    CollectThreadsContext context = {
        .allocator = allocator
    };

    for_each_thread(native_pid, &context, collect_threads);

    memmi_ThreadList result = {
        .data = context.thread_list.data,
        .count = context.thread_list.count
    };

    return result;
}

static bool thread_is_traced_by_us(pid_t pid)
{
    pid_t tracer_pid = get_pid_of_tracing_process(pid);

    bool result = tracer_pid == getpid();

    return result;
}

typedef enum {
    WAITPID_HANG,
    WAITPID_NO_HANG,
} WaitpidHang;

#define ptrace_event_code(e) (SIGTRAP | ((unsigned int)(e)) << 8)

static memmi_DebugEvent *wait_for_debug_event(memmi_Process proc, WaitpidHang hang, memmi_Allocator allocator)
{
    memmi_DebugEvent *result = 0;

    memmi_PID pid = get_platform_process_handle(proc)->pid;

    int waitpid_flags = __WALL;

    if (hang == WAITPID_NO_HANG) {
        waitpid_flags |= WNOHANG;
    }

    int status = 0;
    int id_of_affected_thread = waitpid(-1, &status, waitpid_flags);

    // TODO: if process is immediately killed, it may not exist anymore. Which we'll need to
    // handle. Check errno to see which error occurred.
    if (id_of_affected_thread == -1) {
        ASSERT(0 && "TODO: report error");
    } else if (id_of_affected_thread != 0) {
        // waitpid(-1, ...) will wait on any children, not just tracees,
        // including threads of the client process. We'll check that this thread
        // actually belongs to our tracee before reporting any events.
        pid_t thread_group_id = get_thread_group_id(id_of_affected_thread);
        bool thread_belongs_to_traced_process = thread_group_id == pid.value;

        if (thread_belongs_to_traced_process) {
            result = allocate(allocator, memmi_DebugEvent, 1);
            *result = (memmi_DebugEvent){0};

            result->id_of_affected_thread = (memmi_TID){id_of_affected_thread};

            siginfo_t sig_info = {0};
            long get_sig_result = ptrace(PTRACE_GETSIGINFO, id_of_affected_thread, 0, &sig_info);

            // TODO: breakpoints etc

            DEBUG_BREAK;

            if (get_sig_result == -1) {
                ASSERT(0 && "TODO: Report error");
            } else {
                switch (sig_info.si_code) {
                    // ptrace events
                    case ptrace_event_code(PTRACE_EVENT_STOP): {
                        result->kind = MEMMI_DEBUG_EVENT_THREAD_SUSPENDED;
                    } break;

                    case ptrace_event_code(PTRACE_EVENT_CLONE): {
                        long new_thread_id = 0;
                        long get_msg_result = ptrace(PTRACE_GETEVENTMSG, id_of_affected_thread, 0, &new_thread_id);

                        if (get_msg_result == -1) {
                            ASSERT(0 && "TODO: Report error");
                        } else {
                            result->kind = MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED;
                            result->as.new_thread.id = (memmi_TID){(int64_t)new_thread_id};
                        }
                    } break;

                    case ptrace_event_code(PTRACE_EVENT_EXIT): {
                        /* There's a bug in the kernel that causes SIGKILL to generate a
                           PTRACE_EVENT_EXIT. As far as I can tell there's no way to differentiate
                           this from a normal exit, so we'll have to simply report it as a normal
                           exit with code 0. Thanks ptrace! */
                        long exit_code = 0;
                        long get_msg_result = ptrace(PTRACE_GETEVENTMSG, id_of_affected_thread, 0, &exit_code);

                        if (get_msg_result == -1) {
                            ASSERT(0 && "TODO: Report error");
                        } else {
                            result->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
                            result->as.thread_exited.exit_code = (int)(exit_code >> 8);
                        }
                    } break;

                    default: {
                        // Normal signals
                        if (WIFEXITED(status)) {
                            ASSERT(0 && "Shouldn't happen, should have generated a PTRACE_EVENT_EXIT.");

                            result->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
                            result->as.thread_exited.exit_code = WEXITSTATUS(status);
                        } else if (WIFSIGNALED(status)) {
                            result->kind = MEMMI_DEBUG_EVENT_THREAD_KILLED;
                        } else if (WIFSTOPPED(status)) {
                            int signal = WSTOPSIG(status);

                            if (signal == SIGTRAP) {
                                // TODO: report pc register
                                // TODO: ensure that this works for hardware breakpoints too
                                result->kind = MEMMI_DEBUG_EVENT_BREAKPOINT;
                            } else {
                                result->kind = MEMMI_DEBUG_EVENT_THREAD_STOPPED;
                            }
                        } else if (WIFCONTINUED(status)) {
                            ASSERT(0 && "Unimplemented, how should this be handled?");
                        } else {
                            ASSERT(0 && "Unreachable");
                        }
                    } break;
                }
            }
        }
    }


    return result;
}

// TODO: allow waiting for events in specific thread
// TODO: allowing users to pass on events to tracee
memmi_EventList memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator)
{
    memmi_EventList result = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    // TODO: check that process exists

    // TODO: do we need to check that all threads are traced by us too?
    if (!thread_is_traced_by_us(native_pid)) {
        ASSERT(0 && "Cannot wait for events in a non-traced process");
    } else {
        memmi_resume_process(process);

        memmi_DebugEvent *event = wait_for_debug_event(process, WAITPID_HANG, allocator);

        while (event) {
            sl_push_back(&result, event);

            // Keep checking for debug events without hanging in case any more were queued.
            event = wait_for_debug_event(process, WAITPID_NO_HANG, allocator);
        }
    }

    return result;
}

static unsigned long long *get_user_regs_member_pointer(struct user_regs_struct *regs, memmi_Register reg)
{
    unsigned long long *result = 0;

    switch (reg) {
        case MEMMI_REG_RAX: {
            result = &regs->rax;
        } break;

        case MEMMI_REG_RCX: {
            result = &regs->rcx;
        } break;

        case MEMMI_REG_RDX: {
            result = &regs->rdx;
        } break;

        case MEMMI_REG_RSI: {
            result = &regs->rsi;
        } break;

        case MEMMI_REG_RDI: {
            result = &regs->rdi;
        } break;

        case MEMMI_REG_RSP: {
            result = &regs->rsp;
        } break;

        case MEMMI_REG_RBP: {
            result = &regs->rbp;
        } break;

        case MEMMI_REG_RBX: {
            result = &regs->rbx;
        } break;

        case MEMMI_REG_R8: {
            result = &regs->r8;
        } break;

        case MEMMI_REG_R9: {
            result = &regs->r9;
        } break;

        case MEMMI_REG_R10: {
            result = &regs->r10;
        } break;

        case MEMMI_REG_R11: {
            result = &regs->r11;
        } break;

        case MEMMI_REG_R12: {
            result = &regs->r12;
        } break;

        case MEMMI_REG_R13: {
            result = &regs->r13;
        } break;

        case MEMMI_REG_R14: {
            result = &regs->r14;
        } break;

        case MEMMI_REG_R15: {
            result = &regs->r15;
        } break;

        case MEMMI_REG_RIP: {
            result = &regs->rip;
        } break;

        case MEMMI_REG_CS: {
            result = &regs->cs;
        } break;

        case MEMMI_REG_EFLAGS: {
            result = &regs->eflags;
        } break;

        case MEMMI_REG_SS: {
            result = &regs->ss;
        } break;

        case MEMMI_REG_FS_BASE: {
            result = &regs->fs_base;
        } break;

        case MEMMI_REG_GS_BASE: {
            result = &regs->gs_base;
        } break;

        case MEMMI_REG_DS: {
            result = &regs->ds;
        } break;

        case MEMMI_REG_ES: {
            result = &regs->es;
        } break;

        case MEMMI_REG_FS: {
            result = &regs->fs;
        } break;

        case MEMMI_REG_GS: {
            result = &regs->gs;
        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}

memmi_Registers memmi_get_thread_registers(memmi_TID tid)
{
    memmi_Registers result = {0};

    struct user_regs_struct regs = {0};

    long get_regs_result = ptrace(PTRACE_GETREGS, (pid_t)tid.value, 0, &regs);

    if (get_regs_result == -1) {
        switch (errno) {
            case EPERM: {
                result.status = MEMMI_GET_REGS_INSUFFICIENT_PERMISSIONS;
            } break;

            case ESRCH: {
                result.status = MEMMI_GET_REGS_NO_SUCH_PROCESS;
            } break;

            default: {
                ASSERT(0);
                result.status = MEMMI_GET_REGS_NO_SUCH_PROCESS;
            } break;
        }
    } else {
        result.status = MEMMI_GET_REGS_OK;

        for (memmi_Register r = 0; r < MEMMI_REG_COUNT; ++r) {
            result.values[r] = *get_user_regs_member_pointer(&regs, r);
        }
    }

    return result;
}

// TODO: allow setting all registers at once
memmi_SetRegistersStatus memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value)
{
    memmi_SetRegistersStatus result = MEMMI_SET_REGS_NO_SUCH_PROCESS;

    struct user_regs_struct regs = {0};
    long get_regs_result = ptrace(PTRACE_GETREGS, (pid_t)tid.value, 0, &regs);

    if (get_regs_result == -1) {
        // TODO: Code duplication
        switch (errno) {
            case EPERM: {
                result = MEMMI_SET_REGS_INSUFFICIENT_PERMISSIONS;
            } break;

            case ESRCH: {
                result = MEMMI_SET_REGS_NO_SUCH_PROCESS;
            } break;

            default: {
                ASSERT(0);
                result = MEMMI_SET_REGS_NO_SUCH_PROCESS;
            } break;
        }
    } else {
        *get_user_regs_member_pointer(&regs, reg) = value;

        long set_regs_result = ptrace(PTRACE_SETREGS, (pid_t)tid.value, 0, &regs);

        if (set_regs_result == -1) {
            switch (errno) {
                case EPERM: {
                    result = MEMMI_SET_REGS_INSUFFICIENT_PERMISSIONS;
                } break;

                case ESRCH: {
                    result = MEMMI_SET_REGS_NO_SUCH_PROCESS;
                } break;

                default: {
                    ASSERT(0);
                    result = MEMMI_SET_REGS_NO_SUCH_PROCESS;
                } break;
            }
        } else {
            result = MEMMI_SET_REGS_OK;
        }
    }

    return result;
}

typedef enum {
    DEBUG_REG_DR0,
    DEBUG_REG_DR1,
    DEBUG_REG_DR2,
    DEBUG_REG_DR3,
    DEBUG_REG_DR6,
    DEBUG_REG_DR7,
    DEBUG_REG_COUNT,
} DebugRegister;

typedef struct {
    uint64_t values[DEBUG_REG_COUNT];
} DebugRegisters;

static size_t debug_register_user_struct_indices[DEBUG_REG_COUNT] = {
    [DEBUG_REG_DR0] = 0,
    [DEBUG_REG_DR1] = 1,
    [DEBUG_REG_DR2] = 2,
    [DEBUG_REG_DR3] = 3,
    [DEBUG_REG_DR6] = 6,
    [DEBUG_REG_DR7] = 7,
};

static size_t get_user_struct_debug_register_offset(DebugRegister reg)
{
    struct user u = {0};
    size_t reg_index = debug_register_user_struct_indices[reg];
    size_t regs_base = offsetof(struct user, u_debugreg);
    size_t reg_offset = reg_index * sizeof(*u.u_debugreg);

    size_t result = regs_base + reg_offset;

    return result;
}

static DebugRegisters get_thread_debug_registers(pid_t tid)
{
    DebugRegisters result = {0};

    errno = 0;

    for (DebugRegister reg = 0; reg < DEBUG_REG_COUNT; ++reg) {
        size_t reg_offset = get_user_struct_debug_register_offset(reg);

        long value = ptrace(PTRACE_PEEKUSER, tid, reg_offset, 0);

        if (errno != 0) {
            ASSERT(0);
            break;
        } else {
            result.values[reg] = (uint64_t)value;
        }
    }

    return result;
}

static memmi_SetRegistersStatus set_thread_debug_register(pid_t tid, DebugRegister reg, uint64_t value)
{
    memmi_SetRegistersStatus result = 0;

    size_t debug_reg_offset = get_user_struct_debug_register_offset(reg);
    long poke_result = ptrace(PTRACE_POKEUSER, tid, debug_reg_offset, value);

    if (poke_result == -1) {
        if (errno == EPERM) {
            result = MEMMI_SET_REGS_INSUFFICIENT_PERMISSIONS;
        } else {
            // TODO: more checking
            result = MEMMI_SET_REGS_NO_SUCH_PROCESS;
        }
    } else {
        result = MEMMI_SET_REGS_OK;
    }

    return result;
}

static DebugRegister debug_register_from_index(uint32_t index)
{
    DebugRegister result = 0;

    switch (index) {
        case 0: {
            result = DEBUG_REG_DR0;
        } break;

        case 1: {
            result = DEBUG_REG_DR1;
        } break;

        case 2: {
            result = DEBUG_REG_DR2;
        } break;

        case 3: {
            result = DEBUG_REG_DR3;
        } break;

        default: {
            ASSERT(0);
            result = DEBUG_REG_DR0;
        } break;
    }

    return result;
}

#define DR7_ENABLE_BIT_BASE_INDEX   16u
#define DR7_ENABLE_BIT_STRIDE       2u
#define DR7_COND_BITS_BASE_INDEX    16u
#define DR7_COND_BITS_STRIDE        4u
#define DR7_LENGTH_BITS_BASE_INDEX  18u
#define DR7_LENGTH_BITS_STRIDE      4u

#define DR7_READ_WRITE_COND         0b11u
#define DR7_WRITE_COND              0b01u
#define DR7_SIZE_1_BYTES            0b00
#define DR7_SIZE_2_BYTES            0b01
#define DR7_SIZE_4_BYTES            0b11
#define DR7_SIZE_8_BYTES            0b10

static uint64_t dr7_breakpoint_mask(uint32_t breakpoint_index)
{
    uint32_t result =
          (0b01u << (DR7_ENABLE_BIT_BASE_INDEX  + breakpoint_index * DR7_ENABLE_BIT_STRIDE))
        | (0b11u << (DR7_COND_BITS_BASE_INDEX   + breakpoint_index * DR7_COND_BITS_STRIDE))
        | (0b11u << (DR7_LENGTH_BITS_BASE_INDEX + breakpoint_index * DR7_LENGTH_BITS_STRIDE));

    return result;
}

static uint64_t dr7_local_enable_bit(uint32_t reg_index)
{
    uint64_t result = 0x1 << (reg_index * DR7_ENABLE_BIT_STRIDE);

    return result;
}

static uint64_t dr7_condition_bits(uint32_t reg_index, memmi_BreakpointCondition condition)
{
    uint64_t bits = 0;

    switch (condition) {
        case MEMMI_BREAKPOINT_READ_WRITE: {
            bits = DR7_READ_WRITE_COND;
        } break;

        case MEMMI_BREAKPOINT_WRITE: {
            bits = DR7_WRITE_COND;
        } break;

        default: {
            ASSERT(0);
            bits = DR7_READ_WRITE_COND;
        } break;
    }

    uint64_t result = bits << (DR7_COND_BITS_BASE_INDEX + reg_index * DR7_COND_BITS_STRIDE);

    return result;
}

static uint64_t dr7_length_bits(uint32_t reg_index, memmi_BreakpointLength length)
{
    uint64_t bits = 0;

    switch (length) {
        case MEMMI_BREAKPOINT_1_BYTES: {
            bits = DR7_SIZE_1_BYTES;
        } break;

        case MEMMI_BREAKPOINT_2_BYTES: {
            bits = DR7_SIZE_2_BYTES;
        } break;

        case MEMMI_BREAKPOINT_4_BYTES: {
            bits = DR7_SIZE_4_BYTES;
        } break;

        case MEMMI_BREAKPOINT_8_BYTES: {
            bits = DR7_SIZE_8_BYTES;
        } break;
    }

    uint64_t result = bits << (DR7_LENGTH_BITS_BASE_INDEX + reg_index * DR7_LENGTH_BITS_STRIDE);

    return result;
}

typedef struct {
    uint32_t index;
    uintptr_t address;
    memmi_BreakpointCondition condition;
    memmi_BreakpointLength length;

    memmi_SetRegistersStatus statuses;
} HardwareBreakpointContext;

// TODO: report errors
ForEachThreadResult set_hardware_breakpoint_on_thread_cb(void *user_data, pid_t tid)
{
    HardwareBreakpointContext *context = user_data;

    DebugRegister reg = debug_register_from_index(context->index);
    DebugRegisters debug_regs = get_thread_debug_registers(tid);

    uint64_t old_dr7_value = debug_regs.values[DEBUG_REG_DR7];

    uint64_t new_dr_value = context->address;
    uint64_t new_dr7_value =
        (old_dr7_value & ~dr7_breakpoint_mask(context->index))
        | dr7_local_enable_bit(context->index)
        | dr7_condition_bits(context->index, context->condition)
        | dr7_length_bits(context->index, context->length);

    memmi_SetRegistersStatus set_addr_result = set_thread_debug_register(tid, reg, new_dr_value);
    memmi_SetRegistersStatus set_dr7_result = set_thread_debug_register(tid, DEBUG_REG_DR7, new_dr7_value);

    context->statuses |= BIT(set_addr_result);
    context->statuses |= BIT(set_dr7_result);

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_SetBreakpointResult memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
    memmi_BreakpointCondition condition, uint32_t index, memmi_BreakpointLength length)
{
    memmi_SetBreakpointResult result = 0;

    if (index > 3) {
        result = MEMMI_SET_BREAKPOINT_INVALID_INDEX;
    } else {
        memmi_PID pid = get_platform_process_handle(process)->pid;
        pid_t native_pid = (pid_t)pid.value;

        HardwareBreakpointContext context = {0};
        context.index = index;
        context.address = address;
        context.condition = condition;
        context.length = length;

        bool for_each_result = for_each_thread(native_pid, &context, set_hardware_breakpoint_on_thread_cb);

        if (!for_each_result) {
            result = MEMMI_SET_BREAKPOINT_NO_SUCH_PROCESS;
        } else {
            if (context.statuses & BIT(MEMMI_SET_REGS_NO_SUCH_PROCESS)) {
                result = MEMMI_SET_BREAKPOINT_NO_SUCH_PROCESS;
            } else if (context.statuses & BIT(MEMMI_SET_REGS_INSUFFICIENT_PERMISSIONS)) {
                result = MEMMI_SET_BREAKPOINT_INSUFFICIENT_PERMISSIONS;
            } else {
                ASSERT(context.statuses & BIT(MEMMI_SET_REGS_OK));

                result = MEMMI_SET_BREAKPOINT_OK;
            }
        }
    }

    return result;
}

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
    memmi_ResumeStatus statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, memmi_TID tid)
{
    ResumeThreadsContext *context = user_data;

    if (tid.value != context->parent_pid.value) {
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

static StatusEntry get_proc_status_entry(memmi_PID pid, memmi_String row_name)
{
    StatusEntry result = {0};

    int proc_dir_fd = get_process_directory_fd(pid);
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

// TODO: make this take pid_t as arg
static pid_t get_pid_of_tracing_process(memmi_PID pid)
{
    StatusEntry entry = get_proc_status_entry(pid, str_lit("TracerPid"));
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
    StatusEntry entry = get_proc_status_entry((memmi_PID){tid}, str_lit("Tgid"));
    ASSERT(entry.data);
    ASSERT(entry.count);

    memmi_String tgid_str = str_from_span(entry);
    MaybeS64 tgid_opt = str_to_s64(tgid_str, NUM_BASE_DEC);
    ASSERT(tgid_opt.ok);

    pid_t result = (pid_t)tgid_opt.value;

    return result;
}

// TODO: move elsewhere
static memmi_PID tid_to_pid(memmi_TID tid)
{
    memmi_PID result = {tid.value};

    return result;
}

static memmi_AttachStatus attach_to_thread(memmi_TID tid)
{
    memmi_AttachStatus result = 0;

    pid_t native_tid = (pid_t)tid.value;

    // TODO: trace fork?
    unsigned int ptrace_options = PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT;
    long seize_result = ptrace(PTRACE_SEIZE, native_tid, 0, (int)ptrace_options);

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
        long interrupt_result = ptrace(PTRACE_INTERRUPT, native_tid, 0, 0);

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
                int waitpid_result = waitpid(native_tid, &status, __WALL);

                if (waitpid_result == -1) {
                    result = MEMMI_ATTACH_NO_SUCH_PROCESS;
                } else if (WIFSTOPPED(status)) {
                    // The thread was successfully suspended, we're done here.
                    result = MEMMI_ATTACH_OK;
                    break;
                } else {
                    // The thread received another signal, reinject it and try again.
                    int signal = get_signal_from_wait_status(signal);
                    long reinject_result = ptrace(PTRACE_CONT, native_tid, 0, signal);

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
    /* memmi_PID parent_pid; */
    memmi_AttachStatus statuses;
    int32_t suspended_thread_count;
} AttachThreadsContext;

static ForEachThreadResult attach_to_thread_cb(void *user_data, memmi_TID tid)
{
    AttachThreadsContext *context = user_data;

    bool is_attached = false;

    // TODO: use process_is_traced_by_us
    pid_t tracer_pid = get_pid_of_tracing_process(tid_to_pid(tid));

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

    memmi_PID pid = get_platform_process_handle(process)->pid;
    memmi_TID main_thread_id = {pid.value};

    memmi_AttachStatus main_thread_attach_result = attach_to_thread(main_thread_id);

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

            for_each_thread(pid, &cb_context, attach_to_thread_cb);

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

memmi_ResumeStatus memmi_resume_process(memmi_Process process)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;
    memmi_TID main_thread_tid = {pid.value};

    memmi_ResumeStatus result = 0;

    memmi_ResumeStatus main_thread_resume_result = resume_thread(main_thread_tid);

    if (main_thread_resume_result != MEMMI_RESUME_OK) {
        // If we failed to resume the main thread, count this as a complete failure.
        result = main_thread_resume_result;
    } else {
        // Resuming the main thread succeeded, now try to resume the rest of the threads.
        ResumeThreadsContext resume_cb_context = {
            .parent_pid = pid,
        };

        bool for_each_result = for_each_thread(pid, &resume_cb_context, resume_thread_cb);

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

static bool process_is_traced_by_us(memmi_PID pid)
{
    pid_t tracer_pid = get_pid_of_tracing_process(pid);

    bool result = tracer_pid == getpid();

    return result;
}

static bool status_is_ptrace_event(int status, int event)
{
    bool result = ((unsigned int)status >> 8) == (SIGTRAP | ((unsigned int)event << 8));

    return result;
}

typedef enum {
    WAITPID_HANG,
    WAITPID_NO_HANG,
} WaitpidHang;

static memmi_DebugEvent *wait_for_debug_event(memmi_Process proc, WaitpidHang hang, memmi_Allocator allocator)
{
    memmi_DebugEvent *result = 0;

    memmi_PID pid = get_platform_process_handle(proc)->pid;

    unsigned int waitpid_flags = __WALL;

    if (hang == WAITPID_NO_HANG) {
        waitpid_flags |= WNOHANG;
    }

    int status = 0;
    int id_of_affected_thread = waitpid(-1, &status, (int)waitpid_flags);

    if (id_of_affected_thread == -1) {
        // TODO: report error
        ASSERT(0);
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

            if (status_is_ptrace_event(status, PTRACE_EVENT_CLONE)) {
                // Thread created.
                result->kind = MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED;

                unsigned long new_thread_id = 0;
                long get_msg_result = ptrace(PTRACE_GETEVENTMSG, id_of_affected_thread, 0, &new_thread_id);

                if (get_msg_result == -1) {
                    ASSERT(0 && "Report error");
                } else {

                    result->as.new_thread.id = (memmi_TID){(int64_t)new_thread_id};
                }
            } else if (status_is_ptrace_event(status, PTRACE_EVENT_STOP)) {
                // Thread suspended.
                result->kind = MEMMI_DEBUG_EVENT_THREAD_SUSPENDED;
            } else if (status_is_ptrace_event(status, PTRACE_EVENT_EXIT)) {
                // Thread exited.
                result->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;

                long exit_code = 0;
                long get_msg_result = ptrace(PTRACE_GETEVENTMSG, id_of_affected_thread, 0, &exit_code);

                if (get_msg_result == -1) {
                    ASSERT(0 && "Report error");
                } else {
                    // Apparently exit code is returned shifted left 8 bits...
                    result->as.thread_exited.exit_code = (int)((unsigned int)exit_code >> 8u);
                }
            } else if (WIFSTOPPED(status)) {
                // Thread stopped, either by a stopping signal being received, a
                // breakpoint being triggered or a single step being performed.

                if (WSTOPSIG(status) == SIGTRAP) {
                    // Either a breakpoint that was triggered or a single step,
                    // investigate the signal code to disambiguate.
                    ASSERT(0);

                    siginfo_t sig_info = {0};

                    long get_sig_result = ptrace(PTRACE_GETSIGINFO, id_of_affected_thread, 0, &sig_info);

                    if (get_sig_result == -1){
                        ASSERT(0 && "Report error");
                    } else {
                        ASSERT(sig_info.si_signo == SIGTRAP);

                        switch (sig_info.si_code) {
                            case SI_KERNEL:
                            case TRAP_BRKPT: {
                                // Breakpoint.
                                ASSERT(0 && "Unimplemented");
                            } break;

                            case TRAP_TRACE: {
                                ASSERT(0 && "Unimplemented");
                                // Single step.
                            } break;

                            default: {
                                ASSERT(0 && "Invalid?");
                            } break;
                        }
                    }
                } else {
                    // Normal stop.
                    result->kind = MEMMI_DEBUG_EVENT_THREAD_STOPPED;
                }
            } else if (WIFSIGNALED(status)) {
                // Child was terminated.
                result->kind = MEMMI_DEBUG_EVENT_THREAD_KILLED;
                ASSERT(0);
            } else {
                // Not interested.
                // TODO: allocate in each branch so we don't make an unnecessary allocation in this case
                result = 0;
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

    if (!process_is_traced_by_us(pid)) {
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

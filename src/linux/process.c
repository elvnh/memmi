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

/***************************/
/* Common helper functions */
/***************************/
static memmi_ProcessImpl *get_platform_process_handle(memmi_Process proc)
{
    memmi_ProcessImpl *result = proc.data;

    return result;
}

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

static memmi_Status errno_to_memmi_status(int errno_value)
{
    memmi_Status result = 0;

    switch (errno_value) {
        case 0: {
            ASSERT(0 && "Shouldn't call this unless there was an actual error");
        } break;

        case EACCES:
        case EPERM: {
            result = MEMMI_INSUFFICIENT_PERMISSIONS;
        } break;

        case ESRCH: {
            result = MEMMI_NO_SUCH_PROCESS;
        } break;

        case ENOMEM: {
            result = MEMMI_ALLOCATION_FAILED;
        } break;

        case EFAULT:
        case EINVAL: {
            result = MEMMI_INVALID_ARGUMENTS;
        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}

static memmi_Status proc_fs_errno_to_memmi_status(int errno_value)
{
    memmi_Status result = 0;

    switch (errno_value) {
        case 0: {
            ASSERT(0 && "Shouldn't call this unless there was an actual error");
        } break;

        case EPERM:
        case EACCES: {
            result = MEMMI_INSUFFICIENT_PERMISSIONS;
        } break;

        case EBADF: {
            ASSERT(0 && "Should not have happened as fd should already have been verified to be valid.");
        } break;

        case ENFILE:
        case EMFILE: {
            ASSERT(0 && "TODO: generic MEMMI_OTHER_ERROR");
        } break;

        case ENOENT: {
            result = MEMMI_NO_SUCH_PROCESS;
        } break;

        case ENOMEM: {
            result = MEMMI_ALLOCATION_FAILED;
        } break;

        case ENOTDIR: {
            // TODO: return generic error here too
            ASSERT(0 && "Should not happen");
        } break;

        default: {
            ASSERT(0);
        } break;
    }

    return result;
}
typedef struct {
    memmi_Status status;
    int fd;
} ProcessDirFd;

static ProcessDirFd get_process_directory_fd(pid_t pid)
{
    ProcessDirFd result = {0};
    result.fd = -1;

    DIR *proc_dir = opendir("/proc");

    if (!proc_dir) {
        result.status = proc_fs_errno_to_memmi_status(errno);
    } else {
        // Gets closed by closedir.
        int proc_dir_fd = dirfd(proc_dir);

        if (proc_dir_fd == -1) {
            ASSERT(0 && "Should never happen, as we provided a valid DIR pointer.");
            result.status = MEMMI_OTHER_ERROR;
        } else {
            char buf[64];
            snprintf(buf, ARRAY_COUNT(buf), "%d", pid);

            result.fd = openat(proc_dir_fd, buf, O_RDONLY);

            if (result.fd == -1) {
                result.status = proc_fs_errno_to_memmi_status(errno);
            }
        }
    }

    closedir(proc_dir);

    return result;
}

// TODO: this isn't needed?
typedef enum {
    FOR_EACH_THREAD_RES_CONTINUE,
    FOR_EACH_THREAD_RES_BREAK,
} ForEachThreadResult;

typedef ForEachThreadResult (*ForEachThreadFn)(void *user_data, pid_t tid);

// TODO: generalize this?
static memmi_Status for_each_thread(pid_t pid, void *user_data, ForEachThreadFn fn)
{
    memmi_Status result = 0;

    bool found_thread_dir = false;
    ProcessDirFd proc_dir_fd = get_process_directory_fd(pid);

    if (proc_dir_fd.status != MEMMI_OK) {
        result = proc_dir_fd.status;
    } else {
        // Gets closed by closedir.
        int threads_dir_fd = openat(proc_dir_fd.fd, "task", O_RDONLY);
        ASSERT(proc_dir_fd.fd > 0);

        if (threads_dir_fd == -1) {
            result = proc_fs_errno_to_memmi_status(errno);
        } else {
            DIR *threads_dir = fdopendir(threads_dir_fd);

            if (!threads_dir) {
                result = proc_fs_errno_to_memmi_status(errno);
            } else {
                struct dirent *subdir_entry = 0;

                errno = 0;

                while ((subdir_entry = readdir(threads_dir))) {
                    // Only count as a success if there was at least one thread,
                    // as otherwise the process has been killed.
                    struct stat stat_buf = {0};
                    int stat_result = fstatat(threads_dir_fd, subdir_entry->d_name, &stat_buf, 0);
                    ASSERT(stat_result == 0);

                    // I believe there should never be any non-directory entries in the
                    // task subdirectory, but we'll check just to be sure.
                    if ((stat_result == 0) && S_ISDIR(stat_buf.st_mode)) {
                        found_thread_dir = true;

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

                if (errno != 0) {
                    result = proc_fs_errno_to_memmi_status(errno);
                }
            }

            closedir(threads_dir);
        }
    }

    if (!found_thread_dir) {
        // If we first found the process directory but then didn't find a single thread in
        // /proc/<pid>/task, that must mean that the process died before we had a chance
        // to iterate through the threads.
        result = MEMMI_NO_SUCH_PROCESS;
    }

    close(proc_dir_fd.fd);

    return result;
}

typedef struct {
    memmi_Status status;
    char data[256];
    size_t count;
} StatusFileRow;

static StatusFileRow get_proc_status_file_row(pid_t tid, memmi_String row_name)
{
    StatusFileRow result = {0};

    ProcessDirFd proc_dir_fd = get_process_directory_fd(tid);

    if (proc_dir_fd.status != MEMMI_OK) {
        result.status = proc_dir_fd.status;
    } else {
        ASSERT(proc_dir_fd.fd != -1);
        ASSERT(proc_dir_fd.fd != 0);

        int status_fd = openat(proc_dir_fd.fd, "status", O_RDONLY);

        if (status_fd == -1) {
            result.status = proc_fs_errno_to_memmi_status(errno);
        } else {
            FILE *status_file = fdopen(status_fd, "r");

            if (!status_file) {
                result.status = proc_fs_errno_to_memmi_status(errno);
            } else {
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

        close(status_fd);
    }

    close(proc_dir_fd.fd);

    return result;
}

static memmi_Status pid_exists(pid_t pid)
{
    // TODO: error check and return memmi_Status
    memmi_Status result = 0;

    DIR *proc_dir = opendir("/proc");
    int proc_dir_fd = dirfd(proc_dir);

    if (proc_dir_fd == -1) {
        result = MEMMI_NO_SUCH_PROCESS;
    } else {
        char pid_str[64];
        int chars_written = snprintf(pid_str, ARRAY_COUNT(pid_str), "%d", pid);
        ASSERT(chars_written < (int)ARRAY_COUNT(pid_str));

        struct stat stat_buf = {0};
        int stat_result = fstatat(proc_dir_fd, pid_str, &stat_buf, 0);

        if (stat_result == -1) {
            result = proc_fs_errno_to_memmi_status(errno);
        }

    }

    closedir(proc_dir);

    return result;
}

typedef struct {
    memmi_Status status;
    pid_t pid;
} PidResult;

static PidResult get_pid_of_tracing_process(pid_t tid)
{
    PidResult result = {0};
    StatusFileRow entry = get_proc_status_file_row(tid, str_lit("TracerPid"));

    if (entry.status != MEMMI_OK) {
        result.status = entry.status;
    } else {
        memmi_String str = str_from_span(entry);
        MaybeS64 pid_opt = str_to_s64(str, NUM_BASE_DEC);
        ASSERT(pid_opt.ok);

        result.pid = (pid_t)pid_opt.value;
    }


    return result;
}

static bool thread_is_traced_by_us(pid_t pid)
{
    bool result = false;
    PidResult tracer_pid = get_pid_of_tracing_process(pid);

    if (tracer_pid.status == MEMMI_OK) {
        result = tracer_pid.pid == getpid();
    }

    return result;
}

static PidResult get_thread_group_id(pid_t tid)
{
    PidResult result = {0};

    StatusFileRow entry = get_proc_status_file_row(tid, str_lit("Tgid"));

    if (entry.status != MEMMI_OK) {
        result.status = entry.status;
    } else {
        ASSERT(entry.data);
        ASSERT(entry.count);

        memmi_String tgid_str = str_from_span(entry);
        MaybeS64 tgid_opt = str_to_s64(tgid_str, NUM_BASE_DEC);
        ASSERT(tgid_opt.ok);
        result.pid = (pid_t)tgid_opt.value;
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

/**********************/
/* API implementation */
/**********************/
memmi_OpenProcess memmi_open_process(memmi_PID pid, memmi_Allocator allocator)
{
    memmi_OpenProcess result = {0};

    memmi_Status pid_exists_result = pid_exists((pid_t)pid.value);

    if (pid_exists_result != MEMMI_OK) {
        result.status = pid_exists_result;
    } else {
        memmi_ProcessImpl *data = allocate(allocator, memmi_ProcessImpl, 1);

        if (!data) {
            result.status = MEMMI_ALLOCATION_FAILED;
        } else {
            data->pid = pid;
            result.process.data = data;
        }
    }

    return result;
}

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator)
{
    memmi_ProcessList result = {0};
    ProcessDynArray processes = {0};

    DIR *proc_dir = opendir("/proc");

    if (!proc_dir) {
        result.status = proc_fs_errno_to_memmi_status(errno);
    } else {
        int proc_dir_fd = dirfd(proc_dir); // Automatically closed by closedir
        ASSERT(proc_dir_fd != -1);

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

    result.data = processes.data;
    result.count = processes.count;

    return result;
}

void memmi_close_process(memmi_Process process, memmi_Allocator allocator)
{
    deallocate(allocator, process.data, sizeof(memmi_ProcessImpl));
}

memmi_ReadMemory memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator)
{
    memmi_ProcessImpl *proc = get_platform_process_handle(process);

    memmi_ReadMemory result = {0};

    if (size > (size_t)SSIZE_MAX) {
        result.status = MEMMI_INVALID_ARGUMENTS;
    } else {
        result.memory = allocate(allocator, char, size);

        if (!result.memory) {
            result.status = MEMMI_ALLOCATION_FAILED;
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
                result.status = errno_to_memmi_status(errno);
            } else {
                result.bytes_read = (size_t)bytes_read;

                if (result.bytes_read < size) {
                    result.status = MEMMI_PARTIAL_READ_OR_WRITE;
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
        result.status = MEMMI_INVALID_ARGUMENTS;
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
            result.status = errno_to_memmi_status(errno);
        } else {
            result.bytes_written = (size_t)bytes_written;

            if (result.bytes_written < src_size) {
                result.status = MEMMI_PARTIAL_READ_OR_WRITE;
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

memmi_MemoryRegions memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator)
{
    memmi_MemoryRegions result = {0};
    RegionDynArray regions = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_Status pid_exists_result = pid_exists(native_pid);

    if (pid_exists_result != MEMMI_OK) {
        result.status = pid_exists_result;
    } else {
        ProcessDirFd proc_dir_fd = get_process_directory_fd(native_pid);

        if (proc_dir_fd.status != MEMMI_OK) {
            result.status = proc_dir_fd.status;
        } else {
            ASSERT(proc_dir_fd.fd > 0);

            // TODO: this pattern of proc dir fd, followed by open of subdir, followed by fdopen etc.
            // is starting to be a pattern, maybe extract out to a function

            // Automatically closed by fclose.
            int maps_fd = openat(proc_dir_fd.fd, "maps", O_RDONLY);

            if (maps_fd == -1) {
                result.status = proc_fs_errno_to_memmi_status(errno);
            } else {
                FILE *maps_file = fdopen(maps_fd, "r");

                if (!maps_file) {
                    result.status = proc_fs_errno_to_memmi_status(errno);
                } else {
                    char buffer[256];

                    while (fgets(buffer, ARRAY_COUNT(buffer), maps_file)) {
                        memmi_MemoryRegion region = parse_memory_region(str_from_c_str(buffer));
                        dyn_arr_push(&regions, region, allocator);
                    }

                    result.data = regions.data;
                    result.count = regions.count;
                }

                fclose(maps_file);
            }
        }

        close(proc_dir_fd.fd);
    }

    return result;
}

static memmi_Status resume_thread(pid_t tid)
{
    // TODO: Do we need to waitpid for the signal to be received?

    memmi_Status result = 0;

    if (ptrace(PTRACE_CONT, tid, 0, 0) == -1)  {
        result = errno_to_memmi_status(errno);
    }

    return result;
}

typedef struct {
    pid_t parent_pid;
    memmi_Status statuses;
} ResumeThreadsContext;

static ForEachThreadResult resume_thread_cb(void *user_data, pid_t tid)
{
    ResumeThreadsContext *context = user_data;

    if (tid != context->parent_pid) {
        memmi_Status resume_result = resume_thread(tid);
        context->statuses |= resume_result;
    }

    // TODO: maybe this return code isn't really needed
    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

static memmi_Status attach_to_thread(pid_t tid)
{
    memmi_Status result = 0;

    // TODO: trace fork?
    unsigned int ptrace_options = PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT;
    long seize_result = ptrace(PTRACE_SEIZE, tid, 0, (int)ptrace_options);

    if (seize_result == -1) {
        // We failed to seize the thread, which must either be because we lack
        // the permissions to do so, or because the thread has died.
        result = errno_to_memmi_status(errno);
    } else {
        long interrupt_result = ptrace(PTRACE_INTERRUPT, tid, 0, 0);

        if (interrupt_result == -1) {
            // We failed to interrupt the thread, most likely because the thread
            // died.  If we had the permissions to seize the process, I believe
            // we have the permissions to interrupt it. However, I suppose that
            // this process could technically lose those permissions inbetween
            // these two calls to ptrace.
            ASSERT(errno != EPERM && "Process lost permissions inbetween calls to ptrace");

            result = errno_to_memmi_status(errno);
        } else {
            // We successfully seized the thread and sent a stopping signal to
            // it, now we have to wait for it to actually receive the signal.
            while (true) {
                int status = 0;
                int waitpid_result = waitpid(tid, &status, __WALL);

                if (waitpid_result == -1) {
                    result = errno_to_memmi_status(errno);
                    break;
                } else if (WIFSTOPPED(status)) {
                    // The thread was successfully suspended, we're done here.
                    break;
                } else {
                    // The thread received another signal, reinject it and try again.
                    int signal = get_signal_from_wait_status(signal);
                    long reinject_result = ptrace(PTRACE_CONT, tid, 0, signal);

                    if (reinject_result == -1) {
                        // Reinjecting the signal failed, either because the
                        // thread died or because we lost our permissions.
                        result = errno_to_memmi_status(errno);

                        break;
                    }
                }
            }
        }
    }

    return result;
}

typedef struct {
    memmi_Status statuses;
    int32_t suspended_thread_count;
} AttachThreadsContext;

static ForEachThreadResult attach_to_thread_cb(void *user_data, pid_t tid)
{
    AttachThreadsContext *context = user_data;

    bool is_attached = false;

    if (thread_is_traced_by_us(tid)) {
        is_attached = true;
    } else {
        memmi_Status attach_result = attach_to_thread(tid);
        context->statuses |= attach_result;

        if (attach_result == MEMMI_OK) {
            is_attached = true;
        }
    }

    if (is_attached) {
        ++context->suspended_thread_count;
    }

    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

memmi_Status memmi_attach_to_process(memmi_Process process)
{
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_Status pid_exists_result = pid_exists(native_pid);

    if (pid_exists_result != MEMMI_OK) {
        result = pid_exists_result;
    } else {
        memmi_Status main_thread_attach_result = attach_to_thread(native_pid);

        if (main_thread_attach_result != MEMMI_OK) {
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
            while (!suspended_thread_count_is_stable && (result == MEMMI_OK)) {
                cb_context.suspended_thread_count = 0;

                memmi_Status for_each_thread_result = for_each_thread(native_pid, &cb_context, attach_to_thread_cb);

                if (for_each_thread_result != MEMMI_OK) {
                    result = for_each_thread_result;
                } else {
                    if (cb_context.suspended_thread_count <= last_attached_thread_count) {
                        // The number of threads we attached to has not grown since last iteration, meaning
                        // no new threads can be spawned since all threads in process are suspended. We are
                        // therefore done.  It could also mean that we didn't attach to a single thread,
                        // which we'll notice later on when checking last_attached_thread_count.
                        suspended_thread_count_is_stable = true;
                    }

                    last_attached_thread_count = cb_context.suspended_thread_count;

                    // If we didn't manage to attach to a single thread, we care about the
                    // error code MEMMI_NO_SUCH_PROCESS as this probably means the process
                    // died. If we attached to at least one thread however, we don't care
                    // about that code as a child thread may have legitimately died.
                    if (last_attached_thread_count == 0) {
                        result = cb_context.statuses;
                        ASSERT(result != MEMMI_OK);
                    } else {
                        uint32_t statuses_excluding_no_such_process =
                            cb_context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS;

                        result = statuses_excluding_no_such_process;
                    }
                }
            }
        }
    }

    return result;
}

typedef struct {
    memmi_Status statuses;
} DetachContext;

static ForEachThreadResult detach_from_thread_cb(void *user_data, pid_t tid)
{
    DetachContext *context = user_data;

    if (ptrace(PTRACE_DETACH, tid, 0, 0) == -1) {
        context->statuses |= errno_to_memmi_status(errno);
    }

    return FOR_EACH_THREAD_RES_CONTINUE;
}

memmi_Status memmi_detach_from_process(memmi_Process process)
{
    // NOTE: we assume that the function is suspended
    // TODO: clear breakpoints etc?
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;
    memmi_Status pid_exists_result = pid_exists(native_pid);

    if (pid_exists_result != MEMMI_OK) {
        result = pid_exists_result;
    } else {
        DetachContext cb_context = {0};
        memmi_Status for_each_result = for_each_thread(native_pid, &cb_context, detach_from_thread_cb);

        if (for_each_result != MEMMI_OK) {
            result = for_each_result;
        } else {
            // If we failed to detach from any thread for any reason except for the thread dying,
            // count this as a failure. If we failed due to threads dying, we'll ignore that and
            // count it as a success.
            uint32_t statuses_excluding_no_such_process =
                cb_context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS;

            result = statuses_excluding_no_such_process;
        }
    }

    return result;
}

memmi_Status memmi_resume_process(memmi_Process process)
{
    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_Status result = 0;

    memmi_Status main_thread_resume_result = resume_thread(native_pid);

    if (main_thread_resume_result != MEMMI_OK) {
        // If we failed to resume the main thread, count this as a complete failure.
        result = main_thread_resume_result;
    } else {
        // Resuming the main thread succeeded, now try to resume the rest of the threads.
        ResumeThreadsContext resume_cb_context = {
            .parent_pid = native_pid,
        };

        memmi_Status for_each_result = for_each_thread(native_pid, &resume_cb_context, resume_thread_cb);

        if (for_each_result != MEMMI_OK) {
            result = for_each_result;
        } else {
            // Check if errors occured when resuming certain threads or if all succeeded. If any
            // threads failed due to them dying, we'll ignore that as threads can die without an
            // error having occurred.
            uint32_t statuses_excluding_no_such_process =
                resume_cb_context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS;

            result = statuses_excluding_no_such_process;
        }
    }

    return result;
}

static memmi_Status suspend_thread(pid_t tid)
{
    memmi_Status result = 0;

    long interrupt_result = ptrace(PTRACE_INTERRUPT, tid, 0, 0);

    if (interrupt_result == -1) {
        result = errno_to_memmi_status(errno);
    } else {
        int status = 0;
        // TODO: prevent hanging if process is already suspended
        int waitpid_result = waitpid(tid, &status, __WALL);

        if (waitpid_result == -1) {
            result = errno_to_memmi_status(errno);
        } else {
            if (WIFSTOPPED(status)) {
                // Success!
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
    memmi_Status statuses;
    int32_t suspended_thread_count;
} SuspendThreadsContext;

static ForEachThreadResult suspend_thread_cb(void *user_data, pid_t tid)
{
    SuspendThreadsContext *context = user_data;

    bool is_suspended = false;
    StatusFileRow state_entry = get_proc_status_file_row(tid, str_lit("State"));

    if (state_entry.status != MEMMI_OK) {
        context->statuses |= state_entry.status;
    } else {
        memmi_String state_entry_str = str_from_span(state_entry);

        is_suspended = str_starts_with(state_entry_str, str_lit("T"))
            || str_starts_with(state_entry_str, str_lit("t"));

        if (!is_suspended) {
            memmi_Status suspend_result = suspend_thread(tid);

            if (suspend_result == MEMMI_OK) {
                is_suspended = true;
            } else {
                context->statuses |= suspend_result;
            }
        }
    }

    if (is_suspended) {
        ++context->suspended_thread_count;
    }

    ForEachThreadResult result = FOR_EACH_THREAD_RES_CONTINUE;

    return result;
}

memmi_Status memmi_suspend_process(memmi_Process process)
{
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    memmi_Status main_thread_suspend_result = suspend_thread(native_pid);

    // TODO: code duplication between this and memmi_attach_to_process
    if (main_thread_suspend_result != MEMMI_OK) {
        // We failed to suspend the main thread, count this as a complete failure.
        result = main_thread_suspend_result;
    } else {
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
    memmi_ThreadList result = {0};

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;

    CollectThreadsContext context = {
        .allocator = allocator
    };

    result.status = for_each_thread(native_pid, &context, collect_threads);

    if (result.status == MEMMI_OK) {
        result.data = context.thread_list.data;
        result.count = context.thread_list.count;
    }

    return result;
}

typedef enum {
    WAITPID_HANG,
    WAITPID_NO_HANG,
} WaitpidHang;

#define ptrace_event_code(e) (SIGTRAP | ((unsigned int)(e)) << 8)

typedef struct {
    memmi_Status status;
    memmi_DebugEvent *data;
} DebugEventResult;

static DebugEventResult wait_for_debug_event(memmi_Process proc, WaitpidHang hang, memmi_Allocator allocator)
{
    DebugEventResult result = {0};

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
        PidResult thread_group_id = get_thread_group_id(id_of_affected_thread);

        if (thread_group_id.status != MEMMI_OK) {
            result.status = thread_group_id.status;
        } else {
            bool thread_belongs_to_traced_process = thread_group_id.pid == pid.value;

            if (thread_belongs_to_traced_process) {
                result.data = allocate(allocator, memmi_DebugEvent, 1);
                *result.data = (memmi_DebugEvent){0};

                result.data->id_of_affected_thread = (memmi_TID){id_of_affected_thread};

                siginfo_t sig_info = {0};
                long get_sig_result = ptrace(PTRACE_GETSIGINFO, id_of_affected_thread, 0, &sig_info);

                DEBUG_BREAK;

                if (get_sig_result == -1) {
                    result.status = errno_to_memmi_status(errno);
                } else {
                    switch (sig_info.si_code) {
                        // ptrace events
                        case ptrace_event_code(PTRACE_EVENT_STOP): {
                            result.data->kind = MEMMI_DEBUG_EVENT_THREAD_SUSPENDED;
                        } break;

                        case ptrace_event_code(PTRACE_EVENT_CLONE): {
                            long new_thread_id = 0;
                            long get_msg_result = ptrace(PTRACE_GETEVENTMSG, id_of_affected_thread, 0, &new_thread_id);

                            if (get_msg_result == -1) {
                                result.status = errno_to_memmi_status(errno);
                            } else {
                                result.data->kind = MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED;
                                result.data->as.new_thread.id = (memmi_TID){(int64_t)new_thread_id};
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
                                result.status = errno_to_memmi_status(errno);
                            } else {
                                result.data->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
                                result.data->as.thread_exited.exit_code = (int)(exit_code >> 8);
                            }
                        } break;

                        default: {
                            // Normal signals
                            if (WIFEXITED(status)) {
                                ASSERT(0 && "Shouldn't happen, should have generated a PTRACE_EVENT_EXIT.");

                                result.data->kind = MEMMI_DEBUG_EVENT_THREAD_EXITED;
                                result.data->as.thread_exited.exit_code = WEXITSTATUS(status);
                            } else if (WIFSIGNALED(status)) {
                                result.data->kind = MEMMI_DEBUG_EVENT_THREAD_KILLED;
                            } else if (WIFSTOPPED(status)) {
                                int signal = WSTOPSIG(status);

                                if (signal == SIGTRAP) {
                                    // TODO: report pc register
                                    // TODO: ensure that this works for hardware breakpoints too
                                    result.data->kind = MEMMI_DEBUG_EVENT_BREAKPOINT;
                                } else {
                                    result.data->kind = MEMMI_DEBUG_EVENT_THREAD_STOPPED;
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

    memmi_Status pid_exists_result = pid_exists(native_pid);

    if (pid_exists_result != MEMMI_OK) {
        result.status = pid_exists_result;
    } else {
        // TODO: do we need to check that all threads are traced by us too?
        if (!thread_is_traced_by_us(native_pid)) {
            ASSERT(0 && "Cannot wait for events in a non-traced process");
        } else {
            memmi_Status resume_result = memmi_resume_process(process);

            if (resume_result != MEMMI_OK) {
                result.status = resume_result;
            } else {
                DebugEventResult event = wait_for_debug_event(process, WAITPID_HANG, allocator);

                // Keep checking for debug events without hanging in case any more were queued.
                while (event.status == MEMMI_OK) {
                    ASSERT(event.data);

                    sl_push_back(&result, event.data);

                    event = wait_for_debug_event(process, WAITPID_NO_HANG, allocator);
                }

                result.status = event.status;
            }
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

    memmi_Status pid_exists_result = pid_exists((pid_t)tid.value);

    if (pid_exists_result != MEMMI_OK) {
        result.status = pid_exists_result;
    } else {
        struct user_regs_struct regs = {0};
        long get_regs_result = ptrace(PTRACE_GETREGS, (pid_t)tid.value, 0, &regs);

        if (get_regs_result == -1) {
            result.status = errno_to_memmi_status(errno);
        } else {
            for (memmi_Register r = 0; r < MEMMI_REG_COUNT; ++r) {
                result.values[r] = *get_user_regs_member_pointer(&regs, r);
            }
        }
    }

    return result;
}

// TODO: allow setting all registers at once
memmi_Status memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value)
{
    memmi_Status result = 0;

    memmi_Status pid_exists_result = pid_exists((pid_t)tid.value);

    if (pid_exists_result != MEMMI_OK) {
        result = pid_exists_result;
    } else {
        struct user_regs_struct regs = {0};
        long get_regs_result = ptrace(PTRACE_GETREGS, (pid_t)tid.value, 0, &regs);

        if (get_regs_result == -1) {
            result = errno_to_memmi_status(errno);
        } else {
            *get_user_regs_member_pointer(&regs, reg) = value;

            long set_regs_result = ptrace(PTRACE_SETREGS, (pid_t)tid.value, 0, &regs);

            if (set_regs_result == -1) {
                result = errno_to_memmi_status(errno);
            }
        }
    }

    return result;
}

// TODO: include this in memmi.c instead
#include "memmi_x64.c"

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

    for (DebugRegister reg = 0; reg < DEBUG_REG_COUNT; ++reg) {
        size_t reg_offset = get_user_struct_debug_register_offset(reg);

        // PTRACE_PEEKUSER does not return -1 on error, so errno must be checked, and therefore also
        // cleared before each call.
        errno = 0;
        long value = ptrace(PTRACE_PEEKUSER, tid, reg_offset, 0);

        if (errno != 0) {
            result.status = errno_to_memmi_status(errno);
            break;
        } else {
            result.values[reg] = (uint64_t)value;
        }
    }

    return result;
}

static memmi_Status set_thread_debug_register(pid_t tid, DebugRegister reg, uint64_t value)
{
    memmi_Status result = 0;
    size_t debug_reg_offset = get_user_struct_debug_register_offset(reg);

    if (ptrace(PTRACE_POKEUSER, tid, debug_reg_offset, value) == -1) {
        result = errno_to_memmi_status(errno);
    }

    return result;
}


memmi_Status set_hardware_breakpoint_on_thread(pid_t tid, uint32_t index, uintptr_t address,
    memmi_BreakpointCondition cond, memmi_BreakpointLength length)
{
    memmi_Status result = 0;

    DebugRegister reg = debug_register_from_index(index);
    DebugRegisters debug_regs = get_thread_debug_registers(tid);
    result = debug_regs.status;

    if (result == MEMMI_OK) {
        uint64_t old_dr7_value = debug_regs.values[DEBUG_REG_DR7];

        uint64_t new_dr_value = address;
        uint64_t new_dr7_value = dr7_set_breakpoint(old_dr7_value, index, cond, length);

        memmi_Status set_addr_result = set_thread_debug_register(tid, reg, new_dr_value);
        memmi_Status set_dr7_result = set_thread_debug_register(tid, DEBUG_REG_DR7, new_dr7_value);

        result |= set_addr_result
               | set_dr7_result;
    }

    return result;
}

typedef struct {
    uint32_t index;
    uintptr_t address;
    memmi_BreakpointCondition condition;
    memmi_BreakpointLength length;

    memmi_Status statuses;
} HardwareBreakpointContext;

ForEachThreadResult set_hardware_breakpoint_on_thread_cb(void *user_data, pid_t tid)
{
    HardwareBreakpointContext *context = user_data;

    memmi_Status set_bp_result = set_hardware_breakpoint_on_thread(
        tid, context->index, context->address, context->condition, context->length);
    context->statuses |= set_bp_result;

    ForEachThreadResult result = 0;
    if (context->statuses == MEMMI_OK) {
        result = FOR_EACH_THREAD_RES_CONTINUE;
    } else {
        result = FOR_EACH_THREAD_RES_BREAK;
    }

    return result;
}

memmi_Status memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
    memmi_BreakpointCondition condition, uint32_t index, memmi_BreakpointLength length)
{
    memmi_Status result = 0;

    memmi_PID pid = get_platform_process_handle(process)->pid;
    pid_t native_pid = (pid_t)pid.value;
    memmi_Status pid_exists_result = pid_exists(native_pid);

    if (pid_exists_result != MEMMI_OK) {
        result = pid_exists_result;
    } else {
        if (index > 3) {
            result = MEMMI_INVALID_ARGUMENTS;
        } else {
            memmi_Status main_thread_bp_result = set_hardware_breakpoint_on_thread(
                native_pid, index, address, condition, length);

            if (main_thread_bp_result != MEMMI_OK) {
                result = main_thread_bp_result;
            } else {
                HardwareBreakpointContext context = {0};
                context.index = index;
                context.address = address;
                context.condition = condition;
                context.length = length;

                memmi_Status for_each_result = for_each_thread(native_pid, &context, set_hardware_breakpoint_on_thread_cb);

                if (for_each_result != MEMMI_OK) {
                    result = for_each_result;
                } else {
                    // If setting a breakpoint on a child thread died due to that thread dying due to a
                    // race, we'll ignore it. Any other error we'll report.
                    uint32_t statuses_excluding_no_such_process =
                        context.statuses & ~(uint32_t)MEMMI_NO_SUCH_PROCESS;

                    result = statuses_excluding_no_such_process;
                }
            }
        }
    }

    return result;
}

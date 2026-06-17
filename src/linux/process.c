#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

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

                    if (is_number(dir_name)) {
                        ProcessName proc_name = get_process_name(subdir_fd, allocator);

                        if (proc_name.ok) {
                            int64_t pid_value = str_to_int64(dir_name);

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

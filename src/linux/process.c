#define _GNU_SOURCE
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "process.h"

#include <stdbool.h>

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

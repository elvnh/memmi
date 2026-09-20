#if defined(_MSC_VER)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_utils.h"
#include "ipc/ipc_all.c"

#include <string.h>

#define ARRAY_COUNT(a) (sizeof((a)) / sizeof(*(a)))

#if !defined(DEBUGGEE_EXECUTABLE_NAME)
#    error Please define the filename of the debuggee executable in the build script.
#endif

typedef struct {
    Pid   pid;
    int   return_code;
    char *output;
} Subprocess;

typedef enum {
    SUBPROC_SYNC,
    SUBPROC_ASYNC,
} SubprocessKind;

Subprocess  subprocess_run(const char *exe, char *args[], size_t arg_count, SubprocessKind kind);
void        subprocess_destroy(Subprocess subproc);
char       *get_debuggee_path();

int main(int argc, char **argv)
{
    uint64_t tests_ran = 0;

    uint64_t assertions_passed = 0;
    uint64_t assertions_ran = 0;

    // TODO: don't hardcode path
    char *debuggee_path = get_debuggee_path();

    for (int i = 1; i < argc; ++i) {
        // First launch the debuggee asynchronously. It will wait for the debugger (the test
        // case we launch later) to connect to it.
        Subprocess debuggee_subproc = subprocess_run(
            debuggee_path, 0, 0, SUBPROC_ASYNC);

        char pid_str[64] = {0};
        snprintf(pid_str, sizeof(pid_str), "%" PRId64, debuggee_subproc.pid);

        // Launch the test case and wait for it to finish. Pass the pid of the debuggee process to
        // it so it can connect to it and start interacting with it.
        char *test_case_path = argv[i];
        char *test_case_args[] = {pid_str};

        Subprocess test_case_subproc = subprocess_run(
            test_case_path, test_case_args, ARRAY_COUNT(test_case_args), SUBPROC_SYNC);

        uint32_t assertions_passed_in_test = 0;
        uint32_t assertions_ran_in_test = 0;

        // Parse the output of the test to see how many assertions were passed and ran.
        int scan_result = sscanf(
            test_case_subproc.output,
            IPC_TEST_OUTPUT_FMT_STRING,
            &assertions_passed_in_test,
            &assertions_ran_in_test);

        if (scan_result == 2) {
            assertions_passed += assertions_passed_in_test;
            assertions_ran += assertions_ran_in_test;
        } else {
            fprintf(stderr, "Warning: test case '%s' did not have expected test result output.\n",
                   test_case_path);
        }

        ++tests_ran;

        subprocess_destroy(debuggee_subproc);
        subprocess_destroy(test_case_subproc);
    }

    printf("Passed %" PRIu64 "/%" PRIu64 " assertions in %" PRIu64 " test cases.\n",
        assertions_passed, assertions_ran, tests_ran);

    return 0;
}

#if OS_LINUX
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

Subprocess subprocess_run(const char *exe, char *args[], size_t arg_count, SubprocessKind kind)
{
    Subprocess result = {0};

    int pipes[2];
    int pipe_result = pipe(pipes);
    assert(pipe_result != -1);

    pid_t child_pid = fork();
    assert(child_pid != -1);

    result.pid = child_pid;

    if (child_pid == 0) {
        int dup_res = dup2(pipes[PIPE_WRITE_END], STDOUT_FILENO);
        assert(dup_res != -1);

        // We won't use the pipe file descriptors directly in the child process.
        close(pipes[PIPE_WRITE_END]);
        close(pipes[PIPE_READ_END]);

        // Copy arguments to new array containing executable name and null terminator.
        size_t final_args_count = arg_count + 2;
        char **final_args = calloc(final_args_count, sizeof(char *));
        final_args[0] = (char *)exe;
        final_args[final_args_count - 1] = 0;

        if (arg_count > 0) {
            memcpy(final_args + 1, args, arg_count * sizeof(char *));
        }

        int exec_result = execv(exe, final_args);
        assert(exec_result != -1);
    } else {
        if (kind == SUBPROC_SYNC) {
            int status = 0;
            int wait_result = waitpid(child_pid, &status, 0);
            assert(wait_result != -1);

            if (WIFEXITED(status)) {
                result.return_code = WEXITSTATUS(status);
            } else {
                fprintf(stderr, "Warning: test case '%s' exited unexpectedly.\n", exe);
            }

            // Since we're waiting until after waitpid to read from the pipe, we should always read the
            // entirety of the stdout/stderr of the child process in one call to read(), provided that
            // the buffer is large enough. Since we only print very little from the child process, it
            // should always be large enough.
            size_t buffer_size = 1024;
            result.output = calloc(buffer_size, sizeof(char));

            struct pollfd poll_fd = {0};
            poll_fd.fd = pipes[PIPE_READ_END];
            poll_fd.events = POLLIN;

            int poll_result = poll(&poll_fd, 1, 0);

            if (poll_result > 0) {
                ssize_t bytes_read = read(pipes[PIPE_READ_END], result.output, buffer_size);

                assert(bytes_read > 0);
                assert((size_t)bytes_read < buffer_size);
            }
        }

        // Pipe no longer needed, close it.
        close(pipes[PIPE_WRITE_END]);
        close(pipes[PIPE_READ_END]);
    }

    return result;
}

void subprocess_destroy(Subprocess subproc)
{
    kill(subproc.pid, SIGKILL);
    free(subproc.output);
}

char *get_debuggee_path()
{
    char *self_path = realpath("/proc/self/exe", 0);
    assert(self_path);

    ssize_t last_slash_index = strlen(self_path) - 1;

    while ((last_slash_index >= 0) && (self_path[last_slash_index] != '/')) {
        --last_slash_index;
    }

    assert(last_slash_index >= 0);

    size_t final_length = (last_slash_index + 1) + sizeof(DEBUGGEE_EXECUTABLE_NAME);
    char *result = realloc(self_path, final_length);

    strcpy(result + last_slash_index + 1, DEBUGGEE_EXECUTABLE_NAME);

    return result;
}

#elif OS_WIN32
#include <psapi.h>
static char *create_command_line(const char *exe, char *args[], size_t arg_count)
{
    size_t total_length = 0;
    total_length += strlen(exe) + 1;

    for (size_t i = 0; i < arg_count; ++i) {
        total_length += strlen(args[i]) + 1;
    }

    ++total_length;

    char *result = calloc(total_length, sizeof(char));

    size_t offset = 0;

    strcpy(result + offset, exe);
    offset += strlen(exe);
    result[offset++] = ' ';

    for (size_t i = 0; i < arg_count; ++i) {
        strcpy(result + offset, args[i]);
        offset += strlen(args[i]);
        result[offset++] = ' ';
    }

    return result;
}

Subprocess subprocess_run(const char *exe, char *args[], size_t arg_count, SubprocessKind kind)
{
    Subprocess result = {0};

    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    // TODO: better names
    HANDLE write_pipe = 0;
    HANDLE read_pipe = 0;

    BOOL create_pipe_result = CreatePipe(
        &read_pipe, &write_pipe, &security_attributes, 0);
    assert(create_pipe_result);

    BOOL set_handle_info_result = SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    assert(set_handle_info_result);

    PROCESS_INFORMATION proc_info = {0};
    STARTUPINFO startup_info = {0};
    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.hStdOutput = write_pipe;
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE); // Don't redirect stderr
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    char *cmd_line = create_command_line(exe, args, arg_count);

    BOOL create_proc_result = CreateProcess(
        0,
        cmd_line,
        0,
        0,
        TRUE,
        0,
        0,
        0,
        &startup_info,
        &proc_info
    );

    assert(create_proc_result);

    // TODO: close pipes
    if (kind == SUBPROC_SYNC) {
        DWORD wait_result = WaitForSingleObject(proc_info.hProcess, INFINITE);
        assert(wait_result == WAIT_OBJECT_0);

        DWORD return_code = 0;
        BOOL get_exit_code_result = GetExitCodeProcess(proc_info.hProcess, &return_code);
        assert(get_exit_code_result);

        result.return_code = (int)return_code;

        // Since we're waiting until after waitpid to read from the pipe, we should always read the
        // entirety of the stdout/stderr of the child process in one call to read(), provided that
        // the buffer is large enough. Since we only print very little from the child process, it
        // should always be large enough.
        DWORD buffer_size = 1024;
        result.output = calloc(buffer_size, sizeof(char));

        DWORD bytes_read = 0;

        BOOL read_result = ReadFile(read_pipe, result.output, buffer_size, &bytes_read, 0);
        assert(read_result);
    }

    result.pid = (Pid)proc_info.dwProcessId;

    free(cmd_line);

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);

    return result;
}

void subprocess_destroy(Subprocess subproc)
{
    HANDLE proc_handle = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)subproc.pid);
    TerminateProcess(proc_handle, 0);
    CloseHandle(proc_handle);

    free(subproc.output);
}

char *get_debuggee_path()
{
    // TODO: Code duplication between this and Linux version
    DWORD self_path_length = MAX_PATH;
    char *self_path = calloc(self_path_length, sizeof(char));

    HANDLE self_handle = GetCurrentProcess();

    BOOL query_name_result = QueryFullProcessImageNameA(
        self_handle,
        0,
        self_path,
        &self_path_length
    );

    assert(query_name_result);

    int32_t last_slash_index = self_path_length - 1;

    while ((last_slash_index >= 0) && (self_path[last_slash_index] != '\\')) {
        --last_slash_index;
    }

    assert(last_slash_index >= 0);

    size_t final_length = (last_slash_index + 1) + sizeof(DEBUGGEE_EXECUTABLE_NAME);
    char *result = realloc(self_path, final_length);
    strcpy(result + last_slash_index + 1, DEBUGGEE_EXECUTABLE_NAME);

    return result;
}

#endif

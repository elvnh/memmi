#include "ipc/ipc_all.c"

#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>

typedef struct {
    Pid   pid;
    int   return_code;
    char *output; // TODO: rename to stdout
} Subprocess;

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

typedef enum {
    SUBPROC_SYNC,
    SUBPROC_ASYNC,
} SubprocessKind;

// TODO: move these to the bottom
// TODO: make this function a bit nicer to use, automatically provide exe name as first arg
Subprocess subprocess_run(const char *exe, char *argv[], SubprocessKind kind)
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

        execv(exe, argv);
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

int main(int argc, char **argv)
{
    uint64_t tests_ran = 0;

    uint64_t assertions_passed = 0;
    uint64_t assertions_ran = 0;

    // TODO: don't hardcode path
    char *debuggee_path = "./build/debuggee";
    char *debuggee_args[] = {debuggee_path, 0};

    for (int i = 1; i < argc; ++i) {
        // First launch the debuggee asynchronously. It will wait for the debugger (the test
        // case we launch later) to connect to it.
        Subprocess debuggee_subproc = subprocess_run(debuggee_path, debuggee_args, SUBPROC_ASYNC);
        char pid_str[64] = {0};
        snprintf(pid_str, sizeof(pid_str), "%ld", debuggee_subproc.pid);

        // Launch the test case and wait for it to finish. Pass the pid of the debuggee process to
        // it so it can connect to it and start interacting with it.
        // TODO: rename
        char *test_case_path = argv[i];
        char *test_case_args[] = {test_case_path, pid_str, 0};
        Subprocess test_case_subproc = subprocess_run(test_case_path, test_case_args, SUBPROC_SYNC);

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

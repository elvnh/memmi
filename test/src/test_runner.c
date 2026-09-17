#include <unistd.h>
#include <sys/wait.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "test_case.h"

typedef struct {
    int return_code;
    char *output;
} Subprocess;

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

Subprocess subprocess_run(const char *exe, char *argv[])
{
    Subprocess result = {0};

    int pipes[2];
    int pipe_result = pipe(pipes);
    assert(pipe_result != -1);

    pid_t child_pid = fork();
    assert(child_pid != -1);

    if (child_pid == 0) {
        int dup_res = dup2(pipes[PIPE_WRITE_END], STDOUT_FILENO);
        assert(dup_res != -1);

        // We won't use the pipe file descriptors directly in the child process.
        close(pipes[PIPE_WRITE_END]);
        close(pipes[PIPE_READ_END]);

        execv(exe, argv);
    } else {
        int status = 0;
        int wait_result = waitpid(child_pid, &status, 0);
        assert(wait_result != -1);
        assert(WIFEXITED(status));

        result.return_code = WEXITSTATUS(status);

        // Since we're waiting until after waitpid to read from the pipe, we should always read the
        // entirety of the stdout/stderr of the child process in one call to read(), provided that
        // the buffer is large enough. Since we only print very little from the child process, it
        // should always be large enough.
        size_t buffer_size = 1024;
        result.output = calloc(buffer_size, sizeof(char));

        ssize_t bytes_read = read(pipes[PIPE_READ_END], result.output, buffer_size);

        assert(bytes_read > 0);
        assert((size_t)bytes_read < buffer_size);

        // Pipe no longer needed, close it.
        close(pipes[PIPE_WRITE_END]);
        close(pipes[PIPE_READ_END]);
    }

    return result;
}

void subprocess_free(Subprocess subproc)
{
    free(subproc.output);
}

int main(int argc, char **argv)
{
    assert(argc > 1);

    uint64_t tests_ran = 0;

    uint64_t assertions_passed = 0;
    uint64_t assertions_ran = 0;

    for (int i = 1; i < argc; ++i) {
        char *test_path = argv[i];

        char *subproc_args[] = {test_path, 0};
        Subprocess subproc = subprocess_run(test_path, subproc_args);
        assert(subproc.return_code == 0);

        uint64_t assertions_passed_in_test = 0;
        uint64_t assertions_ran_in_test = 0;

        // Parse the output of the test.
        int scan_result = sscanf(
            subproc.output,
            TEST_OUTPUT_FMT_STRING,
            &assertions_passed_in_test,
            &assertions_ran_in_test);

        assert(scan_result == 2);

        subprocess_free(subproc);

        ++tests_ran;

        assertions_passed += assertions_passed_in_test;
        assertions_ran += assertions_ran_in_test;
    }

    printf("Passed %" PRIu64 "/%" PRIu64 " assertions in %" PRIu64 " test cases.\n",
        assertions_passed, assertions_ran, tests_ran);

    return 0;
}

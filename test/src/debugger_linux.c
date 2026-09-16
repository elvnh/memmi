static bool spawn_debuggee_process(const char *path, Pid *pid)
{
    assert(pid);

    bool result = false;

    pid_t fork_result = fork();

    if (fork_result == -1) {
        result = false;
    } else if (fork_result == 0) {
        char *args[] = {(char *)path, 0};
        execv(path, args);
    } else {
        *pid = (Pid)fork_result;
        result = true;
    }


    return result;
}

static bool spawn_debuggee_process(Pid *pid)
{
    assert(pid);

    bool result = false;

    pid_t fork_result = fork();

    if (fork_result == -1) {
        result = false;
    } else if (fork_result == 0) {
        char *args[] = {(char *)DEBUGGEE_EXECUTABLE_PATH, 0};
        execv(DEBUGGEE_EXECUTABLE_PATH, args);
    } else {
        *pid = (Pid)fork_result;
        result = true;
    }


    return result;
}

static char *lnx_get_debuggee_path()
{
    char *self_path = realpath("/proc/self/exe", 0);
    assert(self_path);

    ssize_t last_slash_index = strlen(self_path) - 1;

    while ((last_slash_index >= 0) && (self_path[last_slash_index] != '/')) {
        --last_slash_index;
    }

    assert(last_slash_index >= 0);

    char s[] = "../" DEBUGGEE_EXECUTABLE_NAME;
    size_t final_length = (last_slash_index + 1) + sizeof(s);
    char *result = realloc(self_path, final_length);

    strcpy(result + last_slash_index + 1, s);

    return result;
}

static bool spawn_debuggee_process(Pid *pid)
{
    assert(pid);

    bool result = false;

    pid_t fork_result = fork();

    if (fork_result == -1) {
        result = false;
    } else if (fork_result == 0) {
        char *debuggee_path = lnx_get_debuggee_path();
        char *args[] = {debuggee_path, 0};

        execv(debuggee_path, args);
    } else {
        *pid = (Pid)fork_result;
        result = true;
    }


    return result;
}

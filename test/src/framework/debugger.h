#pragma once

typedef struct {
    Pid pid;
    Ipc ipc;
} Debuggee;

Debuggee launch_debuggee(const char *path);
void     destroy_debuggee(Debuggee debuggee);
Response send_command_with_timeout(Debuggee debuggee, Command command, uint32_t timeout_ms);
Response send_command(Debuggee debuggee, Command command);

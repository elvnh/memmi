#pragma once

Ipc      launch_debuggee(const char *path);
Response send_command_with_timeout(Ipc ipc, Command command, uint32_t timeout_ms);

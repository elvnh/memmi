#pragma once

// TODO: this file needs a better name

#define IPC_TEST_OUTPUT_FMT_STRING "%" PRIu32 "/%" PRIu32 "\n"
#define IPC_TEST_PORT 8080

typedef int64_t Pid;

typedef struct {
    Pid pid;
    Ipc ipc;
} Debuggee;

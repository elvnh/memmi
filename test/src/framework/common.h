#pragma once

#define TEST_IPC_PORT 8080

typedef int64_t Pid;

/* Messages - used to wrap Responses and Commands when sending them between the processes */
typedef union {
    Command  command;
    Response response;
} Message;

static inline Message msg_command(Command command)
{
    Message result = {0};
    result.command = command;

    return result;
}

static inline Message msg_response(Response response)
{
    Message result = {0};
    result.response = response;

    return result;
}

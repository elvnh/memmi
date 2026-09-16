#pragma once

#define TEST_IPC_PORT 8080

/* Commands - sent from the debugger to the debuggee to make it perform an action */
typedef enum {
    CMD_DO_NOTHING, /* Used to check if debuggee is alive */
} CommandKind;

typedef struct {
    CommandKind kind;
} Command;

/* Responses - sent back from the debuggee to the debugger as a response to a command */
typedef enum {
    /* Responses sent by the other process */
    RES_ACK,

    /* Errors that can occur when receiving response */
    RES_ERROR,
    RES_EXITED,
    RES_TIMEOUT,
} ResponseKind;

typedef struct {
    ResponseKind kind;
} Response;

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

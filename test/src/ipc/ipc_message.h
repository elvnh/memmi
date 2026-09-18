#pragma once

/* Commands - sent from the debugger to the debuggee to make it perform an action */
typedef enum {
    CMD_DO_NOTHING, /* Used to check if debuggee is alive */
    CMD_GET_NEW_VARIABLE, // TODO: rename to DECLARE_NEW_VARIABLE
    CMD_GET_VARIABLE,
} CommandKind;

typedef struct {
    CommandKind kind;

    union {
        TypedValue get_new_variable;
        VariableId get_variable;
    } as;
} Command;

static inline Command cmd_do_nothing()
{
    Command result = {0};
    result.kind = CMD_DO_NOTHING;

    return result;
}

static inline Command cmd_get_new_variable(TypedValue value)
{
    Command result = {0};
    result.kind = CMD_GET_NEW_VARIABLE;
    result.as.get_new_variable = value;

    return result;
}

static inline Command cmd_get_variable(VariableId id)
{
    Command result = {0};
    result.kind = CMD_GET_VARIABLE;
    result.as.get_variable = id;

    return result;
}


/* Responses - sent back from the debuggee to the debugger as a response to a command */
typedef enum {
    /* Responses sent by the other process */
    RES_ACK,
    RES_VARIABLE_INFO,

    /* Errors that can occur when receiving response */
    RES_ERROR,
    RES_EXITED,
    RES_TIMEOUT,
} ResponseKind;

typedef struct {
    ResponseKind kind;

    union {
        VariableInfo variable_info;
    } as;
} Response;

static inline Response res_ack()
{
    Response result = {0};
    result.kind = RES_ACK;

    return result;
}

static inline Response res_variable_info(VariableInfo info)
{
    Response result = {0};
    result.kind = RES_VARIABLE_INFO;
    result.as.variable_info = info;

    return result;
}

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

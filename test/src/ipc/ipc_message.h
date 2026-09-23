#pragma once

/* Commands - sent from the debugger to the debuggee to make it perform an action */
typedef enum {
    CMD_DO_NOTHING, /* Used to check if debuggee is alive */
    CMD_DECLARE_VARIABLE,
    CMD_GET_VARIABLE_BY_ID,
    CMD_MAP_NEW_MEMORY,
    CMD_MALLOC,
    CMD_EXIT_PROCESS,
} CommandKind;

typedef struct {
    CommandKind kind;

    union {
        TypedValue declare_variable;
        VariableId get_variable_by_id;
        size_t     allocation_size;
        int32_t    exit_process_with_code;
    } as;
} Command;

static inline Command cmd_do_nothing()
{
    Command result = {0};
    result.kind = CMD_DO_NOTHING;

    return result;
}

static inline Command cmd_declare_variable(TypedValue value)
{
    Command result = {0};
    result.kind = CMD_DECLARE_VARIABLE;
    result.as.declare_variable = value;

    return result;
}

static inline Command cmd_get_variable(VariableId id)
{
    Command result = {0};
    result.kind = CMD_GET_VARIABLE_BY_ID;
    result.as.get_variable_by_id = id;

    return result;
}

static inline Command cmd_map_new_memory(size_t size)
{
    Command result = {0};
    result.kind = CMD_MAP_NEW_MEMORY;
    result.as.allocation_size = size;

    return result;
}

static inline Command cmd_malloc(size_t size)
{
    Command result = {0};
    result.kind = CMD_MALLOC;
    result.as.allocation_size = size;

    return result;
}

static inline Command cmd_exit_process(int32_t exit_code)
{
    Command result = {0};
    result.kind = CMD_EXIT_PROCESS;
    result.as.exit_process_with_code = exit_code;

    return result;
}

/* Responses - sent back from the debuggee to the debugger as a response to a command */
typedef enum {
    /* Responses sent by the other process */
    RES_ACK,
    RES_VARIABLE_INFO,
    RES_DYNAMIC_MEMORY_ALLOCATION,

    /* Errors that can occur when receiving response */
    RES_ERROR,
    RES_EXITED,
    RES_TIMEOUT,
} ResponseKind;

typedef struct {
    ResponseKind kind;

    union {
        VariableInfo variable_info;
        uintptr_t    dynamic_allocation_address;
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

static inline Response res_memory_allocation(uintptr_t address)
{
    Response result = {0};
    result.kind = RES_DYNAMIC_MEMORY_ALLOCATION;
    result.as.dynamic_allocation_address = address;

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

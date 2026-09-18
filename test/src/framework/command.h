#pragma once

#include "variable.h"

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

#pragma once

/* Commands - sent from the debugger to the debuggee to make it perform an action */
typedef enum {
    CMD_DO_NOTHING, /* Used to check if debuggee is alive */
} CommandKind;

typedef struct {
    CommandKind kind;
} Command;

static inline Command cmd_do_nothing()
{
    Command result = {0};
    result.kind = CMD_DO_NOTHING;

    return result;
}

#pragma once

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

static inline Response res_ack()
{
    Response result = {0};
    result.kind = RES_ACK;

    return result;
}

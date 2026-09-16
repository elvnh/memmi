#pragma once

#define TEST_IPC_PORT 8080

typedef enum {
    CMD_DO_NOTHING,
} CommandKind;

typedef struct {
    CommandKind kind;
} Command;

typedef enum {
    // Responses sent by the other process
    RES_ACK,

    // Error than can occur when receiving response
    RES_ERROR,
    RES_EXITED,
    RES_TIMEOUT,
} ResponseKind;

typedef struct {
    ResponseKind kind;
} Response;

typedef union {
    Command  command;
    Response response;
} Message;

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "allocator.h"
#include "string8.h"

typedef struct {
    int64_t value;
} memmi_PID;

typedef struct {
    void *data;
} memmi_Process;

typedef enum {
    MEMMI_OPEN_PROC_NO_SUCH_PID,
    MEMMI_OPEN_PROC_ALLOCATION_FAILED,
    MEMMI_OPEN_PROC_OK,
} memmi_OpenProcStatus;

typedef struct {
    memmi_OpenProcStatus  status;
    memmi_Process         process;
} memmi_OpenProcess;

typedef struct {
    memmi_String name;
    memmi_PID pid;
} memmi_ProcessInfo;

// TODO: more descriptive errors
typedef enum {
    MEMMI_GET_PROCS_FAIL,
    MEMMI_GET_PROCS_OK,
} memmi_GetProcsStatus;

typedef struct {
    memmi_GetProcsStatus  status;
    memmi_ProcessInfo    *data;
    size_t                count;
} memmi_ProcessList;

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator);
memmi_OpenProcess memmi_open_process(memmi_PID pid, memmi_Allocator allocator);
void              memmi_close_process(memmi_Process process, memmi_Allocator allocator);

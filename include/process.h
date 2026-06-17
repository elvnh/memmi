#pragma once

#include <stddef.h>
#include <stdint.h>

#include "allocator.h"
#include "string8.h"

typedef struct {
    int64_t value;
} memmi_PID;

typedef struct {
    memmi_String name;
    memmi_PID pid;
} memmi_ProcessInfo;

typedef struct {
    // TODO: status
    memmi_ProcessInfo *data;
    size_t count;
} memmi_ProcessList;

memmi_ProcessList memmi_get_running_processes(memmi_Allocator allocator);

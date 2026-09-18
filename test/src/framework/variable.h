#pragma once

#include <stdint.h>
#include <stddef.h>

typedef size_t VariableId;

typedef enum {
    VAR_TYPE_INT32,
} VariableType;

typedef union {
    int32_t int32;
} Value;

typedef struct {
    VariableType type;
    Value        value;
} TypedValue;

static inline TypedValue val_int32(int32_t val)
{
    TypedValue result = {0};
    result.type = VAR_TYPE_INT32;
    result.value.int32 = val;

    return result;
}

typedef struct {
    VariableId    id;
    uintptr_t     address;
    Value         value;
    VariableType  type;
} VariableInfo;

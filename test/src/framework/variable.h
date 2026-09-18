#pragma once

#include <stdint.h>
#include <stddef.h>

typedef size_t VariableId;

typedef enum {
    VAL_TYPE_INT32,
} ValueType;

typedef union {
    int32_t int32;
} Value;

typedef struct {
    ValueType type;
    Value        value;
} TypedValue;

static inline TypedValue val_int32(int32_t val)
{
    TypedValue result = {0};
    result.type = VAL_TYPE_INT32;
    result.value.int32 = val;

    return result;
}

typedef struct {
    VariableId    id;
    uintptr_t     address;
    Value         value;
    ValueType     type;
} VariableInfo;

#pragma once

#include <stddef.h>

typedef void *(*memmi_AllocateFn)(void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align);

typedef struct {
    void *context;
    memmi_AllocateFn function;
} memmi_Allocator;

memmi_Allocator memmi_default_allocator(void);

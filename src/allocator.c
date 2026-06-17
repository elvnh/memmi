#include "allocator.h"

#include <stdlib.h>

#include "utils.h"

static void *memmi_default_allocate(void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    (void)ctx;
    (void)old_size;
    (void)align;

    ASSERT(ptr || (old_size == 0));

    // TODO: get rid of libc

    void *result = realloc(ptr, new_size);

    return result;
}

memmi_Allocator memmi_default_allocator()
{
    memmi_Allocator result = {0, memmi_default_allocate};

    return result;
}

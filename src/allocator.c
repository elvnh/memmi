#include "allocator.h"

#include <stdlib.h>

#include "utils.h"

static void *memmi_default_allocate(void *ctx, void *ptr, size_t old_count, size_t new_count, size_t item_size, size_t align)
{
    (void)ctx;
    (void)old_count;
    (void)align;

    void *result = 0;

    // TODO: get rid of libc?
    MaybeUsize new_size = safe_mul_usize(new_count, item_size);

    if (new_size.ok) {
        result = realloc(ptr, new_size.value);
    }

    return result;
}

memmi_Allocator memmi_default_allocator()
{
    memmi_Allocator result = {0, memmi_default_allocate};

    return result;
}

#pragma once

#include <stdbool.h>
#include <assert.h>

#include "allocator.h"
#include "string8.h"

#define allocate(a, t, count) (a).function((a).context, 0, 0, sizeof(t) * (count), ALIGNOF(t))
#define reallocate(a, ptr, old, new) (a).function((a).context, (ptr), (old), (new), ALIGNOF(*(ptr)))
#define deallocate(a, ptr, size) (a).function((a).context, (ptr), (size), 0, 0)

#if defined(__GNUC__)
#    define ALIGNOF(t) __alignof__(t)
#else
#    error Unsupported compiler
#endif

#define ASSERT(e) assert(e)
#define ARRAY_COUNT(arr) (sizeof((arr)) / sizeof(*(arr)))

static inline bool is_digit(char ch)
{
    bool result = (ch >= '0') && (ch <= '9');

    return result;
}

static inline bool is_number(memmi_String str)
{
    bool result = str.count > 0;

    for (size_t i = 0; i < str.count; ++i) {
        if (!is_digit(str.data[i])) {
            result = false;
            break;
        }
    }

    return result;
}

memmi_String str_from_c_str(char *str);

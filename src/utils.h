#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "allocator.h"
#include "string8.h"

#define allocate(a, t, count) (t *)((a).function((a).context, 0, 0, (count) * sizeof(t), ALIGNOF(t)))
#define reallocate(a, ptr, old, new) (a).function((a).context, (ptr), (old) * sizeof(*(ptr)), \
        (new) * sizeof(*(ptr)), ALIGNOF(*(ptr)))
#define deallocate(a, ptr, size) (a).function((a).context, (ptr), (size) * sizeof(*(ptr)), 0, 0)

#if defined(__x86_64__)
#    define DEBUG_BREAK __asm volatile("int3")
#else
#    error Unsupported architecture
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define BIT(e) (1u << (e))

#define dyn_arr_push(arr, item, alloc)                                  \
do {                                                                    \
    if ((arr)->count == (arr)->capacity) {                              \
        (arr)->capacity = MAX((arr)->capacity, 32);                     \
        (arr)->data = reallocate((alloc), (arr)->data,                  \
            (arr)->capacity, (arr)->capacity * 2);                      \
        (arr)->capacity *= 2;                                           \
    }                                                                   \
    (arr)->data[((arr)->count)++] = (item);                             \
} while (0)

#if defined(__GNUC__)
#    define ALIGNOF(t) __alignof__(t)
#else
#    error Unsupported compiler
#endif

#define ASSERT(e) assert(e)
#define ARRAY_COUNT(arr) (sizeof((arr)) / sizeof(*(arr)))

typedef struct {
    memmi_String head;
    memmi_String tail;
    bool   ok;
} Cut;

static inline bool is_digit(char ch)
{
    bool result = (ch >= '0') && (ch <= '9');

    return result;
}

static inline bool is_alpha(char ch)
{
    bool result = ((ch >= 'a') && (ch <= 'z'))
        || ((ch >= 'A') && (ch <= 'Z'));

    return result;
}

static inline bool is_hex(char ch)
{
    bool result = ((ch >= 'a') && (ch <= 'f'))
        || ((ch >= 'A') && (ch <= 'F'));

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

static inline bool is_whitespace(char ch)
{
    bool result = (ch == ' ') || (ch == '\n') || (ch == '\t') || (ch == '\r');

    return result;
}


#define str_lit(s) (memmi_String) { s, ARRAY_COUNT(s) - 1 }

typedef enum {
    NUM_BASE_DEC = 10,
    NUM_BASE_HEX = 16,
} NumberBase;

typedef struct {
    int64_t value;
    bool ok;
} MaybeS64;

typedef struct {
    uint64_t value;
    bool ok;
} MaybeU64;

memmi_String   str_from_c_str(char *str);
bool           str_starts_with(memmi_String str, memmi_String substr);
Cut            str_cut(memmi_String str, memmi_String pattern);
memmi_String   str_trim_leading_whitespace(memmi_String str);
memmi_String   str_trim_trailing_whitespace(memmi_String str);
memmi_String   str_trim_whitespace(memmi_String str);
MaybeS64       str_to_s64(memmi_String str, NumberBase base);
MaybeU64       str_to_u64(memmi_String str, NumberBase base);

MaybeS64       safe_add_s64(int64_t a, int64_t b);
MaybeU64       safe_add_u64(uint64_t a, uint64_t b);
MaybeS64       safe_mul_s64(int64_t a, int64_t b);
MaybeU64       safe_mul_u64(uint64_t a, uint64_t b);

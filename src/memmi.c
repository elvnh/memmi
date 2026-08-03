#if defined(__linux__)
#    define MEMMI_LINUX
#elif defined(_WIN32)
#    define MEMMI_WINDOWS
#else
#    error Unsupported operating system
#endif

#if defined(__GNUC__)
#    define MEMMI_GCC
#elif defined(_MSC_VER)
#    define MEMMI_MSVC
#else
#    error Unsupported compiler
#endif

#ifdef MEMMI_LINUX
#    define _GNU_SOURCE
#endif

#include "memmi.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/*
  TODO:
  - Undefine all macros at end of file
 */

/***************************/
/*    General utilities    */
/***************************/
#define allocate(a, t, count) (t *)((a).function((a).context, 0, 0, (count), sizeof(t), ALIGNOF(t)))
#define reallocate(a, ptr, old_count, new_count) (a).function((a).context, (ptr), (old_count), \
        (new_count), sizeof(*(ptr)), ALIGNOF(TYPEOF(*(ptr))))
#define deallocate(a, ptr, count) (a).function((a).context, (ptr), (count), 0, sizeof(*(ptr)), ALIGNOF(*(ptr)))
#define str_from_span(span) (memmi_String) {(span).data, (span).count}
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_COUNT(arr) (sizeof((arr)) / sizeof(*(arr)))

#define ASSERT(e) do {                                      \
        if (!(e)) {                                         \
            fprintf(stderr, "\n*** ASSERTION FAILED ***\n"  \
                "Expression: '%s'\nFunction: %s\n%s:%d:\n", \
                #e, __func__, __FILE_NAME__, __LINE__);     \
            DEBUG_BREAK;                                    \
        }                                                   \
    } while (0)

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

#define sl_push_back(list, node)                \
    do {                                        \
        if ((list)->last) {                     \
            (list)->last->next = (node);        \
        }                                       \
        (list)->last = (node);                  \
                                                \
        if (!(list)->first) {                   \
            (list)->first = (node);             \
        }                                       \
                                                \
    } while (0)

// TODO: collapse all these into one ifdef
#if defined(MEMMI_GCC)
#    define DEBUG_BREAK __builtin_trap()
#elif defined(MEMMI_MSVC)
#    define DEBUG_BREAK __debugbreak()
#else
#    error DEBUG_BREAK not defined for this compiler
#endif

#if defined(MEMMI_GCC)
#    define ALIGNOF(t) __alignof__(t)
#elif defined(MEMMI_MSVC)
#    define ALIGNOF(t) __alignof(t)
#else
#    error ALIGNOF not defined for this compiler
#endif

#if defined(MEMMI_GCC)
#    error TODO
#elif defined(MEMMI_MSVC)
#    define TYPEOF(t) __typeof__(t)
#else
#    error TYPEOF not defined for this compiler
#endif

// TODO: move closer to function definition
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

typedef struct {
    size_t value;
    bool ok;
} MaybeUsize;

/***************************/
/*         String          */
/***************************/
#define str_lit(s) (memmi_String) { s, ARRAY_COUNT(s) - 1 }

memmi_String str_from_c_str(char *str)
{
    ASSERT(str);
    size_t length = strlen(str);

    memmi_String result = {str, length};

    return result;
}

bool str_eq(memmi_String a, memmi_String b)
{
    bool result = false;

    if (a.count == b.count) {
        result = (!a.data && !b.data) || (memcmp(a.data, b.data, a.count) == 0);
    }

    return result;
}

bool str_starts_with(memmi_String str, memmi_String substr)
{
    bool result = false;

    if (str.data && (substr.count <= str.count)) {
        memmi_String start = {str.data, substr.count};
        result = str_eq(start, substr);
    }

    return result;
}

Cut str_cut(memmi_String str, memmi_String pattern)
{
    Cut result = {0};
    result.head = str;

    if (pattern.count <= str.count) {
        size_t end = str.count - pattern.count;

        size_t index;
        for (index = 0; index <= end; ++index) {
            memmi_String substr = {str.data + index, pattern.count};

            if (str_eq(substr, pattern)) {
                result.head = (memmi_String) {str.data, index};
                result.tail = (memmi_String) {str.data + index + pattern.count, str.count - index - pattern.count};
                result.ok = true;

                break;
            }
        }
    }

    return result;
}

memmi_String str_trim_leading_whitespace(memmi_String str)
{
    memmi_String result = str;

    while ((result.count > 0) && is_whitespace(*result.data)) {
        ++result.data;
        --result.count;
    }

    return result;
}

memmi_String str_trim_trailing_whitespace(memmi_String str)
{
    memmi_String result = str;

    while ((result.count > 0) && is_whitespace(result.data[result.count - 1])) {
        --result.count;
    }

    return result;
}

memmi_String str_trim_whitespace(memmi_String str)
{
    memmi_String result = str;
    result = str_trim_leading_whitespace(result);
    result = str_trim_trailing_whitespace(result);

    return result;
}

/***************************/
/*      Safe arithmetic    */
/***************************/
// TODO: define these for other compilers
#if defined(MEMMI_GCC)
#    define SAFE_ADD_S64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_ADD_U64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_S64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_U64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_USIZE(a, b, result_ptr) !__builtin_mul_overflow((a), (b), (result_ptr))
#elif defined(MEMMI_MSVC)
#    define SAFE_ADD_S64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_ADD_U64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_S64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_U64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_USIZE(a, b, result_ptr) !__builtin_mul_overflow((a), (b), (result_ptr))
// Thanks MSVC, I'll do it myself.
// TODO: use macros to generate the various types of these
// TODO: the implementations of these are so simple that maybe
// we should just use them for all compilers. They aren't used in any performance
// critical paths anyways.

static bool safe_add_s64_impl(int64_t a, int64_t b, int64_t *out)
{
    bool result = false;

    if (a <= (INT64_MAX - b)) {
        result = true;
        *out = a + b;
    }

    return result;
}

static bool safe_add_u64_impl(uint64_t a, uint64_t b, uint64_t *out)
{
    bool result = false;

    if (a <= (UINT64_MAX - b)) {
        result = true;
        *out = a + b;
    }

    return result;
}

static bool safe_mul_s64_impl(int64_t a, int64_t b, int64_t *out)
{
    bool result = false;

    if ((a == 0) || (b == 0)) {
        result = true;
        *out = 0;
    } else if (a <= (INT64_MAX / b)) {
        result = true;
        *out = a * b;
    }

    return result;
}


static bool safe_mul_u64_impl(uint64_t a, uint64_t b, uint64_t *out)
{
    bool result = false;

    if ((a == 0) || (b == 0)) {
        result = true;
        *out = 0;
    } else if (a <= (UINT64_MAX / b)) {
        result = true;
        *out = a * b;
    }

    return result;
}


static bool safe_mul_usize_impl(size_t a, size_t b, size_t *out)
{
    bool result = false;

    if ((a == 0) || (b == 0)) {
        result = true;
        *out = 0;
    } else if (a <= (SIZE_MAX / b)) {
        result = true;
        *out = a * b;
    }

    return result;
}

#    define SAFE_ADD_U64(a, b, result_ptr)   safe_add_u64_impl(a, b, result_ptr)
#    define SAFE_ADD_S64(a, b, result_ptr)   safe_add_s64_impl(a, b, result_ptr)
#    define SAFE_MUL_S64(a, b, result_ptr)   safe_mul_s64_impl(a, b, result_ptr)
#    define SAFE_MUL_U64(a, b, result_ptr)   safe_mul_u64_impl(a, b, result_ptr)
#    define SAFE_MUL_USIZE(a, b, result_ptr) safe_mul_usize_impl(a, b, result_ptr)
#else
#    error Unsupported compiler
#endif

MaybeS64 safe_add_s64(int64_t a, int64_t b)
{
    MaybeS64 result = {0};

    result.ok = SAFE_ADD_S64(a, b, &result.value);

    return result;
}

MaybeU64 safe_add_u64(uint64_t a, uint64_t b)
{
    MaybeU64 result = {0};

    result.ok = SAFE_ADD_U64(a, b, &result.value);

    return result;
}

MaybeS64 safe_mul_s64(int64_t a, int64_t b)
{
    MaybeS64 result = {0};

    result.ok = SAFE_MUL_S64(a, b, &result.value);

    return result;
}

MaybeU64 safe_mul_u64(uint64_t a, uint64_t b)
{
    MaybeU64 result = {0};

    result.ok = SAFE_MUL_U64(a, b, &result.value);

    return result;
}

MaybeUsize safe_mul_usize(size_t a, size_t b)
{
    MaybeUsize result = {0};

    result.ok = SAFE_MUL_USIZE(a, b, &result.value);

    return result;
}

/***************************/
/*      Number parsing     */
/***************************/
static uint32_t parse_digit(char c, NumberBase base)
{
    ASSERT(is_digit(c) || ((base == NUM_BASE_HEX) && is_hex(c)));

    if (c >= 'a') {
        // Convert to upper
        c -= 'a' - 'A';
    }

    uint32_t result = 0;
    if (is_alpha(c)) {
        result = 10 + (uint32_t)(c - 'A');
    } else {
        result = (uint32_t)(c - '0');
    }

    return result;
}

MaybeS64 str_to_s64(memmi_String str, NumberBase base)
{
    // TODO: reduce code duplication between this and str_to_u64
    MaybeS64 result = {0};
    // negativ hex?

    int32_t sign = 1;
    if (str_starts_with(str, str_lit("-"))) {
        sign = -1;

        ++str.data;
        --str.count;
    }

    if (str_starts_with(str, str_lit("0x")) || str_starts_with(str, str_lit("0X"))) {
        str.data += 2;
        str.count -= 2;
    }

    result.ok = str.count > 0;

    for (size_t i = 0; i < str.count; ++i) {
        char c = str.data[i];

        if (!is_digit(c) && !((base == NUM_BASE_HEX) && is_hex(c))) {
            result.ok = false;
            break;
        } else {
            MaybeS64 product = safe_mul_s64(result.value, base);

            if (product.ok) {
                result.value = product.value;

                int32_t digit = (int32_t)parse_digit(c, base) * sign;

                MaybeS64 sum = safe_add_s64(result.value, digit);

                if (sum.ok) {
                    result.value = sum.value;
                } else {
                    result.ok = false;
                    break;
                }
            } else {
                result.ok = false;
                break;
            }
        }
    }

    // TODO: negative numbers

    return result;
}

MaybeU64 str_to_u64(memmi_String str, NumberBase base)
{
    MaybeU64 result = {0};

    if (str_starts_with(str, str_lit("0x")) || str_starts_with(str, str_lit("0X"))) {
        str.data += 2;
        str.count -= 2;
    }

    result.ok = str.count > 0;

    for (size_t i = 0; i < str.count; ++i) {
        char c = str.data[i];

        if (!is_digit(c) && !((base == NUM_BASE_HEX) && is_hex(c))) {
            result.ok = false;
            break;
        } else {
            MaybeU64 factor = safe_mul_u64(result.value, base);

            if (factor.ok) {
                result.value = factor.value;

                uint32_t digit = parse_digit(c, base);

                MaybeU64 sum = safe_add_u64(result.value, digit);

                if (sum.ok) {
                    result.value = sum.value;
                } else {
                    result.ok = false;
                    break;
                }
            } else {
                result.ok = false;
                break;
            }
        }
    }

    return result;
}

/***************************/
/*         Allocator       */
/***************************/
static void *memmi_default_allocate(void *ctx, void *ptr, size_t old_count, size_t new_count, size_t item_size, size_t align)
{
    (void)ctx;
    (void)old_count;
    (void)align;

    void *result = 0;

    MaybeUsize new_size = safe_mul_usize(new_count, item_size);

    if (new_size.ok) {
        // TODO: get rid of libc?
        result = realloc(ptr, new_size.value);
    }

    return result;
}

memmi_Allocator memmi_default_allocator()
{
    memmi_Allocator result = {0, memmi_default_allocate};

    return result;
}

#ifdef MEMMI_LINUX
#    include "memmi_linux.c"
#endif

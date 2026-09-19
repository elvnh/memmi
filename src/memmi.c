// Define compiler/OS context definitions.
#if defined(__linux__)
#    define MEMMI_LINUX 1
#elif defined(_WIN32)
#    define MEMMI_WIN32 1
#else
#    error Unsupported operating system
#endif

#if defined(__GNUC__)
#    define MEMMI_GCC 1
#elif defined(_MSC_VER)
#    define MEMMI_MSVC 1
#else
#    error Unsupported compiler
#endif

#if defined(MEMMI_LINUX)
#    ifndef _GNU_SOURCE
#        define _GNU_SOURCE
#    endif
#endif

// Define all undefined context definitions to 0.
#if !defined(MEMMI_LINUX)
#    define MEMMI_LINUX 0
#endif

#if !defined(MEMMI_WIN32)
#    define MEMMI_WIN32 0
#endif

#if !defined(MEMMI_GCC)
#    define MEMMI_GCC 0
#endif

#if !defined(MEMMI_MSVC)
#    define MEMMI_MSVC 0
#endif

#if !defined(MEMMI_DEBUG)
#    define MEMMI_DEBUG 0
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
  - Add namespace prefix to all macros/functions, in case user compiles
    as single translation unit
  - Use double underscore after prefix for internal functions
 */

/***************************/
/*    General utilities    */
/***************************/
#define memmi_allocate(a, t, count) (t *)((a).function((a).context, 0, 0, (count), sizeof(t), MEMMI_ALIGNOF(t)))
#define memmi_reallocate(a, ptr, old_count, new_count) (MEMMI_TYPEOF(ptr))((a).function((a).context, (ptr), (old_count), \
            (new_count), sizeof(*(ptr)), MEMMI_ALIGNOF(MEMMI_TYPEOF(*(ptr)))))
#define memmi_deallocate(a, ptr, count) (a).function((a).context, (ptr), (count), 0, sizeof(*(ptr)), \
        MEMMI_ALIGNOF(MEMMI_TYPEOF(*(ptr))))
#define MEMMI_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MEMMI_ARRAY_COUNT(arr) (sizeof((arr)) / sizeof(*(arr)))

#ifdef __cplusplus
#    define memmi_str_from_span(span) {(span).data, (span).count}
#else
#    define memmi_str_from_span(span) (memmi_String) {(span).data, (span).count}
#endif

#define MEMMI_PP_CONCAT_(a, b) a ## b
#define MEMMI_PP_CONCAT(a, b) MEMMI_PP_CONCAT_(a, b)

#if defined(__cplusplus)
#    define memmi_zero_struct(t) {}
#    define memmi_zero_enum(e) (e)0
#else
#    define memmi_zero_struct(t) (t){0}
#    define memmi_zero_enum(e) 0
#endif

// Integer semantics for enums to silence C++ warnings/errors
#define memmi_set_flag(lhs, flag) (lhs) = (MEMMI_TYPEOF(lhs))(lhs | flag)
#define memmi_inc_enum(e) (e = (MEMMI_TYPEOF(e))((e) + 1))

#if MEMMI_DEBUG
#    define MEMMI_ASSERT(e) do {                            \
        if (!(e)) {                                         \
            fprintf(stderr, "\n*** ASSERTION FAILED ***\n"  \
                "Expression: '%s'\nFunction: %s\n%s:%d:\n", \
                #e, __func__, __FILE__, __LINE__);          \
            MEMMI_DEBUG_BREAK;                              \
        }                                                   \
    } while (0)
#else
#    define MEMMI_ASSERT(e) (void)(e)
#endif

#define memmi_sl_push_back(list, node)          \
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

#if MEMMI_GCC
#    define MEMMI_DEBUG_BREAK __builtin_trap()
#elif MEMMI_MSVC
#    define MEMMI_DEBUG_BREAK __debugbreak()
#else
#    error MEMMI_DEBUG_BREAK not defined for this compiler
#endif

#if MEMMI_GCC
#    define MEMMI_ALIGNOF(t) __alignof__(t)
#elif MEMMI_MSVC
#    define MEMMI_ALIGNOF(t) __alignof(t)
#else
#    error MEMMI_ALIGNOF not defined for this compiler
#endif

#if MEMMI_GCC
#    define MEMMI_TYPEOF(t) __typeof__(t)
#elif MEMMI_MSVC
#    if defined(__cplusplus)
#        define MEMMI_TYPEOF(t) decltype(t)
#    else
#        define MEMMI_TYPEOF(t) __typeof__(t)
#    endif
#else
#    error MEMMI_TYPEOF not defined for this compiler
#endif

// Some helper functions are not used in all platform implementation files, causing them to
// emit warnings about being unused. This attribute will be placed on such functions.
#if MEMMI_GCC
#    define MEMMI_MAYBE_UNUSED __attribute__((unused))
#elif MEMMI_MSVC
#    define MEMMI_MAYBE_UNUSED __pragma(warning(disable: 4189))
#else
#    error MEMMI_MAYBE_UNUSED not defined for this compiler
#endif

/***************************/
/*         String          */
/***************************/
#if defined(__cplusplus)
#    define memmi_str_lit(s) { s, MEMMI_ARRAY_COUNT(s) - 1 }
#else
#    define memmi_str_lit(s) (memmi_String) { s, MEMMI_ARRAY_COUNT(s) - 1 }
#endif

static inline bool memmi_is_whitespace(char ch)
{
    bool result = (ch == ' ') || (ch == '\n') || (ch == '\t') || (ch == '\r');

    return result;
}

MEMMI_MAYBE_UNUSED
static memmi_String memmi_str_from_c_str(char *str)
{
    MEMMI_ASSERT(str);
    size_t length = strlen(str);

    memmi_String result = {str, length};

    return result;
}

static bool memmi_str_eq(memmi_String a, memmi_String b)
{
    bool result = false;

    if (a.count == b.count) {
        result = (!a.data && !b.data) || (memcmp(a.data, b.data, a.count) == 0);
    }

    return result;
}

MEMMI_MAYBE_UNUSED
static bool memmi_str_starts_with(memmi_String str, memmi_String substr)
{
    bool result = false;

    if (str.data && (substr.count <= str.count)) {
        memmi_String start = {str.data, substr.count};
        result = memmi_str_eq(start, substr);
    }

    return result;
}

typedef struct {
    memmi_String head;
    memmi_String tail;
    bool   ok;
} memmi_Cut;

MEMMI_MAYBE_UNUSED
static memmi_Cut memmi_str_cut(memmi_String str, memmi_String pattern)
{
    memmi_Cut result = memmi_zero_struct(memmi_Cut);
    result.head = str;

    if (pattern.count <= str.count) {
        size_t end = str.count - pattern.count;

        size_t index;
        for (index = 0; index <= end; ++index) {
            memmi_String substr = {str.data + index, pattern.count};

            if (memmi_str_eq(substr, pattern)) {
                result.head.data = str.data;
                result.head.count = index;
                
                result.tail.data = str.data + index + pattern.count;
                result.tail.count = str.count - index - pattern.count;

                result.ok = true;

                break;
            }
        }
    }

    return result;
}

MEMMI_MAYBE_UNUSED
static memmi_String memmi_str_trim_leading_whitespace(memmi_String str)
{
    memmi_String result = str;

    while ((result.count > 0) && memmi_is_whitespace(*result.data)) {
        ++result.data;
        --result.count;
    }

    return result;
}

MEMMI_MAYBE_UNUSED
static memmi_String memmi_str_trim_trailing_whitespace(memmi_String str)
{
    memmi_String result = str;

    while ((result.count > 0) && memmi_is_whitespace(result.data[result.count - 1])) {
        --result.count;
    }

    return result;
}

MEMMI_MAYBE_UNUSED
static memmi_String memmi_str_trim_whitespace(memmi_String str)
{
    memmi_String result = str;
    result = memmi_str_trim_leading_whitespace(result);
    result = memmi_str_trim_trailing_whitespace(result);

    return result;
}

static memmi_String memmi_str_copy(memmi_String str, memmi_Allocator allocator)
{
    memmi_String result = memmi_zero_struct(memmi_String);

    char *data = memmi_allocate(allocator, char, str.count);

    if (data) {
        memcpy(data, str.data, str.count);
        result.data = data;
        result.count = str.count;
    }

    return result;
}

/***************************/
/*     Safe arithmetic     */
/***************************/
static bool memmi_safe_add_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    bool result = false;

    if ((a == 0) || (b == 0)) {
        result = true;
    } else {
        result = a <= (UINT64_MAX - b);
    }

    if (result) {
        *out = a + b;
    }

    return result;
}

static bool memmi_safe_mul_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    bool result = false;

    if ((a == 0) || (b == 0)) {
        result = true;
    } else {
        result = a <= (UINT64_MAX / b);
    }

    if (result) {
        *out = a * b;
    }

    return result;
}

static bool memmi_safe_mul_usize(size_t a, size_t b, size_t *out)
{
    bool result = false;

    uint64_t u64_value = 0;

    if (memmi_safe_mul_u64(a, b, &u64_value)) {
        result = u64_value <= SIZE_MAX;
        *out = (size_t)u64_value;
    }

    return result;
}

/***************************/
/*     Integer parsing     */
/***************************/
int memmi_parse_digit(char c, uint32_t *out)
{
    int result = 0;
    uint32_t value = 0;

    if ((c >= '0') && (c < ('0' + (char)10))) {
        value = (uint32_t)((char)c - '0');
        result = 1;
    }

    *out = value;

    return result;
}

static bool memmi_str_to_u64(memmi_String str, uint64_t *out)
{
    bool result = str.count > 0;
    uint64_t value = 0;

    for (size_t i = 0; i < str.count; ++i) {
        uint32_t digit = 0;
        int digit_ok = memmi_parse_digit(str.data[i], &digit);

        int multiply_ok = memmi_safe_mul_u64(value, 10, &value);
        int add_ok = memmi_safe_add_u64(value, digit, &value);

        if (!(digit_ok && multiply_ok && add_ok)) {
            result = 0;
            break;
        }
    }

    *out = value;

    return result;
}

MEMMI_MAYBE_UNUSED
static bool memmi_str_to_usize(memmi_String str, size_t *out)
{
    bool result = false;
    uint64_t u64_value = 0;

    if (memmi_str_to_u64(str, &u64_value)) {
        result = u64_value <= SIZE_MAX;
    }

    *out = u64_value;

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

    size_t new_size = 0;

    if (memmi_safe_mul_usize(new_count, item_size, &new_size)) {
        result = realloc(ptr, new_size);
    }

    return result;
}

memmi_Allocator memmi_default_allocator(void)
{
    memmi_Allocator result = {0, memmi_default_allocate};

    return result;
}

/***************************/
/*      Dynamic array      */
/***************************/
// This macro returns a type erased dynamic array that has the new data, count and
// capacity. This result should be checked to ensure that any reallocation succeeded, and
// can then be assigned using dyn_arr_assign.
// TODO: check that sizeof item and sizeof arr->data is equal

typedef struct {
    void *data;
    size_t count;
    size_t capacity;
} memmi_DynArray;

#define memmi_dyn_arr_push(arr, item, alloc)                                     \
    memmi_dyn_arr_push_impl((arr)->data, (arr)->count, (arr)->capacity, &(item), \
        sizeof(*((arr)->data)), MEMMI_ALIGNOF(MEMMI_TYPEOF(*((arr)->data))), alloc)

static memmi_DynArray memmi_dyn_arr_push_impl(void *data, size_t count, size_t cap, void *item,
    size_t memb_size, size_t align, memmi_Allocator allocator)
{
    memmi_DynArray result = memmi_zero_struct(memmi_DynArray);
    result.data = data;
    result.count = count;
    result.capacity = cap;

    if (result.count == result.capacity) {
        size_t new_cap = MEMMI_MAX(result.capacity * 2, 32);
        result.data = allocator.function(allocator.context, data, result.capacity, new_cap, memb_size, align);
        result.capacity = new_cap;
    }

    if (result.data) {
        memcpy((char *)result.data + (memb_size * result.count), item, memb_size);
        ++result.count;
    }

    return result;
}

#define memmi_dyn_arr_assign(lhs, rhs)                          \
    do {                                                        \
        MEMMI_ASSERT(rhs.data);                                 \
        MEMMI_ASSERT(rhs.count);                                \
        MEMMI_ASSERT(rhs.capacity);                             \
                                                                \
        (lhs)->data = (MEMMI_TYPEOF((lhs)->data))(rhs).data;    \
        (lhs)->count = (rhs).count;                             \
        (lhs)->capacity = (rhs).capacity;                       \
    } while (0);

/***************************/
/*       Common types      */
/***************************/
typedef struct {
    memmi_ProcessInfo *data;
    size_t count;
    size_t capacity;
} memmi_ProcessDynArray;

typedef struct {
    memmi_MemoryRegion *data;
    size_t count;
    size_t capacity;
} memmi_RegionDynArray;

typedef struct {
    memmi_TID *data;
    size_t count;
    size_t capacity;
} memmi_ThreadDynArray;

typedef struct {
    memmi_Object *data;
    size_t count;
    size_t capacity;
} memmi_ObjectDynArray;

/***************************/
/*      Architecture       */
/***************************/
// TODO: get rid of these macros, not worth it
#if MEMMI_X64
#    define MEMMI_REGISTER_PREFIX_LETTER_UPPER  R
#    define MEMMI_REGISTER_PREFIX_LETTER_LOWER  r
#elif MEMMI_X86
#    define MEMMI_REGISTER_PREFIX_LETTER_UPPER  E
#    define MEMMI_REGISTER_PREFIX_LETTER_LOWER  e
#endif

#define MEMMI_16_BIT_TO_32_64_BIT_REGISTER_ENUM(name)                   \
    MEMMI_PP_CONCAT(MEMMI_REG_, MEMMI_PP_CONCAT(MEMMI_REGISTER_PREFIX_LETTER_UPPER, name))

#if MEMMI_X64
#    define MEMMI_VARIABLE_WIDTH_REGISTER_LIST_EXCLUDING_FLAGS  \
        MEMMI_REGISTER(RAX, rax)                                \
        MEMMI_REGISTER(RCX, rcx)                                \
        MEMMI_REGISTER(RDX, rdx)                                \
        MEMMI_REGISTER(RSI, rsi)                                \
        MEMMI_REGISTER(RDI, rdi)                                \
        MEMMI_REGISTER(RSP, rsp)                                \
        MEMMI_REGISTER(RBP, rbp)                                \
        MEMMI_REGISTER(RBX, rbx)                                \
        MEMMI_REGISTER(RIP, rip)
#elif MEMMI_X86
#    define MEMMI_VARIABLE_WIDTH_REGISTER_LIST_EXCLUDING_FLAGS  \
        MEMMI_REGISTER(EAX, eax)                                \
        MEMMI_REGISTER(ECX, ecx)                                \
        MEMMI_REGISTER(EDX, edx)                                \
        MEMMI_REGISTER(ESI, esi)                                \
        MEMMI_REGISTER(EDI, edi)                                \
        MEMMI_REGISTER(ESP, esp)                                \
        MEMMI_REGISTER(EBP, ebp)                                \
        MEMMI_REGISTER(EBX, ebx)                                \
        MEMMI_REGISTER(EIP, eip)
#endif

#define MEMMI_X64_ONLY_REGISTER_LIST         \
    MEMMI_REGISTER(R8,  r8)                  \
    MEMMI_REGISTER(R9,  r9)                  \
    MEMMI_REGISTER(R10, r10)                 \
    MEMMI_REGISTER(R11, r11)                 \
    MEMMI_REGISTER(R12, r12)                 \
    MEMMI_REGISTER(R13, r13)                 \
    MEMMI_REGISTER(R14, r14)                 \
    MEMMI_REGISTER(R15, r15)

#define MEMMI_REGISTER_ENUM(reg) MEMMI_REG_##reg

#define MEMMI_DR7_ENABLE_BIT_BASE_INDEX   16u
#define MEMMI_DR7_ENABLE_BIT_STRIDE       2u
#define MEMMI_DR7_COND_BITS_BASE_INDEX    16u
#define MEMMI_DR7_COND_BITS_STRIDE        4u
#define MEMMI_DR7_LENGTH_BITS_BASE_INDEX  18u
#define MEMMI_DR7_LENGTH_BITS_STRIDE      4u

#define MEMMI_DR7_READ_WRITE_COND         0x3u
#define MEMMI_DR7_WRITE_COND              0x1u
#define MEMMI_DR7_SIZE_1_BYTES            0x0u
#define MEMMI_DR7_SIZE_2_BYTES            0x1u
#define MEMMI_DR7_SIZE_4_BYTES            0x3u
#define MEMMI_DR7_SIZE_8_BYTES            0x2u

static memmi_Register memmi_debug_register_from_index(uint32_t index)
{
    memmi_Register result = memmi_zero_enum(memmi_Register);

    switch (index) {
        case 0: {
            result = MEMMI_REG_DR0;
        } break;

        case 1: {
            result = MEMMI_REG_DR1;
        } break;

        case 2: {
            result = MEMMI_REG_DR2;
        } break;

        case 3: {
            result = MEMMI_REG_DR3;
        } break;

        default: {
            MEMMI_ASSERT(0);
            result = MEMMI_REG_DR0;
        } break;
    }

    return result;
}

static memmi_RegisterValue memmi_dr7_breakpoint_mask(uint32_t breakpoint_index)
{
    uint32_t result =
          (0x1u << (MEMMI_DR7_ENABLE_BIT_BASE_INDEX  + breakpoint_index * MEMMI_DR7_ENABLE_BIT_STRIDE))
        | (0x3u << (MEMMI_DR7_COND_BITS_BASE_INDEX   + breakpoint_index * MEMMI_DR7_COND_BITS_STRIDE))
        | (0x3u << (MEMMI_DR7_LENGTH_BITS_BASE_INDEX + breakpoint_index * MEMMI_DR7_LENGTH_BITS_STRIDE));

    return result;
}

static memmi_RegisterValue memmi_dr7_local_enable_bit(uint32_t reg_index)
{
    // TODO: get rid of these casts
    memmi_RegisterValue result = (memmi_RegisterValue)((memmi_RegisterValue)0x1u
        << ((memmi_RegisterValue)reg_index * (memmi_RegisterValue)MEMMI_DR7_ENABLE_BIT_STRIDE));

    return result;
}

static memmi_RegisterValue memmi_dr7_condition_bits(uint32_t reg_index, memmi_BreakpointCondition condition)
{
    memmi_RegisterValue bits = 0;

    switch (condition) {
        case MEMMI_BREAKPOINT_READ_WRITE: {
            bits = MEMMI_DR7_READ_WRITE_COND;
        } break;

        case MEMMI_BREAKPOINT_WRITE: {
            bits = MEMMI_DR7_WRITE_COND;
        } break;

        default: {
            MEMMI_ASSERT(0);
            bits = MEMMI_DR7_READ_WRITE_COND;
        } break;
    }

    memmi_RegisterValue result = bits << (MEMMI_DR7_COND_BITS_BASE_INDEX + reg_index * MEMMI_DR7_COND_BITS_STRIDE);

    return result;
}

static memmi_RegisterValue memmi_dr7_length_bits(uint32_t reg_index, memmi_BreakpointLength length)
{
    memmi_RegisterValue bits = 0;

    switch (length) {
        case MEMMI_BREAKPOINT_1_BYTES: {
            bits = MEMMI_DR7_SIZE_1_BYTES;
        } break;

        case MEMMI_BREAKPOINT_2_BYTES: {
            bits = MEMMI_DR7_SIZE_2_BYTES;
        } break;

        case MEMMI_BREAKPOINT_4_BYTES: {
            bits = MEMMI_DR7_SIZE_4_BYTES;
        } break;

        case MEMMI_BREAKPOINT_8_BYTES: {
            bits = MEMMI_DR7_SIZE_8_BYTES;
        } break;
    }

    memmi_RegisterValue result = bits << (MEMMI_DR7_LENGTH_BITS_BASE_INDEX + reg_index * MEMMI_DR7_LENGTH_BITS_STRIDE);

    return result;
}

static memmi_RegisterValue memmi_dr7_set_breakpoint_value(memmi_RegisterValue old_dr7, uint32_t index,
    memmi_BreakpointCondition cond, memmi_BreakpointLength length)
{
    MEMMI_ASSERT(index <= 3);

    memmi_RegisterValue result =
        (old_dr7 & ~memmi_dr7_breakpoint_mask(index))
        | memmi_dr7_local_enable_bit(index)
        | memmi_dr7_condition_bits(index, cond)
        | memmi_dr7_length_bits(index, length);

    return result;
}

static int32_t memmi_get_dr6_breakpoint_index(memmi_RegisterValue dr6)
{
    int32_t result = -1;

    if (dr6 & 0x1) {
        result = 0;
    } else if (dr6 & 0x2) {
        result = 1;
    } else if (dr6 & 0x4) {
        result = 2;
    } else if (dr6 & 0x8) {
        result = 3;
    }

    return result;
}

/*******************************************/
/* Platform-independent API implementation */
/*******************************************/
int memmi_process_is_null(memmi_Process process)
{
    int result = process.pid == 0;

    return result;
}

/***************************/
/* Platform implementation */
/***************************/
#if MEMMI_LINUX
#    include "memmi_linux.c"
#elif MEMMI_WIN32
#    include "memmi_win32.c"
#endif

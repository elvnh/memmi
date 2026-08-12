#if defined(__linux__)
#    define MEMMI_LINUX
#elif defined(_WIN32)
#    define MEMMI_WIN32
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

#if defined(MEMMI_LINUX)
#    ifndef _GNU_SOURCE
#        define _GNU_SOURCE
#    endif
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
 */

/***************************/
/*    General utilities    */
/***************************/
#define allocate(a, t, count) (t *)((a).function((a).context, 0, 0, (count), sizeof(t), ALIGNOF(t)))
#define reallocate(a, ptr, old_count, new_count) (TYPEOF(ptr))((a).function((a).context, (ptr), (old_count), \
            (new_count), sizeof(*(ptr)), ALIGNOF(TYPEOF(*(ptr)))))
#define deallocate(a, ptr, count) (a).function((a).context, (ptr), (count), 0, sizeof(*(ptr)), ALIGNOF(TYPEOF(*(ptr))))
#define str_from_span(span) (memmi_String) {(span).data, (span).count}
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_COUNT(arr) (sizeof((arr)) / sizeof(*(arr)))

#define MEMMI_PP_CONCAT_(a, b) a ## b
#define MEMMI_PP_CONCAT(a, b) MEMMI_PP_CONCAT_(a, b)

#if defined(__cplusplus)
#    define zero_struct(t) {}
#    define zero_enum(e) (e)0
#else
#    define zero_struct(t) (t){0}
#    define zero_enum(e) 0
#endif

// Integer semantics for enums to silence C++ warnings/errors
#define set_flag(lhs, flag) (lhs) = (TYPEOF(lhs))(lhs | flag)
#define inc_enum(e) (e = (TYPEOF(e))((e) + 1))

#define ASSERT(e) do {                                      \
        if (!(e)) {                                         \
            fprintf(stderr, "\n*** ASSERTION FAILED ***\n"  \
                "Expression: '%s'\nFunction: %s\n%s:%d:\n", \
                #e, __func__, __FILE__, __LINE__);     \
            DEBUG_BREAK;                                    \
        }                                                   \
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
#    define TYPEOF(t) __typeof__(t)
#elif defined(MEMMI_MSVC)
#    if defined(__cplusplus)
#        define TYPEOF(t) decltype(t)
#    else
#        define TYPEOF(t) __typeof__(t)
#    endif
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
#if defined(__cplusplus)
#    define str_lit(s) { s, ARRAY_COUNT(s) - 1 }
#else
#    define str_lit(s) (memmi_String) { s, ARRAY_COUNT(s) - 1 }
#endif

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
    Cut result = zero_struct(Cut);
    result.head = str;

    if (pattern.count <= str.count) {
        size_t end = str.count - pattern.count;

        size_t index;
        for (index = 0; index <= end; ++index) {
            memmi_String substr = {str.data + index, pattern.count};

            if (str_eq(substr, pattern)) {
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
// TODO: these can be simplified
// TODO: are these even needed for win32?
#    define SAFE_ADD_S64(a, b, result_ptr)   safe_add_s64_impl((a), (b), (result_ptr))
#    define SAFE_ADD_U64(a, b, result_ptr)   safe_add_u64_impl((a), (b), (result_ptr))
#    define SAFE_MUL_S64(a, b, result_ptr)   safe_mul_s64_impl((a), (b), (result_ptr))
#    define SAFE_MUL_U64(a, b, result_ptr)   safe_mul_u64_impl((a), (b), (result_ptr))
#    define SAFE_MUL_USIZE(a, b, result_ptr) safe_mul_usize_impl((a), (b), (result_ptr))
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

#else
#    error Unsupported compiler
#endif

MaybeS64 safe_add_s64(int64_t a, int64_t b)
{
    MaybeS64 result = zero_struct(MaybeS64);

    result.ok = SAFE_ADD_S64(a, b, &result.value);

    return result;
}

MaybeU64 safe_add_u64(uint64_t a, uint64_t b)
{
    MaybeU64 result = zero_struct(MaybeU64);

    result.ok = SAFE_ADD_U64(a, b, &result.value);

    return result;
}

MaybeS64 safe_mul_s64(int64_t a, int64_t b)
{
    MaybeS64 result = zero_struct(MaybeS64);

    result.ok = SAFE_MUL_S64(a, b, &result.value);

    return result;
}

MaybeU64 safe_mul_u64(uint64_t a, uint64_t b)
{
    MaybeU64 result = zero_struct(MaybeU64);

    result.ok = SAFE_MUL_U64(a, b, &result.value);

    return result;
}

MaybeUsize safe_mul_usize(size_t a, size_t b)
{
    MaybeUsize result = zero_struct(MaybeUsize);

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

// TODO: These number parsing functions are bad
MaybeS64 str_to_s64(memmi_String str, NumberBase base)
{
    // TODO: reduce code duplication between this and str_to_u64
    MaybeS64 result = zero_struct(MaybeS64);
    // negativ hex?

    // Predeclare literals in order to avoid extended initializer list errors pre-C++11.
    memmi_String minus_lit = str_lit("-");
    memmi_String lower_hex_lit = str_lit("0x");
    memmi_String upper_hex_lit = str_lit("0X");

    int32_t sign = 1;
    if (str_starts_with(str, minus_lit)) {
        sign = -1;

        ++str.data;
        --str.count;
    }

    if (str_starts_with(str, lower_hex_lit) || str_starts_with(str, upper_hex_lit)) {
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
    MaybeU64 result = zero_struct(MaybeU64);

    // Predeclare literals in order to avoid extended initializer list errors pre-C++11.
    memmi_String lower_hex_lit = str_lit("0x");
    memmi_String upper_hex_lit = str_lit("0X");

    if (str_starts_with(str, lower_hex_lit) || str_starts_with(str, upper_hex_lit)) {
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

/***************************/
/*      Dynamic array      */
/***************************/
// This macro returns a type erased dynamic array that has the new data, count and
// capacity. This result should be checked to ensure that any reallocation succeeded, and
// can then be assigned using dyn_arr_assign.
// TODO: check that sizeof item and sizeof arr->data is equal
// TODO: Rename to dyn_arr_push
#define dyn_arr_push(arr, item, alloc)                                     \
    dyn_arr_push_impl((arr)->data, (arr)->count, (arr)->capacity, &(item), \
        sizeof(*((arr)->data)), ALIGNOF(TYPEOF(*((arr)->data))), alloc)

typedef struct {
    void *data;
    size_t count;
    size_t capacity;
} DynArray;

static DynArray dyn_arr_push_impl(void *data, size_t count, size_t cap, void *item,
    size_t memb_size, size_t align, memmi_Allocator allocator)
{
    DynArray result = zero_struct(DynArray);
    result.data = data;
    result.count = count;
    result.capacity = cap;

    if (result.count == result.capacity) {
        size_t new_cap = MAX(result.capacity * 2, 32);
        result.data = allocator.function(allocator.context, data, result.capacity, new_cap, memb_size, align);
        result.capacity = new_cap;
    }

    if (result.data) {
        memcpy((char *)result.data + (memb_size * result.count), item, memb_size);
        ++result.count;
    }

    return result;
}

#define dyn_arr_assign(lhs, rhs)                \
    do {                                        \
        ASSERT(rhs.data);                       \
        ASSERT(rhs.count);                      \
        ASSERT(rhs.capacity);                   \
                                                \
        (lhs)->data = (TYPEOF((lhs)->data))(rhs).data;    \
        (lhs)->count = (rhs).count;             \
        (lhs)->capacity = (rhs).capacity;       \
    } while (0);

/***************************/
/*       Common types      */
/***************************/
typedef struct {
    memmi_ProcessInfo *data;
    size_t count;
    size_t capacity;
} ProcessDynArray;

typedef struct {
    memmi_MemoryRegion *data;
    size_t count;
    size_t capacity;
} RegionDynArray;

typedef struct {
    memmi_TID *data;
    size_t count;
    size_t capacity;
} ThreadDynArray;

/***************************/
/*      Architecture       */
/***************************/
#if defined(MEMMI_X64)
#    define MEMMI_REGISTER_PREFIX_LETTER_UPPER  R
#    define MEMMI_REGISTER_PREFIX_LETTER_LOWER  r
#elif defined(MEMMI_X86)
#    define MEMMI_REGISTER_PREFIX_LETTER_UPPER  E
#    define MEMMI_REGISTER_PREFIX_LETTER_LOWER  e
#endif

// TODO: keeping this macro around probably isn't worth it
#define MEMMI_16_BIT_TO_32_64_BIT_REGISTER_ENUM(name)                                     \
    MEMMI_PP_CONCAT(MEMMI_REG_, MEMMI_PP_CONCAT(MEMMI_REGISTER_PREFIX_LETTER_UPPER, name))

#if defined(MEMMI_X64)
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
#elif defined(MEMMI_X86)
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

#define DR7_ENABLE_BIT_BASE_INDEX   16u
#define DR7_ENABLE_BIT_STRIDE       2u
#define DR7_COND_BITS_BASE_INDEX    16u
#define DR7_COND_BITS_STRIDE        4u
#define DR7_LENGTH_BITS_BASE_INDEX  18u
#define DR7_LENGTH_BITS_STRIDE      4u

// TODO: don't use binary constants
#define DR7_READ_WRITE_COND         0b11u
#define DR7_WRITE_COND              0b01u
#define DR7_SIZE_1_BYTES            0b00
#define DR7_SIZE_2_BYTES            0b01
#define DR7_SIZE_4_BYTES            0b11
#define DR7_SIZE_8_BYTES            0b10

static memmi_Register debug_register_from_index(uint32_t index)
{
    memmi_Register result = zero_enum(DebugRegister);

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
            ASSERT(0);
            result = MEMMI_REG_DR0;
        } break;
    }

    return result;
}

static memmi_RegisterValue dr7_breakpoint_mask(uint32_t breakpoint_index)
{
    uint32_t result =
          (0b01u << (DR7_ENABLE_BIT_BASE_INDEX  + breakpoint_index * DR7_ENABLE_BIT_STRIDE))
        | (0b11u << (DR7_COND_BITS_BASE_INDEX   + breakpoint_index * DR7_COND_BITS_STRIDE))
        | (0b11u << (DR7_LENGTH_BITS_BASE_INDEX + breakpoint_index * DR7_LENGTH_BITS_STRIDE));

    return result;
}

static memmi_RegisterValue dr7_local_enable_bit(uint32_t reg_index)
{
    // TODO: get rid of these casts
    memmi_RegisterValue result = (memmi_RegisterValue)((memmi_RegisterValue)0x1u
        << ((memmi_RegisterValue)reg_index * (memmi_RegisterValue)DR7_ENABLE_BIT_STRIDE));

    return result;
}

static memmi_RegisterValue dr7_condition_bits(uint32_t reg_index, memmi_BreakpointCondition condition)
{
    memmi_RegisterValue bits = 0;

    switch (condition) {
        case MEMMI_BREAKPOINT_READ_WRITE: {
            bits = DR7_READ_WRITE_COND;
        } break;

        case MEMMI_BREAKPOINT_WRITE: {
            bits = DR7_WRITE_COND;
        } break;

        default: {
            ASSERT(0);
            bits = DR7_READ_WRITE_COND;
        } break;
    }

    memmi_RegisterValue result = bits << (DR7_COND_BITS_BASE_INDEX + reg_index * DR7_COND_BITS_STRIDE);

    return result;
}

static memmi_RegisterValue dr7_length_bits(uint32_t reg_index, memmi_BreakpointLength length)
{
    memmi_RegisterValue bits = 0;

    switch (length) {
        case MEMMI_BREAKPOINT_1_BYTES: {
            bits = DR7_SIZE_1_BYTES;
        } break;

        case MEMMI_BREAKPOINT_2_BYTES: {
            bits = DR7_SIZE_2_BYTES;
        } break;

        case MEMMI_BREAKPOINT_4_BYTES: {
            bits = DR7_SIZE_4_BYTES;
        } break;

        case MEMMI_BREAKPOINT_8_BYTES: {
            bits = DR7_SIZE_8_BYTES;
        } break;
    }

    memmi_RegisterValue result = bits << (DR7_LENGTH_BITS_BASE_INDEX + reg_index * DR7_LENGTH_BITS_STRIDE);

    return result;
}

static memmi_RegisterValue dr7_set_breakpoint_value(memmi_RegisterValue old_dr7, uint32_t index,
    memmi_BreakpointCondition cond, memmi_BreakpointLength length)
{
    ASSERT(index <= 3);

    memmi_RegisterValue result =
        (old_dr7 & ~dr7_breakpoint_mask(index))
        | dr7_local_enable_bit(index)
        | dr7_condition_bits(index, cond)
        | dr7_length_bits(index, length);

    return result;
}

static int32_t get_dr6_breakpoint_index(memmi_RegisterValue dr6)
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

/***************************/
/* Platform implementation */
/***************************/
#if defined(MEMMI_LINUX)
#    include "memmi_linux.c"
#elif defined(MEMMI_WIN32)
#    include "memmi_win32.c"
#endif

#include "utils.h"

#include <string.h>
#include <stdlib.h>

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

// TODO: define these for other compilers
#if defined(__GNUC__)
#    define SAFE_ADD_S64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_ADD_U64(a, b, result_ptr)   !__builtin_add_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_S64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_U64(a, b, result_ptr)   !__builtin_mul_overflow((a), (b), (result_ptr))
#    define SAFE_MUL_USIZE(a, b, result_ptr) !__builtin_mul_overflow((a), (b), (result_ptr))
#elif defined(_MSC_VER)
#    define SAFE_ADD_U64(a, b, result_ptr)   (abort(), 0)
#    define SAFE_ADD_S64(a, b, result_ptr)   (abort(), 0)
#    define SAFE_MUL_S64(a, b, result_ptr)   (abort(), 0)
#    define SAFE_MUL_U64(a, b, result_ptr)   (abort(), 0)
#    define SAFE_MUL_USIZE(a, b, result_ptr) (abort(), 0)
#else
#    error Safe arithmetic undefined for compiler
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

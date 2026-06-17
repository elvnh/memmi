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
    ASSERT(str.data);
    ASSERT(str.count);

    ASSERT(substr.data);
    ASSERT(substr.count);

    bool result = false;

    if (substr.count <= str.count) {
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

memmi_String str_null_terminate_in_place(memmi_String s)
{
    memmi_String result = s;
    result.data[result.count] = '\0';

    return result;
}

int64_t str_to_s64(memmi_String str)
{
    ASSERT(is_number(str));
    // TODO: don't use atol
    int64_t result = atol(str.data);

    return result;
}

ParseU64 str_to_u64(memmi_String str)
{
    ParseU64 result = {0};
    result.ok = str.count > 0;

    for (size_t i = 0; i < str.count; ++i) {
        char c = str.data[i];

        if (!is_digit(c)) {
            result.ok = false;
            break;
        } else {
            if (!multiply_would_overflow_u64(result.value, 10)) {
                result.value *= 10;

                uint64_t digit = (uint64_t)(c - '0');

                if (!add_would_overflow_u64(result.value, digit)) {
                    result.value += digit;
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

bool add_would_overflow_u64(uint64_t a, uint64_t b)
{
    bool result = a > (UINT64_MAX - b);

    return result;
}

bool multiply_would_overflow_u64(uint64_t a, uint64_t b)
{
    bool result = a > (UINT64_MAX / b);

    return result;
}

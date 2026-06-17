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

int64_t str_to_int64(memmi_String str)
{
    ASSERT(is_number(str));
    int64_t result = atol(str.data);

    return result;
}

#include "utils.h"

#include <string.h>

memmi_String str_from_c_str(char *str)
{
    ASSERT(str);
    size_t length = strlen(str);

    memmi_String result = {str, length};

    return result;
}

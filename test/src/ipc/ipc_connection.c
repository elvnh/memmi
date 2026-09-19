#include "ipc_connection.h"

#if defined(__linux__)
#    include "ipc_connection_linux.c"
#else
#    include "ipc_connection_win32.c"
#endif

bool ipc_ok(Ipc ipc)
{
    bool result = ipc.data != 0;

    return result;
}

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ipc.c"
#include "test_common.h"

int main()
{
    char buffer[256];

    Ipc ipc = ipc_connect(TEST_IPC_PORT, sizeof(buffer));

    if (!ipc_ok(ipc)) {
        perror("");
        assert(0);
        return 1;
    }

    strncpy(buffer, "hello!", sizeof(buffer));

    bool result = ipc_send(ipc, buffer);
    assert(result);

    ipc_destroy(ipc);
}

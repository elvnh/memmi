#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "ipc.c"
#include "test_common.h"

int main()
{
    Ipc ipc = ipc_connect(TEST_IPC_PORT);

    if (!ipc_ok(ipc)) {
        perror("");
        assert(0);
        return 1;
    }

    char *buf = "hello world";
    size_t buf_size = strlen(buf);
    bool result = ipc_send(ipc, buf, buf_size);
    assert(result);

    ipc_destroy(ipc);
}

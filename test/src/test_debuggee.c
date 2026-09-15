#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ipc.c"
#include "test_common.h"

int main()
{
    Ipc ipc = ipc_accept(TEST_IPC_PORT);

    if (!ipc_ok(ipc)) {
        assert(0);

        return 1;
    }

    char buffer[1024];

    while (true) {
        size_t bytes_received = 0;

        if (!ipc_receive(ipc, buffer, sizeof(buffer), &bytes_received)) {
            assert(0);
            break;
        } else if (bytes_received == 0) {
            printf("Done\n");
            break;
        } else  {
            printf("%.*s\n", (int)bytes_received, buffer);
        }
    }

    ipc_destroy(ipc);
}

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

        IpcReceiveResult recv_res = ipc_receive(ipc, buffer, sizeof(buffer), &bytes_received, 1000);

        if (recv_res == IPC_RECEIVE_ERROR) {
            assert(0 && "Error");
            break;
        } else if (recv_res == IPC_RECEIVE_TIMEOUT) {
            assert(0 && "Timeout");
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

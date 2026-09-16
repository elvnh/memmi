#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ipc.c"
#include "test_common.h"

int main()
{
    char buffer[256];

    Ipc ipc = ipc_accept(TEST_IPC_PORT, sizeof(buffer));

    if (!ipc_ok(ipc)) {
        assert(0);

        return 1;
    }


    while (true) {
        IpcReceiveResult recv_res = ipc_receive_with_timeout(ipc, buffer, 1000);

        if (recv_res == IPC_RECEIVE_ERROR) {
            assert(0 && "Error");
            break;
        } else if (recv_res == IPC_RECEIVE_TIMEOUT) {
            assert(0 && "Timeout");
            break;
        } else if (recv_res == IPC_RECEIVE_DONE) {
            printf("Done\n");
            break;
        } else  {
            assert(recv_res == IPC_RECEIVE_OK);
            printf("%s\n", buffer);
        }
    }

    ipc_destroy(ipc);
}

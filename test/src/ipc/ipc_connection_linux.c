#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>

typedef enum {
    LNX_IPC_SERVER,
    LNX_IPC_CLIENT,
} IpcEnd;

typedef struct{
    IpcEnd ipc_end; // TODO: not needed?
    int server_socket;
    int client_socket; // Either ourselves, or the client from the servers point of view
} lnx_Ipc;

Ipc ipc_accept(int32_t port, size_t message_size)
{
    assert(message_size > 0);

    Ipc result = {0};
    result.message_size = message_size;

    lnx_Ipc ipc = {0};
    ipc.ipc_end = LNX_IPC_SERVER;

    ipc.server_socket = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = port;

    int bind_result = bind(ipc.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    int listen_result = listen(ipc.server_socket, 1);

    /* socklen_t client_addr_length = sizeof(client_addr); */
    ipc.client_socket = accept(ipc.server_socket, 0, 0/*(struct sockaddr *)client_addr, &client_addr_length*/);

    if ((ipc.server_socket != -1) && (bind_result != -1) && (listen_result != -1)) {
        lnx_Ipc *ipc_copy = calloc(1, sizeof(lnx_Ipc));
        *ipc_copy = ipc;
        result.data = ipc_copy;
    }

    return result;
}

Ipc ipc_connect(int32_t port, size_t message_size)
{
    Ipc result = {0};
    result.message_size = message_size;

    lnx_Ipc ipc = {0};
    ipc.ipc_end = LNX_IPC_CLIENT;

    ipc.client_socket = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = port;

    int connect_result = connect(ipc.client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr));

    if ((ipc.client_socket != -1) && (connect_result != -1)) {
        lnx_Ipc *ipc_copy = calloc(1, sizeof(lnx_Ipc));
        *ipc_copy = ipc;
        result.data = ipc_copy;
    } else {
        close(ipc.client_socket);
    }

    return result;
}

void ipc_destroy(Ipc ipc)
{
    lnx_Ipc *lnx_ipc = ipc.data;
    close(lnx_ipc->client_socket);
    close(lnx_ipc->server_socket);

    free(ipc.data);
}

bool ipc_send(Ipc ipc, void *msg)
{
    assert(ipc.message_size > 0);

    lnx_Ipc *lnx_ipc = ipc.data;

    size_t bytes_sent = 0;

    bool result = true;

    while (result && (bytes_sent < ipc.message_size)) {
        ssize_t send_result = send(lnx_ipc->client_socket, msg, ipc.message_size, 0);

        if (send_result == -1) {
            result = false;
        } else {
            bytes_sent += send_result;
        }
    }

    return result;
}

IpcReceiveResult ipc_receive_with_timeout(Ipc ipc, void *msg, uint32_t timeout_ms)
{
    assert(ipc.message_size > 0);

    IpcReceiveResult result = IPC_RECEIVE_OK;

    lnx_Ipc *lnx_ipc = ipc.data;

    size_t bytes_received = 0;

    while ((result == IPC_RECEIVE_OK) && (bytes_received < ipc.message_size)) {
        if (timeout_ms != IPC_TIMEOUT_NONE) {
            struct pollfd poll_fd = {0};
            poll_fd.fd = lnx_ipc->client_socket;
            poll_fd.events = POLLIN;

            int poll_result = poll(&poll_fd, 1, timeout_ms);

            if (poll_result == -1) {
                result = IPC_RECEIVE_ERROR;
            } else if (poll_result == 0) {
                result = IPC_RECEIVE_TIMEOUT;
            }
        }

        char *dst = (char *)msg + bytes_received;
        size_t bytes_to_read = ipc.message_size - bytes_received;
        ssize_t recv_result = recv(lnx_ipc->client_socket, dst, bytes_to_read, 0);
        assert(bytes_to_read > 0);

        if (recv_result == 0) {
            if (bytes_received == 0) {
                result = IPC_RECEIVE_DONE;
            } else {
                result = IPC_RECEIVE_ERROR;
            }
        } else if (recv_result == -1) {
            result = IPC_RECEIVE_ERROR;
        } else {
            bytes_received += recv_result;
        }
    }

    assert((result != IPC_RECEIVE_OK) || (bytes_received == ipc.message_size));

    return result;
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

typedef enum {
    LNX_IPC_SERVER,
    LNX_IPC_CLIENT,
} IpcEnd;

typedef struct{
    IpcEnd ipc_end; // TODO: not needed?
    int server_socket;
    int client_socket; // Either ourselves, or the client from the servers point of view
} lnx_Ipc;

Ipc ipc_accept(int port)
{
    Ipc result = {0};

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

Ipc ipc_connect(int port)
{
    Ipc result = {0};

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

bool ipc_send(Ipc ipc, void *buf, size_t buf_size)
{
    lnx_Ipc *lnx_ipc = ipc.data;

    ssize_t bytes_sent = send(lnx_ipc->client_socket, buf, buf_size, 0);
    bool result = bytes_sent == (ssize_t)buf_size;

    return result;
}

bool ipc_receive(Ipc ipc, void *buf, size_t buf_size, size_t *bytes_received)
{
    bool result = false;

    lnx_Ipc *lnx_ipc = ipc.data;

    ssize_t recv_result = recv(lnx_ipc->client_socket, buf, buf_size, 0);

    if (recv_result >= 0) {
        result = true;
        *bytes_received = recv_result;
    }

    return result;
}

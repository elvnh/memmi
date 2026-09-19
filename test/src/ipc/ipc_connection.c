#include "ipc_connection.h"

#if OS_LINUX
#    include <sys/socket.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <netinet/in.h>
#    include <errno.h>
#    include <poll.h>

#    define IPC_INVALID_SOCKET -1

    // This typedef is needed because Windows send() and recv() defines the size parameter as int.
    typedef size_t ipc_MsgSizeType;

#elif OS_WIN32
#    define IPC_INVALID_SOCKET INVALID_SOCKET

    // This typedef is needed because Windows send() and recv() defines the size parameter as int.
    typedef int ipc_MsgSizeType;

#endif

static void ipc_initialize_sockets();
static void ipc_close_socket(ipc_Socket socket_fd);
static int  ipc_poll_socket(ipc_Socket socket_fd, uint32_t timeout_ms);

bool ipc_ok(Ipc ipc)
{
    bool result = ipc.ok;

    return result;
}

Ipc ipc_accept(int32_t port, size_t message_size)
{
    assert(message_size > 0);

    ipc_initialize_sockets();

    Ipc result = {0};
    result.message_size = message_size;
    result.server_socket = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = (short)port;

    int bind_result = bind(result.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    int listen_result = listen(result.server_socket, 1);

    result.client_socket = accept(result.server_socket, 0, 0);

    if ((result.server_socket == IPC_INVALID_SOCKET) || (result.server_socket == IPC_INVALID_SOCKET)
        || (bind_result == -1) || (listen_result == -1)) {
        ipc_destroy(result);
    } else {
        result.ok = true;
    }

    return result;
}

Ipc ipc_connect(int32_t port, size_t message_size)
{
    ipc_initialize_sockets();

    Ipc result = {0};
    result.message_size = message_size;
    result.client_socket = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = (short)port;

    int connect_result = connect(result.client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr));

    if ((result.client_socket == IPC_INVALID_SOCKET) || (connect_result == -1)) {
        ipc_destroy(result);
    } else {
        result.ok = true;
    }

    return result;
}


void ipc_destroy(Ipc ipc)
{
    ipc_close_socket(ipc.client_socket);
    ipc_close_socket(ipc.server_socket);
}

bool ipc_send(Ipc ipc, void *msg)
{
    assert(ipc.message_size > 0);
    assert(ipc_ok(ipc));

    size_t bytes_sent = 0;

    bool result = true;

    while (result && (bytes_sent < ipc.message_size)) {
        int64_t send_result = send(ipc.client_socket, msg, (ipc_MsgSizeType)ipc.message_size, 0);

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
    assert(ipc_ok(ipc));

    IpcReceiveResult result = IPC_RECEIVE_OK;

    size_t bytes_received = 0;

    while ((result == IPC_RECEIVE_OK) && (bytes_received < ipc.message_size)) {
        if (timeout_ms != IPC_TIMEOUT_NONE) {
            int poll_result = ipc_poll_socket(ipc.client_socket, timeout_ms);

            if (poll_result == -1) {
                result = IPC_RECEIVE_ERROR;
            } else if (poll_result == 0) {
                result = IPC_RECEIVE_TIMEOUT;
            }
        }

        char *dst = (char *)msg + bytes_received;
        size_t bytes_to_read = ipc.message_size - bytes_received;
        int64_t recv_result = recv(ipc.client_socket, dst, (ipc_MsgSizeType)bytes_to_read, 0);
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

    assert(((result != IPC_RECEIVE_OK) || (bytes_received == ipc.message_size))
        && "If receiving succeeded, should always receive an entire message.");

    return result;
}

static void ipc_initialize_sockets()
{
    #if defined(_WIN32)
    WSADATA wsa_data = {0};
    int wsa_startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    assert(wsa_startup_result == 0);
    #endif
}

static void ipc_close_socket(ipc_Socket socket_fd)
{
    #if OS_LINUX
    close(socket_fd);
    #elif OS_WIN32
    closesocket(socket_fd);
    #endif
}

static int ipc_poll_socket(ipc_Socket socket_fd, uint32_t timeout_ms)
{
    struct pollfd poll_fd = {0};
    poll_fd.fd = socket_fd;
    poll_fd.events = POLLIN;

    int poll_result =
    #if OS_LINUX
        poll(&poll_fd, 1, timeout_ms);
    #elif OS_WIN32
        WSAPoll(&poll_fd, 1, timeout_ms);
    #endif

    return poll_result;
}

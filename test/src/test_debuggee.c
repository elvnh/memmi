#include "ipc/ipc_all.c"

#define MAX_VARIABLE_COUNT 1024

/* Inter-process communication */
Ipc      accept_debugger_connection(void);
bool     receive_command(Ipc ipc, Command *cmd);
bool     send_response(Ipc ipc, Response response);

/* Debuggee actions */
Response     handle_command(Command cmd);
VariableInfo get_variable(VariableId id);
VariableInfo set_variable(VariableId id, TypedValue typed_value);
VariableId   declare_variable(TypedValue typed_value);

/* Platform functions */
void *allocate_memory(size_t size);

/* Globals */
static struct {
    struct {
        VariableId next_id;
        Value values[MAX_VARIABLE_COUNT];
        ValueType types[MAX_VARIABLE_COUNT];
    } variables;
} g;

int main()
{
    Ipc ipc = accept_debugger_connection();

    while (true) {
        Command cmd = {0};

        if (receive_command(ipc, &cmd)) {
            Response response = handle_command(cmd);

            send_response(ipc, response);
        } else {
            break;
        }
    }

    ipc_destroy(ipc);
}

/* Inter-process communication */
Ipc accept_debugger_connection(void)
{
    Ipc result = ipc_accept(IPC_TEST_PORT, sizeof(Message));

    if (!ipc_ok(result)) {
        assert(0);
    }

    return result;
}

bool receive_command(Ipc ipc, Command *cmd)
{
    bool result = false;

    Message message = {0};
    IpcReceiveResult recv_res = ipc_receive_with_timeout(ipc, &message, IPC_TIMEOUT_NONE);

    if (recv_res == IPC_RECEIVE_OK) {
        result = true;
        *cmd = message.command;
    }

    return result;
}

bool send_response(Ipc ipc, Response response)
{
    Message message = {0};
    message.response = response;

    bool result = ipc_send(ipc, &message);

    return result;
}

Response handle_command(Command cmd)
{
    Response result = {0};

    switch (cmd.kind) {
        case CMD_DO_NOTHING: {
            result = res_ack();
        } break;

        case CMD_DECLARE_VARIABLE: {
            VariableId id = declare_variable(cmd.as.declare_variable);
            VariableInfo info = set_variable(id, cmd.as.declare_variable);
            result = res_variable_info(info);
        } break;

        case CMD_GET_VARIABLE_BY_ID: {
            VariableInfo info = get_variable(cmd.as.get_variable_by_id);

            result = res_variable_info(info);
        } break;

        case CMD_MAP_NEW_MEMORY: {
            void *memory = allocate_memory(cmd.as.allocation_size);

            result = res_memory_allocation((uintptr_t)memory);
        } break;

        case CMD_MALLOC: {
            void *memory = malloc(cmd.as.allocation_size);
            assert(memory);

            result = res_memory_allocation((uintptr_t)memory);
        } break;

        case CMD_EXIT_PROCESS: {
            exit(cmd.as.exit_process_with_code);
        } break;
    }

    return result;
}

/* Debuggee actions */
VariableInfo get_variable(VariableId id)
{
    assert(id < MAX_VARIABLE_COUNT);

    VariableInfo info = {0};
    info.id = id;
    info.address = (uintptr_t)&g.variables.values[id];
    info.value = g.variables.values[id];
    info.type = g.variables.types[id];

    return info;
}

VariableInfo set_variable(VariableId id, TypedValue typed_value)
{
    assert(id < MAX_VARIABLE_COUNT);

    g.variables.values[id] = typed_value.value;
    g.variables.types[id] = typed_value.type;

    VariableInfo result = get_variable(id);

    return result;
}

VariableId declare_variable(TypedValue typed_value)
{
    assert(g.variables.next_id < MAX_VARIABLE_COUNT);

    VariableId id = g.variables.next_id++;
    set_variable(id, typed_value);

    return id;
}

/* Platform functions */
#if OS_LINUX

#include <sys/mman.h>

void *allocate_memory(size_t size)
{
    void *result = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(result != MAP_FAILED);

    return result;
}
#else

#include <memoryapi.h>

void *allocate_memory(size_t size)
{
    void *result = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(result != 0);

    return result;
}

#endif

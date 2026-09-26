#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    uint8_t buffer[128] = {0};
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = (uint8_t)i;
    }

    Response res = send_command(ipc, cmd_map_new_memory(sizeof(buffer)));
    assert(res.kind == RES_DYNAMIC_MEMORY_ALLOCATION);

    // Write a certain pattern into the memory just allocated.
    uintptr_t address = res.as.dynamic_allocation_address;
    memmi_WriteMemory write_result = memmi_write_memory(proc, address, buffer, sizeof(buffer));
    REQUIRE(write_result.status == MEMMI_OK);
    REQUIRE(write_result.bytes_written = sizeof(buffer));

    uint8_t buffer2[sizeof(buffer)] = {0};

    // Read the memory just written into a new buffer and check that the two buffers match.
    memmi_ReadMemory read_result = memmi_read_memory(proc, buffer2, address, sizeof(buffer));
    REQUIRE(read_result.status == MEMMI_OK);
    REQUIRE(read_result.bytes_read == sizeof(buffer));

    REQUIRE(memcmp(buffer, buffer2, sizeof(buffer)) == 0);
}

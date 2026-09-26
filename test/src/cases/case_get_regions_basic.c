#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    Response response = send_command(ipc, cmd_declare_variable(val_int32(0)));

    uintptr_t address = response.as.variable_info.address;
    size_t var_size = sizeof(int32_t);

    memmi_MemoryRegions regions = memmi_get_process_memory_regions(proc, memmi_default_allocator());
    REQUIRE(regions.status == MEMMI_OK);

    bool variable_found = false;

    for (size_t i = 0; i < regions.count; ++i) {
        memmi_MemoryRegion region = regions.data[i];

        if ((region.base_address <= address) && ((address + var_size) <= (region.base_address + region.size))) {
            assert(!variable_found);
            variable_found = true;

            REQUIRE(region.permissions & MEMMI_REGION_PERMISSION_READ);
            REQUIRE(region.permissions & MEMMI_REGION_PERMISSION_WRITE);
            REQUIRE(!(region.permissions & MEMMI_REGION_PERMISSION_EXECUTE));
            REQUIRE(region.kind == MEMMI_REGION_NORMAL);
        }
    }

    REQUIRE(variable_found);
}

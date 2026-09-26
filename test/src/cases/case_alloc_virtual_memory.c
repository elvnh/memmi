#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
    memmi_Process proc = memmi_open_process(pid, memmi_default_allocator()).process;

    memmi_MemoryRegions regions_before_allocation =
        memmi_get_process_memory_regions(proc, memmi_default_allocator());
    assert(regions_before_allocation.status == MEMMI_OK);

    // Make a virtual memory allocation.
    size_t allocation_size = 4096;
    Response res = send_command(ipc, cmd_map_new_memory(allocation_size));
    assert(res.kind == RES_DYNAMIC_MEMORY_ALLOCATION);

    uintptr_t allocation_address = res.as.dynamic_allocation_address;

    // Check that the memory region wasn't found BEFORE the allocation was made.
    for (size_t i = 0; i < regions_before_allocation.count; ++i) {
        memmi_MemoryRegion r = regions_before_allocation.data[i];

        if (r.base_address == allocation_address) {
            REQUIRE(false);
        }
    }

    // Check that the memory region is found AFTER the allocation was made.
    memmi_MemoryRegions regions_after_allocation =
        memmi_get_process_memory_regions(proc, memmi_default_allocator());

    bool region_found = false;

    for (size_t i = 0; i < regions_after_allocation.count; ++i) {
        memmi_MemoryRegion r = regions_after_allocation.data[i];

        if (r.base_address == allocation_address) {
            REQUIRE(!region_found);

            REQUIRE(r.size >= allocation_size);
            REQUIRE(r.permissions & MEMMI_REGION_PERMISSION_READ);
            REQUIRE(r.permissions & MEMMI_REGION_PERMISSION_WRITE);
            REQUIRE(!(r.permissions & MEMMI_REGION_PERMISSION_EXECUTE));

            region_found = true;
        }
    }

    REQUIRE(region_found);
}

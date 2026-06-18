#pragma once

#include <stddef.h>
#include <stdint.h>

#include "allocator.h"
#include "string8.h"

typedef struct {
    int64_t value;
} memmi_PID;

typedef struct {
    int64_t value;
} memmi_TID;

typedef struct {
    memmi_String name;
    memmi_PID pid;
} memmi_ProcessInfo;

// TODO: handle if pid has been reused by another program since opening, should fail
typedef struct {
    void *data;
} memmi_Process;

typedef enum {
    MEMMI_OPEN_PROC_NO_SUCH_PID,
    MEMMI_OPEN_PROC_ALLOCATION_FAILED,
    MEMMI_OPEN_PROC_OK,
} memmi_OpenProcStatus;

typedef struct {
    memmi_OpenProcStatus  status;
    memmi_Process         process;
} memmi_OpenProcess;

// TODO: more descriptive errors
typedef enum {
    MEMMI_GET_PROCS_FAIL,
    MEMMI_GET_PROCS_OK,
} memmi_GetProcsStatus;

typedef struct {
    memmi_GetProcsStatus  status;
    memmi_ProcessInfo    *data;
    size_t                count;
} memmi_ProcessList;

typedef enum {
    MEMMI_READ_MEM_ACCESS_ERROR,
    MEMMI_READ_MEM_ALLOCATION_FAILURE,
    MEMMI_READ_MEM_INSUFFICIENT_PERMISSIONS,
    MEMMI_READ_MEM_NO_SUCH_PROCESS,
    MEMMI_READ_MEM_PARTIAL_READ,
    MEMMI_READ_MEM_READ_TOO_LARGE,
    MEMMI_READ_MEM_OK,
} memmi_ReadMemoryStatus;

typedef struct {
    memmi_ReadMemoryStatus status;
    char *memory;
    size_t bytes_read;
} memmi_ReadMemory;

// TODO: the status codes between read/write memory should probably be shared
typedef enum {
    MEMMI_WRITE_MEM_ACCESS_ERROR,
    MEMMI_WRITE_MEM_ALLOCATION_FAILURE,
    MEMMI_WRITE_MEM_INSUFFICIENT_PERMISSIONS,
    MEMMI_WRITE_MEM_NO_SUCH_PROCESS,
    MEMMI_WRITE_MEM_WRITE_TOO_LARGE,
    MEMMI_WRITE_MEM_PARTIAL_WRITE,
    MEMMI_WRITE_MEM_OK,
} memmi_WriteMemoryStatus;

typedef struct {
    memmi_WriteMemoryStatus status;
    size_t bytes_written;
} memmi_WriteMemory;

typedef enum {
    MEMMI_REGION_PERMISSION_READ    = 1 << 0,
    MEMMI_REGION_PERMISSION_WRITE   = 1 << 1,
    MEMMI_REGION_PERMISSION_EXECUTE = 1 << 2,
} memmi_MemoryRegionPermission;

typedef struct {
    uintptr_t base_address;
    size_t size;
    memmi_MemoryRegionPermission permissions;
} memmi_MemoryRegion;

typedef enum {
    MEMMI_GET_REGIONS_OK,
    MEMMI_GET_REGIONS_FAIL,
} memmi_GetMemoryRegionsStatus;

// TODO: for functions like this, it's not necessary to return status,
// since success/fail can be inferred from whether data/count is non-zero
typedef struct {
    memmi_GetMemoryRegionsStatus status;
    memmi_MemoryRegion *data;
    size_t count;
} memmi_GetMemoryRegions;

typedef struct {
    memmi_TID *data;
    size_t     count;
} memmi_ThreadList;

typedef enum {
    MEMMI_ATTACH_OK,
    MEMMI_ATTACH_NO_SUCH_PROCESS,
    MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS,
} memmi_AttachStatus;

typedef enum {
    MEMMI_RESUME_OK,
    MEMMI_RESUME_DEAD_OR_NOT_SUSPENDED,
    MEMMI_RESUME_INSUFFICIENT_PERMISSIONS,
} memmi_ResumeStatus;

memmi_ProcessList      memmi_get_running_processes(memmi_Allocator allocator);
memmi_OpenProcess      memmi_open_process(memmi_PID pid, memmi_Allocator allocator);
void                   memmi_close_process(memmi_Process process, memmi_Allocator allocator);
memmi_ReadMemory       memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator);
memmi_WriteMemory      memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size);
memmi_GetMemoryRegions memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator);
memmi_ThreadList       memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator);
memmi_AttachStatus     memmi_attach_to_process(memmi_Process process);
memmi_AttachStatus     memmi_attach_to_thread(memmi_TID tid);
memmi_AttachStatus     memmi_suspend_process(memmi_Process process);
memmi_AttachStatus     memmi_resume_process(memmi_Process process);
memmi_AttachStatus     memmi_suspend_thread(memmi_TID tid);
memmi_ResumeStatus     memmi_resume_thread(memmi_TID tid);

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "allocator.h"
#include "string8.h"

/*
  TODO:
  - Opaque thread handle
  - Store pid in opaque process handle
  - always use zero initialization for statuses
  - use common status enum so that each function doesn't define its own
  - Software breakpoints
  - Separate architecture-specific things into separate file
  - Unit test debug breakpoints
 */

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
    MEMMI_ATTACH_SOME_THREADS_ATTACHED,
    MEMMI_ATTACH_NO_SUCH_PROCESS,
    MEMMI_ATTACH_INSUFFICIENT_PERMISSIONS,
} memmi_AttachStatus;

typedef enum {
    MEMMI_DETACH_OK,
    MEMMI_DETACH_SOME_THREADS_DETACHED,
    MEMMI_DETACH_NO_SUCH_PROCESS,
    MEMMI_DETACH_INSUFFICIENT_PERMISSIONS,
} memmi_DetachStatus;

typedef enum {
    MEMMI_RESUME_OK,
    MEMMI_RESUME_DEAD_OR_NOT_SUSPENDED,
    MEMMI_RESUME_INSUFFICIENT_PERMISSIONS,
    MEMMI_RESUME_PARTIAL_SUCCESS,
} memmi_ResumeStatus;

// TODO: Report failure to get event
// TODO: is both THREAD_SUSPENDED and THREAD_STOPPED needed?
typedef struct memmi_DebugEvent {
    enum {
        MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED,
        MEMMI_DEBUG_EVENT_BREAKPOINT,
        MEMMI_DEBUG_EVENT_THREAD_STOPPED,
        MEMMI_DEBUG_EVENT_THREAD_SUSPENDED,
        MEMMI_DEBUG_EVENT_THREAD_EXITED,
        MEMMI_DEBUG_EVENT_THREAD_KILLED,
        MEMMI_DEBUG_EVENT_THREAD_RESUMED,
    } kind;

    memmi_TID id_of_affected_thread;

    union {
        struct {
            memmi_TID id;
        } new_thread;

        struct {
            int exit_code;
        } thread_exited;
    } as;

    struct memmi_DebugEvent *next;
} memmi_DebugEvent;

typedef struct {
    memmi_DebugEvent *first;
    memmi_DebugEvent *last;
} memmi_EventList;

// TODO: floating point registers
typedef enum {
    MEMMI_REG_RAX,
    MEMMI_REG_RCX,
    MEMMI_REG_RDX,
    MEMMI_REG_RSI,
    MEMMI_REG_RDI,
    MEMMI_REG_RSP,
    MEMMI_REG_RBP,
    MEMMI_REG_RBX,

    MEMMI_REG_R8,
    MEMMI_REG_R9,
    MEMMI_REG_R10,
    MEMMI_REG_R11,
    MEMMI_REG_R12,
    MEMMI_REG_R13,
    MEMMI_REG_R14,
    MEMMI_REG_R15,

    /* MEMMI_REG_ORIG_RAX, */

    MEMMI_REG_RIP,

    MEMMI_REG_CS,
    MEMMI_REG_EFLAGS,

    MEMMI_REG_SS,
    MEMMI_REG_FS_BASE,
    MEMMI_REG_GS_BASE,
    MEMMI_REG_DS,
    MEMMI_REG_ES,
    MEMMI_REG_FS,
    MEMMI_REG_GS,

    MEMMI_REG_COUNT
} memmi_Register;

typedef enum {
    MEMMI_GET_REGS_OK,
    MEMMI_GET_REGS_NO_SUCH_PROCESS,
    MEMMI_GET_REGS_INSUFFICIENT_PERMISSIONS,
} memmi_GetRegistersStatus;

typedef enum {
    MEMMI_SET_REGS_OK,
    MEMMI_SET_REGS_NO_SUCH_PROCESS,
    MEMMI_SET_REGS_INSUFFICIENT_PERMISSIONS,
} memmi_SetRegistersStatus;

typedef struct {
    memmi_GetRegistersStatus status;
    uint64_t values[MEMMI_REG_COUNT]; // TODO: typedef register values to make porting to other arch easier
} memmi_Registers;

typedef enum {
    MEMMI_SUSPEND_OK,
    MEMMI_SUSPEND_PARTIAL_SUCCESS,
    MEMMI_SUSPEND_NO_SUCH_PROCESS,
    MEMMI_SUSPEND_INSUFFICIENT_PERMISSIONS,
} memmi_SuspendStatus;

// TODO: allow checking if process exists
memmi_ProcessList        memmi_get_running_processes(memmi_Allocator allocator);
memmi_OpenProcess        memmi_open_process(memmi_PID pid, memmi_Allocator allocator);
void                     memmi_close_process(memmi_Process process, memmi_Allocator allocator);
memmi_ReadMemory         memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator);
memmi_WriteMemory        memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size);
memmi_GetMemoryRegions   memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator);
memmi_ThreadList         memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator);
memmi_AttachStatus       memmi_attach_to_process(memmi_Process process);
memmi_DetachStatus       memmi_detach_from_process(memmi_Process process);
memmi_ResumeStatus       memmi_resume_process(memmi_Process process);
memmi_SuspendStatus      memmi_suspend_process(memmi_Process process);
memmi_EventList          memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator);
memmi_Registers          memmi_get_thread_registers(memmi_TID tid);
memmi_SetRegistersStatus memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value);

/* Breakpoint */
typedef enum {
    MEMMI_SET_BREAKPOINT_OK,
    MEMMI_SET_BREAKPOINT_NO_SUCH_PROCESS,
    MEMMI_SET_BREAKPOINT_INSUFFICIENT_PERMISSIONS,
    MEMMI_SET_BREAKPOINT_INVALID_INDEX,
} memmi_SetBreakpointResult;

typedef enum {
    MEMMI_BREAKPOINT_READ_WRITE,
    MEMMI_BREAKPOINT_WRITE,
    /* MEMMI_BREAKPOINT_EXECUTE, */
} memmi_BreakpointCondition;

typedef enum {
    MEMMI_BREAKPOINT_1_BYTES = 1,
    MEMMI_BREAKPOINT_2_BYTES = 2,
    MEMMI_BREAKPOINT_4_BYTES = 4,
    MEMMI_BREAKPOINT_8_BYTES = 8,
} memmi_BreakpointLength;

// TODO: communicate partial successes
memmi_SetBreakpointResult memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
    memmi_BreakpointCondition condition, uint32_t index, memmi_BreakpointLength length);

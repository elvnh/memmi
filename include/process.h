#pragma once

#include <stddef.h>
#include <stdint.h>

#include "allocator.h"
#include "string8.h"
#include "status.h"

/*
  TODO:
  - Make Linux implementation use continue_after_debug_events
  - Make Linux report MEMMI_DEBUG_EVENT_PROCESS_EXIT if main thread
    exits
  - Opaque thread handle?
  - Store pid in opaque process handle
  - Software breakpoints
  - Separate architecture-specific things into separate file
  - Unit test breakpoints
  - Letting user spawn a new process as a debuggee rather than just attaching

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

// TODO: store PID directly here. Linux won't need to use data member, windows will use it for handle.
// Doing it this way will skip an unnecessary allocation.
typedef struct {
    void *data;
} memmi_Process;

typedef struct {
    memmi_Status   status;
    memmi_Process  process;
} memmi_OpenProcess;

typedef struct {
    memmi_Status        status;
    memmi_ProcessInfo  *data;
    size_t              count;
} memmi_ProcessList;

typedef struct {
    memmi_Status  status;
    char         *memory;
    size_t        bytes_read;
} memmi_ReadMemory;

typedef struct {
    memmi_Status  status;
    size_t        bytes_written;
} memmi_WriteMemory;

typedef enum {
    MEMMI_REGION_PERMISSION_READ    = (1u << 0u),
    MEMMI_REGION_PERMISSION_WRITE   = (1u << 1u),
    MEMMI_REGION_PERMISSION_EXECUTE = (1u << 2u),
} memmi_MemoryRegionPermission;

typedef struct {
    uintptr_t base_address;
    size_t size;
    memmi_MemoryRegionPermission permissions;
} memmi_MemoryRegion;

typedef struct {
    memmi_Status         status;
    memmi_MemoryRegion  *data;
    size_t               count;
} memmi_MemoryRegions;

typedef struct {
    memmi_Status   status;
    memmi_TID     *data;
    size_t         count;
} memmi_ThreadList;

// TODO: is both THREAD_SUSPENDED and THREAD_STOPPED needed?
// TODO: on linux, if the main thread exits, report as PROCESS_EXIT
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

// TODO: it's unfortunate that we have to return a list of events
// due to ptrace jank. Try to get rid of this.
typedef struct {
    memmi_Status      status;
    memmi_TID         id_of_affected_thread;
    memmi_DebugEvent *first;
    memmi_DebugEvent *last;
} memmi_EventList;

// TODO: floating point registers
// TODO: include debug registers in these?
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

    MEMMI_REG_RIP,

    MEMMI_REG_CS,
    MEMMI_REG_EFLAGS,

    MEMMI_REG_SS,
    MEMMI_REG_FS_BASE, // TODO: not needed?
    MEMMI_REG_GS_BASE, // TODO: not needed?
    MEMMI_REG_DS,
    MEMMI_REG_ES,
    MEMMI_REG_FS,
    MEMMI_REG_GS,

    MEMMI_REG_COUNT
} memmi_Register;

// TODO: include debug registers in this
typedef struct {
    memmi_Status status;
    uint64_t     values[MEMMI_REG_COUNT]; // TODO: typedef register values to make porting to other arch easier
} memmi_Registers;

// TODO: allow checking if process exists
// TODO: should new threads start in suspended state?
memmi_ProcessList        memmi_get_running_processes(memmi_Allocator allocator);
memmi_OpenProcess        memmi_open_process(memmi_PID pid, memmi_Allocator allocator);
void                     memmi_close_process(memmi_Process process, memmi_Allocator allocator);
memmi_ReadMemory         memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator);
memmi_WriteMemory        memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size);
memmi_MemoryRegions      memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator);
memmi_ThreadList         memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator);
memmi_Status             memmi_attach_to_process(memmi_Process process);
memmi_Status             memmi_detach_from_process(memmi_Process process);
memmi_Status             memmi_resume_process(memmi_Process process);
memmi_Status             memmi_suspend_process(memmi_Process process);
memmi_EventList          memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator);
memmi_Status             memmi_continue_after_debug_events(memmi_Process process, memmi_EventList events);
memmi_Registers          memmi_get_thread_registers(memmi_TID tid);
memmi_Status             memmi_set_thread_register(memmi_TID tid, memmi_Register reg, uint64_t value);

/* Breakpoint */
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

memmi_Status memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
    memmi_BreakpointCondition condition, uint32_t index, memmi_BreakpointLength length);

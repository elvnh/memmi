#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__) || defined (_M_AMD64)
#    define MEMMI_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(_M_IX86)
#    define MEMMI_X86 1
#else
#    error Unsupported architecture
#endif

// Define all undefined context definitions to 0.
#if !defined(MEMMI_X64)
#    define MEMMI_X64 0
#endif

#if !defined(MEMMI_X86)
#    define MEMMI_X86 0
#endif

/* Type definitions */
typedef enum {
    MEMMI_OK                       =  0u,
    MEMMI_NO_SUCH_PROCESS          = (1u << 0u),
    MEMMI_INSUFFICIENT_PERMISSIONS = (1u << 1u),
    MEMMI_INVALID_ARGUMENTS        = (1u << 2u),
    MEMMI_ALLOCATION_FAILED        = (1u << 3u),
    MEMMI_PARTIAL_READ_OR_WRITE    = (1u << 4u),
    MEMMI_OTHER_ERROR              = (1u << 5u),
} memmi_Status;

typedef void *(*memmi_AllocateFn)(void *ctx, void *ptr, size_t old_count, size_t new_count, size_t item_size, size_t align);

typedef struct {
    void             *context;
    memmi_AllocateFn  function;
} memmi_Allocator;

// TODO: ensure that strings that are returned to user are always null terminated
typedef struct {
    const char *data;
    size_t count;
} memmi_String;

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

typedef struct {
    void *data;
    memmi_PID pid;
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
    char         *memory; // TODO: make into void *
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

typedef enum {
    MEMMI_DEBUG_EVENT_NEW_THREAD_CREATED,
    MEMMI_DEBUG_EVENT_BREAKPOINT,
    MEMMI_DEBUG_EVENT_THREAD_STOPPED,
    MEMMI_DEBUG_EVENT_THREAD_EXITED,
    MEMMI_DEBUG_EVENT_THREAD_KILLED,
    MEMMI_DEBUG_EVENT_PROCESS_EXITED,
} memmi_DebugEventKind;

typedef struct memmi_DebugEvent {
    memmi_DebugEventKind kind;

    memmi_TID id_of_affected_thread;

    union {
        struct {
            memmi_TID id;
        } new_thread;

        struct {
            uint32_t breakpoint_index;
            size_t   ip_register;
        } breakpoint;

        int exit_code; // Used by both THREAD_EXITED and PROCESS_EXITED.
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
typedef enum {
    // General purpose registers
#if MEMMI_X64
    MEMMI_REG_RAX,
    MEMMI_REG_RCX,
    MEMMI_REG_RDX,
    MEMMI_REG_RSI,
    MEMMI_REG_RDI,
    MEMMI_REG_RSP,
    MEMMI_REG_RBP,
    MEMMI_REG_RBX,
#elif MEMMI_X86
    MEMMI_REG_EAX,
    MEMMI_REG_ECX,
    MEMMI_REG_EDX,
    MEMMI_REG_ESI,
    MEMMI_REG_EDI,
    MEMMI_REG_ESP,
    MEMMI_REG_EBP,
    MEMMI_REG_EBX,
#endif

    MEMMI_REG_R8,
    MEMMI_REG_R9,
    MEMMI_REG_R10,
    MEMMI_REG_R11,
    MEMMI_REG_R12,
    MEMMI_REG_R13,
    MEMMI_REG_R14,
    MEMMI_REG_R15,

    // Instruction pointer and flags
#if MEMMI_X64
    MEMMI_REG_RIP,
    MEMMI_REG_RFLAGS,
#elif MEMMI_X86
    MEMMI_REG_EIP,
    MEMMI_REG_EFLAGS,
#endif

    // Segment registers
    MEMMI_REG_CS,
    MEMMI_REG_SS,
    MEMMI_REG_DS,
    MEMMI_REG_ES,
    MEMMI_REG_FS,
    MEMMI_REG_GS,

    // Debug registers
    MEMMI_REG_DR0,
    MEMMI_REG_DR1,
    MEMMI_REG_DR2,
    MEMMI_REG_DR3,
    MEMMI_REG_DR6,
    MEMMI_REG_DR7,

    // Number of registers present
    MEMMI_REG_COUNT
} memmi_Register;

#if MEMMI_X64
    typedef uint64_t memmi_RegisterValue;
#elif MEMMI_X86
    typedef uint32_t memmi_RegisterValue;
#endif

typedef struct {
    memmi_Status        status;
    memmi_RegisterValue values[MEMMI_REG_COUNT];
} memmi_Registers;

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

typedef enum {
    MEMMI_OBJECT_EXECUTABLE,
    MEMMI_OBJECT_DYNAMIC_LIBRARY,
} memmi_ObjectKind;

typedef struct {
    memmi_ObjectKind kind;
    memmi_String     path;
    uintptr_t        base_address;
    size_t           size;
} memmi_Object;

typedef struct {
    memmi_Status  status;
    memmi_Object *data;
    size_t        count;
} memmi_ObjectList;

/* Functions */
// TODO: allow checking if process exists
// TODO: should new threads start in suspended state?
// TODO: free_process_list
memmi_ProcessList        memmi_get_running_processes(memmi_Allocator allocator);
memmi_OpenProcess        memmi_open_process(memmi_PID pid);
void                     memmi_close_process(memmi_Process process);
int                      memmi_process_is_null(memmi_Process process);
memmi_ReadMemory         memmi_read_memory(memmi_Process process, uintptr_t address, size_t size, memmi_Allocator allocator);
memmi_WriteMemory        memmi_write_memory(memmi_Process process, uintptr_t dst, void *src, size_t src_size);
memmi_MemoryRegions      memmi_get_process_memory_regions(memmi_Process process, memmi_Allocator allocator);
memmi_ThreadList         memmi_get_process_threads(memmi_Process process, memmi_Allocator allocator);
memmi_Status             memmi_attach_to_process(memmi_Process process);
memmi_Status             memmi_detach_from_process(memmi_Process process);
memmi_Status             memmi_resume_process(memmi_Process process);
memmi_Status             memmi_suspend_process(memmi_Process process);

// NOTE: Will resume process if suspended then wait. A debug event causes all threads in process to be suspended
memmi_EventList          memmi_wait_for_debug_events(memmi_Process process, memmi_Allocator allocator);
memmi_Status             memmi_continue_after_debug_events(memmi_Process process, memmi_EventList events);
memmi_Registers          memmi_get_thread_registers(memmi_TID tid);
memmi_Status             memmi_set_thread_register(memmi_TID tid, memmi_Register reg, memmi_RegisterValue value);
memmi_Status             memmi_set_hardware_breakpoint(memmi_Process process, uintptr_t address,
                                                       memmi_BreakpointCondition condition, uint32_t index,
                                                       memmi_BreakpointLength length);
memmi_ObjectList         memmi_get_loaded_objects(memmi_Process proc, memmi_Allocator allocator);
memmi_Allocator          memmi_default_allocator(void);

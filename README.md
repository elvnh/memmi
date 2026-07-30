# memmi
memmi is a simple process debugging library made in C99. It allows the user to do things
such as attaching to a remote process as a debugger, reading/writing its memory, setting
breakpoints in it and so on. The library is currently under development, as such the API is
subject to change and no guarantees are made about the library being correct.

memmi is currently available for Linux but a Windows version is currently in progress. The
library only supports x86 architectures but in the future, support may be added for other
architectures such as ARM.

## Roadmap
- [x] Linux support
- [ ] Windows support
- [ ] Non-x86 support
- [x] Retrieving currently running processes
- [x] Querying remote process information such as threads and mapped memory regions
- [x] Reading/writing remote process memory
- [x] Attaching as debugger
- [x] Resuming/suspending process
- [x] Waiting for debug events
- [x] Getting/setting general-purpose registers
- [ ] Getting/setting floating point registers
- [ ] Software breakpoints
- [x] Hardware breakpoints

## Building
This library aims to be easy to compile and include in your project. It consists only of a
header file, a platform-independent implementation file and one platform-dependent
implementation file per operating system. These can either be vendored by adding
them directly to your source tree, or you can compile the project as library and then link to it. In
the future, there will be a script to amalgamate all source files into one to make
vendoring even easier.

memmi requires no specific compiler options to compile, save for the requirement that the
compiler supports C99. Therefore, compiling the library should be as simple as invoking
your compiler of choice on it, or compiling it the same way you compile the rest of your
project.

In the future, there will be a Makefile/build script provided for users who prefer that.

## Example usage
```c
memmi_PID pid = {atoi(argv[1])};
memmi_OpenProcess open_result = memmi_open_process(pid, memmi_default_allocator());

if (open_result.status == MEMMI_OK) {
    memmi_Process proc = open_result.process;
    memmi_Status attach_result = memmi_attach_to_process(proc);

    if (attach_result == MEMMI_OK) {
        memmi_ReadMemory memory = memmi_read_memory(proc, 0xDEADBEEF, 1024, memmi_default_allocator());
        // Do something with the memory if the read succeeded...
        memmi_detach_from_process(proc);
        memmi_close_process(proc, memmi_default_allocator());
    }
}
```

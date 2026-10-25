# memmi (WIP)
memmi is a simple process debugging library made in C99. It allows the user to do things
such as attaching to a remote process as a debugger, reading/writing its memory, setting
breakpoints in it and so on.

memmi is currently available for Linux and Windows. The library only supports x86
architectures but in the future, support may be added for other architectures such as ARM.

## Warning
This library is currently under development and parts of it do not yet work correctly. I
do not recommend using this library in its current state.

## Features
- [x] Linux support
- [x] Windows support
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
memmi lets the user control all allocations. Therefore, all functions in memmi that may
allocate take an allocator as a parameter. If you wish to simply use `malloc`/`free`, pass
in the return value of `memmi_default_allocator()` wherever an allocator is required.

```c
memmi_PID pid = {atoi(argv[1])};
memmi_OpenProcess open_result = memmi_open_process(pid, memmi_default_allocator());

if (open_result.status == MEMMI_OK) {
    memmi_Process proc = open_result.process;
    memmi_Status attach_result = memmi_attach_to_process(proc);

    if (attach_result == MEMMI_OK) {
        char buffer[1024];
        memmi_ReadMemory memory = memmi_read_memory(proc, buffer, 0xDEADBEEF, sizeof(buffer));

        if (memory.status == MEMMI_OK) {
            // Do something with the memory if the read succeeded...
        }

        memmi_detach_from_process(proc);
        memmi_close_process(proc, memmi_default_allocator());
    }
}
```

## Tests
This library is tested using a form of integration testing. The tests are comprised of the
following parts: a test runner, a debuggee and test cases.

### Components
#### Test runner
The test runner is the entry point for running tests, and is responsible for launching the
debuggee and the test case executables as child processes. The test cases that are to be
launched are passed as command line parameters.

The test cases will be launched in sequence, with a fresh instance of the debuggee
launched for each case. The output of the test case is then parsed by the test runner to
see how many assertions were failed/passed.

The test runner is located in `build/test_runner`.

#### Debuggee
The debuggee is the application that is attached to and then interacted with by the test case
process. The debuggee opens a socket, and then runs a loop in which it executes commands
that are received over the socket. A command could for example be to write to a variable,
exit the process, or make a heap allocation. A response is then sent back to the test
case to indicate the result of performing the command.

The debuggee is located in `build/debuggee`.

#### Test cases
A test case is an application that opens a socket and connects to the debuggee process. It
will then generally send commands to the debuggee, and assert that these have the desired
effect.

For example, a test case might attach to the debuggee, place a hardware breakpoint on a
variable and then send a command to the debuggee, instructing it to write to that
variable. The test case can then assert that this write triggered a breakpoint.

Commands can either be send synchronously, meaning that the test case hangs until a
response is received, or asynchronously, meaning that the test case does not wait for a
response. Asynchronous commands are useful if the debuggee is suspended.

The test cases are located in `build/cases/`.

### Creating a test case
To create a new test case, simply create a new `.c`-file with the following form:

```c
#include "test_case_base.c"

void test_case_main(Pid pid, Ipc ipc)
{
}
```

The file `test_case_base.c` is located in `test/src/` and contains the entry point for the
test case, along with some definitions necessary for performing tests. The entry point
takes care of connecting to the debuggee via socket, and then passes control along to
`test_case_main()` defined in the test case itself.

The parameter `pid` is the PID of the debuggee process, while `ipc` is a handle used for
communicating with the debuggee process.

Assertions are generally made using the `REQUIRE` macro.

This test case can then be compiled, and the executable can then be passed to the test
runner, or be ran manually.

Note that the `stdout` output of a test case must follow a certain form in order to be
considered valid by the test runner. In order to ensure this, do not print anything
manually to `stdout`. If logging is necessary, you may log to `stderr` instead. The
`stdout` output is captured and parsed by the test runner to check the results of the test
case.

#### Flaky tests
If a test is flaky, meaning it is undeterministic and inconsistenly fails, you may include a call to `test_rerun_count()` somewhere in your test, and pass to it an integer that signifies how many times the test should be reran. For example, `test_rerun_count(10)` would mean that the test is reran 10 times, for a total of 11 runs. 

This only has an effect when running via the test runner.

### Running test cases
A test case may either be passed to the test runner, or be ran automatically.

#### Running via test runner
To run via the test runner, simply pass the path of the compiled test case(s) to the test
runner. The test runner will then take care of launching the debuggee as well as the test
cases, and then print some statistics.

#### Running manually
Sometimes, one may wish to run the cases manually, for example, if one wishes to attach a
debugger to the debuggee or the test case (or both).

To do this, first launch the debuggee process. Then, launch the test case, and pass the
PID of the debuggee process as a command line parameter. On Linux, an easy way to do this
is to simply run `./case $(pgrep debuggee)`. If running the test case via a debugger, make
sure your debugger passes the correct command line parameters to it.

@echo off
:restart
setlocal enabledelayedexpansion

:: TODO: make this work for msvc

set build_dir=build
set cases_dir=%build_dir%/cases

if not exist "%build_dir%" mkdir "%build_dir%" ||goto error
if not exist "%cases_dir%" mkdir "%cases_dir%" ||goto error

gcc src/test_debuggee.c -std=c99 -Wall -Wextra -ggdb -o "%build_dir%/debuggee" -lws2_32
gcc src/test_runner.c -std=c99 -Wall -Wextra -ggdb -o "%build_dir%/test_runner" -DDEBUGGEE_EXECUTABLE_NAME=\"debuggee.exe\" -lws2_32

for %%f in (src/cases/*) do (
    set name=%%~nf
    set test_case_exe="%cases_dir%/!name!"

    gcc src/cases/%%f -std=c99 -Wall -Wextra -ggdb -o !test_case_exe! -Isrc -I../src -I../include/memmi -lws2_32
)
endlocal



exit /b 0

:error
echo [error: failed to build tests]
exit /b 1

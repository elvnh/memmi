@echo off
:restart
setlocal enabledelayedexpansion

:: TODO: rename constants to all caps
:: TODO: make script change dir to dir of itself

:: TODO: allow user to set some of these options
set cc=cl
set arch=x64

set build_dir=build
set cases_dir=%build_dir%/cases
set obj_dir=%build_dir%/obj

set msvc_cflags=/Isrc -I../src /I../include/memmi /W4 /wd4100 /wd4702 /wd4127 /nologo ws2_32.lib /Fd"%obj_dir%\\" /Fo"%obj_dir%\\"

set msvc_cflags_debug=-Zi /DMEMMI_DEBUG=1 /fsanitize=address
set msvc_cflags_release=

set msvc_cflags=%msvc_cflags% %msvc_cflags_debug%

set cflags=%msvc_cflags%

:: if exist %build_dir% RD /S /Q %build_dir% ||goto error

if not exist "%build_dir%" mkdir "%build_dir%" ||goto error
if not exist "%cases_dir%" mkdir "%cases_dir%" ||goto error
if not exist "%obj_dir%" mkdir "%obj_dir%" ||goto error

call vcvarsall %arch% > nul 2>&1 || goto error

:: TODO: use variable for debuggee name
%cc% %cflags% src/test_debuggee.c /Fe"%build_dir%/debuggee" ||goto error
%cc% %cflags% src/test_runner.c /Fe"%build_dir%/test_runner" /D DEBUGGEE_EXECUTABLE_NAME=\"debuggee.exe\" ||goto error

for %%f in (src/cases/*) do (
    set name=%%~nf
    set test_case_exe="%cases_dir%/!name!"

    %cc% %cflags% src/cases/%%f /Fe!test_case_exe! ||goto error
)

if "%1"=="run" (
   set cases=
   for %%f in (%cases_dir%/*.exe) do set cases=!cases! "%cases_dir%/%%f"
   "%build_dir%/test_runner" !cases! ||goto error
)

exit /b 0

:error
echo [error: failed to build tests]
exit /b 1

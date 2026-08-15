@echo off
:restart
setlocal enabledelayedexpansion

:: TODO: shared library for msvc, will need either .def file or __declspec(dllexport)

:: Options
for %%a in (%*) do set "%%~a=1"

if not "%msvc%"=="1"    if not "%clang%"=="1"  set msvc=1
if not "%x64%"=="1"     if not "%x86%"=="1"    set x64=1
if not "%release%"=="1" if not "%debug%"=="1"  set release=1
if not "%static%"=="1"  if not "%shared%"=="1" set static=1

if "%clang%"=="1" set msvc=0&& echo [compiler: clang] && set compiler=clang
if "%msvc%"=="1"  set clang=0&& echo [compiler: msvc] && set compiler=cl 

if "%x64%"=="1" set x86=0&& echo [architecture: x64] && set arch=x64
if "%x86%"=="1" set x64=0&& echo [architecture: x86] && set arch=x86

if "%debug%"=="1"   set release=0&& echo [mode: debug]
if "%release%"=="1" set debug=0&& echo [mode: release]

if "%shared%"=="1" set static=0&& echo [target: shared]
if "%static%"=="1" set shared=0&& echo [target: static]

call vcvarsall %arch% > nul 2>&1

:: Compiler flags
set sources=src/memmi.c
set build_dir=build
set bin=%build_dir%/memmi

set cflags=

set msvc_cflags=/Iinclude /W4 /wd4100 /wd4702 /wd4127 /nologo /Fo:%bin% /c
set msvc_cflags_debug=-Zi /DMEMMI_DEBUG=1 /fsanitize=address
set msvc_cflags_release=

set clang_cflags=-Iinclude -Werror -std=c99 -Wall -Wextra -Wshadow -Wcast-align -Wunused -Wconversion -Wstrict-prototypes -Wsign-conversion -Wsign-compare -Wenum-compare -Wenum-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wcast-align -Werror=return-type -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=implicit-function-declaration -Werror=overflow -Werror=implicit-int -Wsign-conversion -Werror=missing-braces -Werror=ignored-qualifiers -Wno-error=unused-parameter -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable
set clang_cflags_debug=-g -fsanitize=address,undefined
set clang_cflags_release=

if %debug%==1 set msvc_cflags=%msvc_cflags% %msvc_cflags_debug%&& set clang_flags=%clang_cflags% %clang_cflags_debug%
if %release%==1 set msvc_cflags=%msvc_cflags% %msvc_cflags_release%&& set clang_cflags=%clang_cflags% %clang_cflags_release%

if %msvc%==1 set cflags=%msvc_cflags%
if %clang%==1 set cflags=%clang_cflags%

:: Compilation
if not exist %build_dir% mkdir %build_dir%

echo [compiling...]
%compiler% %cflags% %sources%

:: TODO: check if ok
if %static%==1 lib %bin%.obj /OUT:%bin%.a /NOLOGO
if %shared%==1 link %bin%.obj /OUT:%bin%.dll /DLL /NOLOGO
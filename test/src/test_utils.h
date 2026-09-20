#pragma once

#if defined(__linux__)
#    define OS_LINUX 1
#elif defined(_WIN32)
#    define OS_WIN32 1
#else
#    error Unsupported operating system
#endif

#if defined(__GNUC__)
#    define COMPILER_GCC 1
#elif defined(_MSC_VER)
#    define COMPILER_MSVC 1
#else
#    error Unsupported compiler
#endif

// Define all undefined context definitions to 0.
#if !defined(OS_LINUX)
#    define OS_LINUX 0
#endif

#if !defined(OS_WIN32)
#    define OS_WIN32 0
#endif

#if !defined(COMPILER_GCC)
#    define COMPILER_GCC 0
#endif

#if !defined(COMPILER_MSVC)
#    define COMPILER_MSVC 0
#endif

// As stdout is captured by the test runner, debug prints have to be done to stderr.
#define LOG(...) fprintf(stderr, __VA_ARGS__)

#pragma once

/* This file should be included by all test case files. */

#include "debugger.c"

#if defined(__GNUC__)
#    define DEBUG_BREAK()
#else
#    error DEBUG_BREAK() not defined for this compiler
#endif

#define REQUIRE(e)                                              \
    do {                                                        \
        if (!(e)) {                                             \
            fprintf(stderr, "\n*** TEST ASSERTION FAILED ***\n" \
                "Expression: '%s'\nTest case: %s\n%s:%d:\n",    \
                #e, __FILE__, __FILE__, __LINE__);              \
            DEBUG_BREAK();                                      \
        } else {                                                \
            ++g__assertions_passed;                             \
        }                                                       \
        ++g__assertions_ran;                                    \
    } while (0);

static uint64_t g__assertions_ran;
static uint64_t g__assertions_passed;

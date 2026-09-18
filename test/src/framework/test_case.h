
#pragma once

/*
  TODO:
  - Allow tests to define arbitrary names
  - Allow breaking/continuing/stopping on test failure
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "command.h"
#include "response.h"
#include "common.h"
#include "ipc.h"
#include "debugger.h"

#define TEST_OUTPUT_FMT_STRING "%" PRIu64 "/%" PRIu64 "\n"

#if defined(__GNUC__)
#    define MAYBE_UNUSED __attribute__((unused))
#else
#    error MAYBE_UNUSED not defined for this compiler
#endif

#define REQUIRE(e)                                              \
    do {                                                        \
        if (!(e)) {                                             \
            fprintf(stderr, "\n*** TEST ASSERTION FAILED ***\n" \
                "Expression: '%s'\nTest case: %s\n%s:%d:\n\n",    \
                #e, __FILE__, __FILE__, __LINE__);              \
        } else {                                                \
            ++g__assertions_passed;                             \
        }                                                       \
        ++g__assertions_ran;                                    \
    } while (0)

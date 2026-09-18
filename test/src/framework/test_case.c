/* This file should be included by all test case files. */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#    define _GNU_SOURCE
#endif

#include "test_case.h"

#define MEMMI_DEBUG 1
#include "memmi.c"

#include "debugger.c"

static MAYBE_UNUSED uint64_t g__assertions_ran;
static MAYBE_UNUSED uint64_t g__assertions_passed;

void test_case_main();

int main()
{
    test_case_main();
    printf(TEST_OUTPUT_FMT_STRING, g__assertions_passed, g__assertions_ran);
}


/* This file should be included by all test case files. */

#include "test_case.h"

#include "debugger.c"

static MAYBE_UNUSED uint64_t g__assertions_ran;
static MAYBE_UNUSED uint64_t g__assertions_passed;

void test_case_main();

int main()
{
    test_case_main();
    printf(TEST_OUTPUT_FMT_STRING, g__assertions_passed, g__assertions_ran);
}


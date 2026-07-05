#!/usr/bin/env sh

CFLAGS="
    -Werror
    -fsanitize=address,undefined
    -std=c99
    -Wall
    -Wextra
    -ggdb
    -Wall
    -Wextra
    -Wshadow
    -Wcast-align
    -Wunused
    -Wconversion
    -Wstrict-prototypes
    -Wsign-conversion
    -Wsign-compare
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wcast-align
    -Werror=return-type
    -Werror=incompatible-pointer-types
    -Werror=int-conversion
    -Werror=implicit-function-declaration
    -Werror=overflow
    -Werror=implicit-int
    -Wsign-conversion
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Werror=missing-braces
    -Werror=discarded-qualifiers
    -Wno-error=unused-parameter
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
"

gcc ${CFLAGS} main.c -Iinclude -Isrc src/*.c src/**/*.c -o memmi;
g++ -Wall -Wextra -ggdb test.cpp;

#!/usr/bin/env sh

CFLAGS="-Wall -Wextra -ggdb -I../include/ -Isrc/framework"

cd $(dirname $0);
mkdir -p build;

gcc ${CFLAGS} src/debuggee_main.c -o build/debuggee;

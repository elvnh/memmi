#!/usr/bin/env sh

CFLAGS="-Wall -Wextra -ggdb -I../include/"

cd $(dirname $0);
mkdir -p build;

gcc ${CFLAGS} src/test_debuggee.c -o build/debuggee;
gcc ${CFLAGS} src/test_debugger.c -o build/client;

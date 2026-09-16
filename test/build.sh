#!/usr/bin/env sh

CFLAGS="-Wall -Wextra -ggdb -I../include/"

cd $(dirname $0);
mkdir -p build;

gcc ${CFLAGS} src/debuggee.c -o build/debuggee;

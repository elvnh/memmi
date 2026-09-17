#!/usr/bin/env sh

CC="gcc"
CFLAGS="-Wall -Wextra -ggdb -I../include/ -Isrc/framework"

cd $(dirname $0);

rm -r build 2> /dev/null;
mkdir -p build/cases;

${CC} ${CFLAGS} src/debuggee_main.c -o build/debuggee;
${CC} ${CFLAGS} src/test_runner.c  -o build/test_runner;

for file in src/cases/*.c; do
    [ -e "$file" ] || continue

    name=$(basename ${file})
    name_without_ext=${name%.*}

    ${CC} ${CFLAGS} ${file} -o "build/cases/"${name_without_ext}
done

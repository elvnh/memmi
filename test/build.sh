#!/usr/bin/env sh

CC="gcc"
CFLAGS="-Wall -Wextra -ggdb -I../include/ -Isrc/framework"

BUILD_DIR="build"
DEBUGGEE_EXE="debuggee"
DEBUGGEE_PATH="${BUILD_DIR}/${DEBUGGEE_EXE}"
TEST_RUNNER_PATH="${BUILD_DIR}/test_runner"

cd $(dirname $0);

rm -r ${BUILD_DIR} 2> /dev/null;
mkdir -p "${BUILD_DIR}/cases";

${CC} ${CFLAGS} src/debuggee_main.c -o ${DEBUGGEE_PATH};
${CC} ${CFLAGS} src/test_runner.c  -o ${TEST_RUNNER_PATH};

for file in src/cases/*.c; do
    [ -e "$file" ] || continue

    name=$(basename ${file})
    name_without_extension=${name%.*}

    ${CC} ${CFLAGS} ${file} -o "build/cases/"${name_without_extension} \
          -DDEBUGGEE_EXECUTABLE_NAME="\"${DEBUGGEE_EXE}\""
done

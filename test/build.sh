#!/usr/bin/env bash

CC="gcc"
CFLAGS="-Wall -Wextra -ggdb -I../include/memmi -Isrc/framework -I../src/"

BUILD_DIR="build"
CASES_DIR="${BUILD_DIR}/cases"
DEBUGGEE_EXE="debuggee"
DEBUGGEE_PATH="${BUILD_DIR}/${DEBUGGEE_EXE}"
TEST_RUNNER_PATH="${BUILD_DIR}/test_runner"

cd $(dirname $0);

rm -r ${BUILD_DIR} 2> /dev/null;
mkdir -p ${CASES_DIR};

${CC} ${CFLAGS} src/debuggee_main.c -o ${DEBUGGEE_PATH};
${CC} ${CFLAGS} src/test_runner.c  -o ${TEST_RUNNER_PATH};

for file in src/cases/*.c; do
    [ -e "$file" ] || continue

    name=$(basename ${file})
    name_without_extension=${name%.*}

    ${CC} ${CFLAGS} ${file} -o "${CASES_DIR}/${name_without_extension}" \
          -DDEBUGGEE_EXECUTABLE_NAME="\"${DEBUGGEE_EXE}\""
done

if [[ "$1" == "run" ]]; then
    ./${TEST_RUNNER_PATH} ${CASES_DIR}/*
fi

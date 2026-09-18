#!/usr/bin/env bash

CC="gcc"
CFLAGS="-Wall -Wextra -ggdb -I../include/memmi -I../src/ -Isrc"

BUILD_DIR="build"
CASES_DIR="${BUILD_DIR}/cases"
DEBUGGEE_EXE="debuggee"
DEBUGGEE_PATH="${BUILD_DIR}/${DEBUGGEE_EXE}"
TEST_RUNNER_PATH="${BUILD_DIR}/test_runner"

cd $(dirname $0);

# TODO: don't remove the directories, this gets weird if the user is inside the
# directory when it gets removed, just remove all files instead
# TODO: if sudo when creating dir, it will be write protected
rm -r ${BUILD_DIR} 2> /dev/null;
mkdir -p ${CASES_DIR};

${CC} ${CFLAGS} src/test_debuggee.c -o ${DEBUGGEE_PATH} &&
${CC} ${CFLAGS} src/test_runner.c  -o ${TEST_RUNNER_PATH} -DDEBUGGEE_EXECUTABLE_NAME="\"${DEBUGGEE_EXE}\"";

success=$?


for file in src/cases/*.c; do
    if [[ ${success} != 0 ]]; then
        break
    fi

    [ -e "$file" ] || continue
    name=$(basename ${file})
    name_without_extension=${name%.*}
    test_case_exe="${CASES_DIR}/${name_without_extension}"

    ${CC} ${CFLAGS} ${file} -o ${test_case_exe} &&
        setcap CAP_SYS_PTRACE=eip ${test_case_exe};
    success=$?
done

# TODO: separate test script for running tests so they don't have to be run with sudo
if [[ ${success} == 0 ]]; then
    if [[ "$1" == "run" ]]; then
        ./${TEST_RUNNER_PATH} ${CASES_DIR}/*
    fi
else
    echo "Failed to compile tests."
fi

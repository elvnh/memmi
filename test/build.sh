#!/usr/bin/env bash

CC="gcc"
CFLAGS="-Wall -Wextra -ggdb -I../include/memmi -I../src/ -Isrc"
CLIBS=""

BUILD_DIR="build"
CASES_DIR="${BUILD_DIR}/cases"
DEBUGGEE_EXE="debuggee"
DEBUGGEE_PATH="${BUILD_DIR}/${DEBUGGEE_EXE}"
TEST_RUNNER_PATH="${BUILD_DIR}/test_runner"

cd $(dirname $0);

rm -r ${BUILD_DIR}/* 2> /dev/null;
rm -r ${CASES_DIR}/* 2> /dev/null;
mkdir -p ${CASES_DIR};

# Check if user is compiling on MSYS on Windows
if [[ -n "$MSYSTEM" ]]; then
    CLIBS="-lws2_32"
else
    sudo -n true 2> /dev/null;
    sudo_result=$?

    if [[ "${sudo_result}" != "0" ]]; then
        echo "In order to set the CAP_SYS_PTRACE permission on the test cases you will need root permissions. Please enter your password: ";
    fi
fi

${CC} ${CFLAGS} src/test_debuggee.c -o ${DEBUGGEE_PATH} ${CLIBS} &&
${CC} ${CFLAGS} src/test_runner.c  -o ${TEST_RUNNER_PATH} -DDEBUGGEE_EXECUTABLE_NAME="\"${DEBUGGEE_EXE}\"" ${CLIBS};

success=$?

for file in src/cases/*.c; do
    if [[ ${success} != 0 ]]; then
        break
    fi

    [ -e "$file" ] || continue

    name=$(basename ${file})
    name_without_extension=${name%.*}
    test_case_exe="${CASES_DIR}/${name_without_extension}"

    test_case_flags="${CFLAGS} -Wno-unused-parameter"

    ${CC} ${test_case_flags} ${file} -o ${test_case_exe} ${CLIBS};
    success=$?

    if [[ -z "$MSYSTEM" ]]; then
        sudo setcap CAP_SYS_PTRACE=eip ${test_case_exe};
    fi

    success=$?
done

if [[ ${success} == 0 ]]; then
    if [[ "$1" == "run" ]]; then
        extension=""
        if [[ -n "$MSYSTEM" ]]; then
            extension=".exe"
        fi

        ./${TEST_RUNNER_PATH} ${CASES_DIR}/*${extension}
    fi
else
    echo "Failed to compile tests."
fi

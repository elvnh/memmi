#!/usr/bin/env bash

# Compiler flags
CC="${CC:-gcc}"

CFLAGS_DEBUG="-ggdb -fsanitize=address,undefined -DMEMMI_DEBUG=1"
CFLAGS_RELEASE=""

CFLAGS_SHARED="-fPIC -shared"
CFLAGS_STATIC="-c"

CFLAGS_X64="-m64"
CFLAGS_X86="-m32"

CFLAGS_CLANG=""
CFLAGS_GCC=""
CFLAGS_COMMON="
    -Iinclude
    -Werror
    -std=c99
    -Wall
    -Wextra
    -Wshadow
    -Wcast-align
    -Wunused
    -Wconversion
    -Wstrict-prototypes
    -Wsign-conversion
    -Wsign-compare
    -Wenum-compare
    -Wenum-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wlogical-not-parentheses
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
    -Wno-error=unused-but-set-variable"

CFLAGS="${CFLAGS_COMMON}"

# Options
for arg in "$@"; do
    declare "$arg"="1";
done

if   [[ "${clang:-0}" == "1" ]]; then echo "[compiler: clang]"; CFLAGS="${CFLAGS} ${CFLAGS_CLANG}";
elif [[ "${gcc:-1}" == "1" ]];   then echo "[compiler: gcc]";   CFLAGS="${CFLAGS} ${CFLAGS_GCC}";
fi

if   [[ "${debug:-0}" == "1" ]];   then echo "[mode: debug]"; CFLAGS="${CFLAGS} ${CFLAGS_DEBUG}";
elif [[ "${release:-1}" == "1" ]]; then echo "[mode: release]"; CFLAGS="${CFLAGS} ${CFLAGS_RELEASE}";
fi

if   [[ "${x86:-0}" == "1" ]]; then echo "[architecture: x86]"; CFLAGS="${CFLAGS} ${CFLAGS_X86}";
elif [[ "${x64:-1}" == "1" ]]; then echo "[architecture: x64]"; CFLAGS="${CFLAGS} ${CFLAGS_X64}";
fi

if [[ "${shared:-0}" == "1" ]]; then
    echo "[target: shared]";
    CFLAGS="${CFLAGS} ${CFLAGS_SHARED}";
    EXTENSION=".so"
elif [[ "${static:-1}" == "1" ]]; then
    echo "[target: static]";
    CFLAGS="${CFLAGS} ${CFLAGS_STATIC}";
    EXTENSION=".a"
fi

# Compilation
SOURCES=src/memmi.c
BUILD_DIR="build"
BIN="${BUILD_DIR}/memmi"
EXTENSION=""

BIN="${BIN}${EXTENSION}"

cd "$(dirname "$0")"
mkdir -p "${BUILD_DIR}";

echo "[compiling...]"

${CC} ${CFLAGS} ${SOURCES} -o ${BIN}.o;
compile_result="$?";

if [[ "${compile_result}" == "0" ]]; then
    if [[ "${SHARED}" == "0" ]]; then
        ar rcs ${BIN} ${BIN}.o;
    else
        mv ${BIN}.o ${BIN};
    fi

    compile_result="$?"
fi

if [[ "${compile_result}" == "0" ]]; then
    echo "[successfully built '${BIN}']"
else
    echo "[failed to build '${BIN}']"
fi

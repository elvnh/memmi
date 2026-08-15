#!/usr/bin/env bash

# TODO: allow building as shared library

# Options
CC="${CC:-gcc}"
DEBUG="${DEBUG:-0}"
ARCH="${ARCH:-""}"
SHARED="${SHARED:-0}"

# Compiler flags
CFLAGS_DEBUG="-ggdb -fsanitize=address,undefined -DMEMMI_DEBUG=1"
CFLAGS_RELEASE="-DMEMMI_DEBUG=0"

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

# Compilation
SOURCES=src/memmi.c
BUILD_DIR="build"
BIN="${BUILD_DIR}/memmi"
EXTENSION=""

echo "[compiler: ${CC}]";
if   [[ "${CC}" == "gcc" ]];   then CFLAGS="${CFLAGS} ${CFLAGS_GCC}";
elif [[ "${CC}" == "clang" ]]; then CFLAGS="${CFLAGS} ${CFLAGS_CLANG}";
fi

if   [[ "${DEBUG}" == "1" ]]; then echo "[mode: debug]"; CFLAGS="${CFLAGS} ${CFLAGS_DEBUG}";
elif [[ "${DEBUG}" == "0" ]]; then echo "[mode: release]"; CFLAGS="${CFLAGS} ${CFLAGS_RELEASE}";
else
    echo "[error: Invalid argument for option DEBUG.]";
    exit 1
fi

if [[ "${ARCH}" == "" ]]; then
    bits=$(getconf LONG_BIT)
    if   [[ "${bits}" == "64" ]]; then ARCH="x64";
    elif [[ "${bits}" == "32" ]]; then ARCH="x86";
    else
        echo "[error: Could not get platform architecture, please provide manually]";
        exit 1;
    fi
fi

if   [[ "${ARCH}" == "x64" ]]; then echo "[architecture: x64]"; CFLAGS="${CFLAGS} ${CFLAGS_X64}";
elif [[ "${ARCH}" == "x86" ]]; then echo "[architecture: x86]"; CFLAGS="${CFLAGS} ${CFLAGS_X86}";
else
    echo "[error: Invalid argument for option ARCH.]";
    exit 1
fi

if   [[ "${SHARED}" == "1" ]]; then
    echo "[target: shared]";
    CFLAGS="${CFLAGS} ${CFLAGS_SHARED}";
    EXTENSION=".so"
elif [[ "${SHARED}" == "0" ]]; then
    echo "[target: static]";
    CFLAGS="${CFLAGS} ${CFLAGS_STATIC}";
    EXTENSION=".a"
else
    echo "[error: Invalid argument for option ARCH.]";
    exit 1
fi

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

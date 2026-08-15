#!/usr/bin/env bash

# TODO: allow building as shared library

CC="${CC:-gcc}"
CFLAGS_DEBUG="-ggdb -fsanitize=address,undefined -DMEMMI_DEBUG=1"
CFLAGS_RELEASE="-DMEMMI_DEBUG=0"

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

echo "[compiler: ${CC}]";
if   [[ "${CC}" == "gcc" ]];   then CFLAGS="${CFLAGS} ${CFLAGS_GCC}";
elif [[ "${CC}" == "clang" ]]; then CFLAGS="${CFLAGS} ${CFLAGS_CLANG}";
fi

DEBUG="${DEBUG:-0}"
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

SOURCES=src/memmi.c
BUILD_DIR="build"
BIN="${BUILD_DIR}/memmi.a"

cd "$(dirname "$0")"
mkdir -p "${BUILD_DIR}";

echo "[compiling...]"

${CC} ${CFLAGS} ${SOURCES} -o ${BIN}.o -c &&
    ar rcs ${BIN} ${BIN}.o &&
    echo "[successfully built '${BIN}']"

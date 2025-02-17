#!/bin/bash

BUILD_DIR="build"
BUILD_TYPE=0 # 0: debug, 1: release

if [ ! -d $BUILD_DIR ]; then
    mkdir $BUILD_DIR
fi

if argument is passed, set build type
if [ $# -eq 1 ]; then
    if [ $1 -eq 1 ]; then
        BUILD_TYPE=1
    else
        echo "Invalid argument. Usage: ./build.sh [0|1]"
        exit 1
    fi
fi

CC=gcc
LDFLAGS="-lm"

CFLAGS="-Wall -Wextra -Werror -pedantic -std=c99"
# disable some warnings
CFLAGS="$CFLAGS -Wno-unused-function"
# optimization flags
if [ $BUILD_TYPE -eq 0 ]; then
    CFLAGS="$CFLAGS -O0 -g"
else
    CFLAGS="$CFLAGS -O3 -march=native -flto -fwhole-program"
fi

cd $BUILD_DIR
$CC $CFLAGS ../src/rtn_main.c -I../src $LDFLAGS -o rtn
cd ..

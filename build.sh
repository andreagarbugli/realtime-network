#!/bin/bash

if [ ! -d "build" ]; then
    mkdir build
fi

CC=gcc
LDFLAGS="-lm"

CFLAGS="-Wall -Wextra -Werror -pedantic -std=c99"
# disable some warnings
CFLAGS="$CFLAGS -Wno-unused-function"
# optimization flags
CFLAGS="$CFLAGS -g -march=native -flto -fwhole-program"

cd build
$CC $CFLAGS ../src/rtn_main.c -I../src $LDFLAGS -o main
cd ..
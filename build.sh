#!/bin/bash

if [ ! -d "build" ]; then
    mkdir build
fi

CC=gcc
CFLAGS="-Wall -Wextra -Werror -pedantic -std=c99"
LDFLAGS="-lm"

# optimization flags
CFLAGS="$CFLAGS -O3 -march=native -flto -fwhole-program"

cd build
$CC $CFLAGS ../main.c $LDFLAGS -o main
cd ..
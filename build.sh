#!/bin/sh

set -e

CC="${1}gcc"
FLAGS="-Wall -Wextra -Werror -g -std=gnu99 -I./ext/raylib/include -L./ext/raylib/lib -I./ext/strap/src -L./ext/strap"
LIBS="-l:libraylib.a -lm -lpthread -ldl -lrt -lstrap"

if test ! -f ext/strap/libstrap.a; then
    ext/strap/build.sh && cd .
fi

$CC $FLAGS -o mus2 src/main.c $LIBS

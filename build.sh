#!/bin/sh

set -e

if [[ "t${1}" == "twindows" ]]; then
    CC="x86_64-w64-mingw32-gcc"
    FLAGS="-Wall -Wextra -Werror -g -std=gnu99 -I./ext/raylib-w/include -L./ext/raylib-w/lib -I./ext/strap/src -L./ext/strap"
    LIBS="-l:libraylib.a -lgdi32 -lwinmm -lstrap"
    ext/strap/build.sh && cd .
else
    CC="gcc"
    FLAGS="-Wall -Wextra -Werror -g -std=gnu99 -I./ext/raylib/include -L./ext/raylib/lib -I./ext/strap/src -L./ext/strap"
    LIBS="-l:libraylib.a -lm -lpthread -ldl -lrt -lstrap"
    ext/strap/build.sh && cd .
fi

$CC $FLAGS -o mus2 src/main.c $LIBS

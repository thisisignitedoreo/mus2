#!/bin/sh

set -e

if test ! -f "src/assets.h"; then
    echo "Bundling assets..."
    python bundle.py
fi

if test ! -f "ext/strap/libstrap.a"; then
    echo "Building libstrap..."
    ext/strap/build.sh && cd .
fi

echo "Building mus2..."
if [[ "t${1}" == "twindows" ]]; then
    CC="x86_64-w64-mingw32-gcc"
    FLAGS="-Wall -Wextra -Werror -g -std=gnu99 -I./ext/raylib-w/include -L./ext/raylib-w/lib -I./ext/strap/src -L./ext/strap"
    LIBS="-l:libraylib.a -lgdi32 -lwinmm -lstrap"
elif [[ "t${1}" == "twindows-release" ]]; then
    CC="x86_64-w64-mingw32-gcc"
    FLAGS="-Wall -Wextra -Werror -g -mwindows -std=gnu99 -I./ext/raylib-w/include -L./ext/raylib-w/lib -I./ext/strap/src -L./ext/strap"
    LIBS="-l:libraylib.a -lgdi32 -lwinmm -lstrap"
else
    CC="gcc"
    FLAGS="-Wall -Wextra -Werror -g -std=gnu99 -I./ext/raylib/include -L./ext/raylib/lib -I./ext/strap/src -L./ext/strap"
    LIBS="-l:libraylib.a -lm -lpthread -ldl -lrt -lstrap"
fi

$CC $FLAGS -o mus2 src/main.c $LIBS

#!/bin/sh

set -eu

CC="clang"
CCFLAGS="-Wall -Wextra -Wpedantic -Wno-strict-prototypes -g"

LIBRARIES="-lX11 -lasound"
SOURCES="linux.c"

$CC $CCFLAGS -o Game $SOURCES $LIBRARIES

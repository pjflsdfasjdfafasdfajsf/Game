#!/bin/sh

set -eu

CC="clang"
CCFLAGS="-Wall -Wextra -Wpedantic -Wno-strict-prototypes -g"

WAYLAND_PROTOCOLS=$(pkg-config --variable=pkgdatadir wayland-protocols)
XDG_SHELL_XML="$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml"

if [ ! -f "xdg-shell-client-protocol.h" ]; then
    echo "Generating xdg-shell-client-protocol.h"
    wayland-scanner client-header "$XDG_SHELL_XML" xdg-shell-client-protocol.h

    echo "Generating xdg-shell-client-protocol.c"
    wayland-scanner private-code "$XDG_SHELL_XML" xdg-shell-client-protocol.c
fi

LIBRARIES="-lwayland-client -lasound -pthread"
SOURCES="linux.c xdg-shell-client-protocol.c"

$CC $CCFLAGS -o Game $SOURCES $LIBRARIES


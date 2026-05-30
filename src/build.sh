#!/bin/sh

set -eu

CC="clang"
CCFLAGS="-g -Wall -Wextra -Wpedantic -Wno-strict-prototypes -g"
DEBUG_FLAGS="-g -fsanitize=address -DDEBUG"

WAYLAND_PROTOCOLS=$(pkg-config --variable=pkgdatadir wayland-protocols)
XDG_SHELL_XML="$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml"

if [ ! -f "xdg-shell-client-protocol.h" ]; then
    echo "Generating xdg-shell-client-protocol.h"
    wayland-scanner client-header "$XDG_SHELL_XML" xdg-shell-client-protocol.h

    echo "Generating xdg-shell-client-protocol.c"
    wayland-scanner private-code "$XDG_SHELL_XML" xdg-shell-client-protocol.c
fi

# Shader compilation
glslc glsl/BasicGeometry.vert -o BasicGeometry.vert.spv
glslc glsl/BasicGeometry.frag -o BasicGeometry.frag.spv

# Asset preprocess
$CC -o AssetPreprocess game_asset_preprocess.c
./AssetPreprocess BasicGeometry.vert.spv BasicGeometry.vert.h
./AssetPreprocess BasicGeometry.frag.spv BasicGeometry.frag.h

rm BasicGeometry.vert.spv
rm BasicGeometry.frag.spv

LIBRARIES="-lwayland-client -lvulkan -lasound -pthread"
SOURCES="linux.c linux_vulkan.c xdg-shell-client-protocol.c"

$CC $CCFLAGS -o Game $SOURCES $LIBRARIES


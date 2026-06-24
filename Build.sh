#!/bin/sh
#
# NOTE: Linux compilation script.
# 
set -e

# NOTE: Dirs.

AssetsDir="Assets"
BuildDir="Build"
ToolsDir="${BuildDir}/Tools"

mkdir -p ${BuildDir} \
         ${ToolsDir}

# NOTE: This is for printing.
Align="%-15s"

# NOTE: Compiler & Archiver.

Compiler="clang"
Ar="ar"
CommonCompilerFlags="-ICode/Public -I${BuildDir}"

VendorLibs="Ext/SDL3/Bin/Linux/libSDL3.a Ext/WAMR/Bin/Linux/libiwasm.a"
SystemLibs="-lm -lpthread -ldl -lrt -lstdc++"

LinkerFlags="${VendorLibs} ${SystemLibs}"

#
# NOTE: Atlas
# 

AtlasPackSrc="Code/Host/AtlasPack.c \
              Code/Host/STB.c
             "
AtlasPackTarget="${ToolsDir}/AtlasPack"
AtlasPackFlags="${CommonCompilerFlags} -IExt/STB -O3 -march=native"
AtlasPackLinkerFlags="-lm"

GameAtlas="${BuildDir}/GameAtlas"

ShouldPack=0

if [ "$1" = "Pack" ]; then
    ShouldPack=1
fi

# NOTE: If the atlas does not exist we do not care about what user passed in and
# we should create it
if [ ! -f ${AtlasPackTarget} ] || [ ! -f ${GameAtlas}.h ]; then
    ShouldPack=1
fi

if [ ${ShouldPack} -eq 1 ]; then
    printf ${Align} "Sprite Atlas"

    ${Compiler} ${AtlasPackFlags} ${AtlasPackSrc} ${AtlasPackLinkerFlags} -o ${AtlasPackTarget}

    ${AtlasPackTarget} ${GameAtlas} ${AssetsDir}/Images/*
    printf "Done\n"
fi

#
# NOTE: SDK
#

printf ${Align} "SDK"
SDKSrc="Code/Public/Init.c   \
        Code/Public/Mem.c    \
        Code/Public/Render.c \
        Code/Public/Math.c 
       "
SDKTarget="${BuildDir}/libSDK.a"
SDKFlags="${CommonCompilerFlags}"

${Compiler} ${SDKFlags} -c ${SDKSrc}
${Ar} rcs ${SDKTarget} Init.o Mem.o Render.o Math.o
rm -f Init.o Mem.o Render.o Math.o

SDKWasmTarget="${BuildDir}/libSDK_wasm.a"
SDKWasmFlags="${CommonCompilerFlags} --target=wasm32 -nostdlib"

${Compiler} ${SDKWasmFlags} -c ${SDKSrc}
${Ar} rcs ${SDKWasmTarget} Init.o Mem.o Render.o Math.o
rm -f Init.o Mem.o Render.o Math.o

printf "Done\n"

#
# NOTE: Host
#

printf ${Align} "Host"
HostSrc="Code/Host/Main.c    \
         Code/Host/Runtime.c \
         Code/Host/STB.c     \
         Code/Host/SDL.c
        "
HostTarget="${BuildDir}/Game"
HostFlags="${CommonCompilerFlags} -IExt/WAMR/Include -IExt/SDL3/Include -IExt/STB"

${Compiler} ${HostFlags} ${HostSrc} ${SDKTarget} ${LinkerFlags} -o ${HostTarget}
printf "Done\n"

#
# NOTE: Game
#

printf ${Align} "Game"
GameSrc="Code/Game/Game.c"
GameTarget="${BuildDir}/Game.wasm"
GameFlags="${CommonCompilerFlags} --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"

${Compiler} ${GameFlags} ${GameSrc} -Wl,--whole-archive ${SDKWasmTarget} -Wl,--no-whole-archive -o ${GameTarget}
printf "Done\n"

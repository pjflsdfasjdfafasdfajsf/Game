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

# NOTE: Compiler.

Compiler="clang"
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
# NOTE: Compressor
#

printf ${Align} "Compressor"
SimpleCompressSrc="Code/Host/SimpleCompress.c"
SimpleCompressTarget="${BuildDir}/SimpleCompress"
SimpleCompressFlags="${CommonCompilerFlags} -O3 -march=native"

${Compiler} ${SimpleCompressFlags} ${SimpleCompressSrc} -o ${SimpleCompressTarget}
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

${Compiler} ${HostFlags} ${HostSrc} ${LinkerFlags} -o ${HostTarget}
printf "Done\n"

#
# NOTE: Game
#

printf ${Align} "Game"
GameSrc="Code/Game/Game.c   \
         Code/Public/Init.c
        "
GameTarget="${BuildDir}/Game.wasm"
GameFlags="${CommonCompilerFlags} --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"

${Compiler} ${GameFlags} ${GameSrc} -o ${GameTarget}
printf "Done\n"

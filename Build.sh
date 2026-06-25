#!/bin/sh
#
# NOTE: Linux compilation script.
# 
set -e

# NOTE: Dirs.

AssetsDir="Assets"
BuildDir="Build"
ToolsDir="${BuildDir}/Tools"
TestDir="${BuildDir}/Test"
TestDataDir="${TestDir}/Data"

mkdir -p ${BuildDir}    \
         ${ToolsDir}    \
         ${TestDir}     \
         ${TestDataDir}

# NOTE: This is for printing.
Align="%-15s"

# NOTE: Compiler & Archiver.

Compiler="clang"
Ar="ar"
Zip="zip"
CommonCompilerFlags="-ICode/Public -I${BuildDir}"

VendorLibs="Ext/SDL3/Bin/Linux/libSDL3.a Ext/WAMR/Bin/Linux/libiwasm.a"
SystemLibs="-lm -lpthread -ldl -lrt -lstdc++"

LinkerFlags="${VendorLibs} ${SystemLibs}"

# TODO: Have our own Zip packaging tool
if ! command -v ${Zip} > /dev/null 2>&1; then
    echo "Your system is missing 'zip' for game packaging."
    exit 1
fi

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
         Code/Host/SDL.c     \
         Code/Host/Zip.c
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
GameUncompressedTarget="${BuildDir}/Game.wasm"
GameCompressedTarget="${BuildDir}/Game.zip"
GameFlags="${CommonCompilerFlags} --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"

${Compiler} ${GameFlags} ${GameSrc} -Wl,--whole-archive ${SDKWasmTarget} -Wl,--no-whole-archive -o ${GameUncompressedTarget}
${Zip} -9 -r ${GameCompressedTarget} ${GameUncompressedTarget} >/dev/null

rm -f ${GameUncompressedTarget}

printf "Done\n"

#
# NOTE: Tests
#

ShouldTest=0

if [ "$1" = "Test" ]; then
    ShouldTest=1
fi

if [ ${ShouldTest} -eq 1 ]; then
    printf ${Align} "Tests"

    Uncompressed="${TestDataDir}/Archive"
    mkdir -p ${Uncompressed}
    echo "Hi i love u <3 AAAAAAANBBBBBCBCCCBCBCABAAAABCBCBCBABA" > ${Uncompressed}/Hello.txt
    echo "Imagine this is a cool binary" > ${Uncompressed}/SomeAsset.bin

    Compressed="${Uncompressed}.zip"
    rm -f ${Compressed}
    ${Zip} -9 -r ${Compressed} ${Uncompressed} >/dev/null

    ZipTestSrc="Code/Host/Zip_Test.c \
                Code/Host/Zip.c      \
               "
    ZipTestTarget="${TestDir}/Zip"
    # TODO: Check for C23 support? Or perhaps just make AtlasPack more generic.
    # TODO SDL3 thingy 
    ZipTestFlags="${CommonCompilerFlags} -IExt/WAMR/Include -lSDL3 -std=c23 --embed-dir=${TestDataDir}"

    ${Compiler} ${ZipTestFlags} ${ZipTestSrc} ${SDKTarget} -o ${ZipTestTarget}

    printf "Done\n"
fi

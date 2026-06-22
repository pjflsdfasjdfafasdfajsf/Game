#
# NOTE: Linux compilation script.
# 
Compiler="clang"
CommonCompilerFlags="-ICode/Public"

VendorLibs="Ext/SDL3/Bin/Linux/libSDL3.a Ext/WAMR/Bin/Linux/libiwasm.a"
SystemLibs="-lm -lpthread -ldl -lrt -lstdc++"

LinkerFlags="${VendorLibs} ${SystemLibs}"

# NOTE: Dirs.

BuildDir="Build"
BinDir="${BuildDir}/Bin"

mkdir -p ${BuildDir} \
         ${BinDir}

#
# NOTE: Host
#

HostSrc="Code/Host/Main.c    \
         Code/Host/Runtime.c \
         Code/Host/STB.c     \
         Code/Host/SDL.c
        "
HostTarget="${BinDir}/Host"
HostFlags="${CommonCompilerFlags} -IExt/WAMR/Include -IExt/SDL3/Include -IExt/STB"

${Compiler} ${HostFlags} ${HostSrc} ${LinkerFlags} -o ${HostTarget}

#
# NOTE: Game
#

GameSrc="Code/Game/Game.c   \
         Code/Public/Init.c
        "
GameTarget="${BinDir}/Game.wasm"
GameFlags="${CommonCompilerFlags} --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"

${Compiler} ${GameFlags} ${GameSrc} -o ${GameTarget}

echo "Done"

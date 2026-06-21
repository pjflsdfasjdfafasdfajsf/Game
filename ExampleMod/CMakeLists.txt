# TODO: Get rid of that once mods can be built through the game
cmake_minimum_required(VERSION 3.16)
project(ExampleMod C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(ExampleMod Source/Main.c ../Public/Init.c)

set_target_properties(ExampleMod PROPERTIES SUFFIX ".wasm")
target_compile_options(ExampleMod PRIVATE -nostdlib)
target_include_directories(ExampleMod PRIVATE ../Public)
target_link_options(ExampleMod PRIVATE
    -nostdlib
    -fuse-ld=lld
    -Wl,--no-entry
    -Wl,--export-dynamic
    -Wl,--allow-undefined
)

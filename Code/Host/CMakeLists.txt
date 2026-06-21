cmake_minimum_required(VERSION 3.16)
project(App C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ca67f19c8f161cfff26ee52c57ff2ae8b9d42ef6
)

FetchContent_Declare(
    WAMR
    GIT_REPOSITORY https://github.com/bytecodealliance/wasm-micro-runtime.git
    GIT_TAG        26756a5c5846c262c9e5c89f3fc8e1e693ee2539
)

set(WAMR_BUILD_FAST_JIT 1 CACHE BOOL "Enable Fast JIT" FORCE)
set(WAMR_BUILD_INTERP 0 CACHE BOOL "Disable Interpreter" FORCE)
set(WAMR_BUILD_LIBC_BUILTIN 0 CACHE BOOL "Disable LibC" FORCE)
set(WAMR_BUILD_SIMD 0 CACHE BOOL "Disable SIMD" FORCE)

FetchContent_MakeAvailable(SDL3 WAMR)


###############################################
# Watch
###############################################

add_custom_target(Copy
    COMMAND ${CMAKE_COMMAND} --build "${GAME_BINARY_DIR}" --target Game
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GAME_BINARY_DIR}/Game.wasm"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Game.wasm"
)

###

add_executable(Watcher
    Watcher.c
)

target_include_directories(Watcher PRIVATE ../Public)

add_custom_target(Watch
    COMMAND $<TARGET_FILE:Watcher>
            "${CMAKE_CURRENT_SOURCE_DIR}/../Game"
            "${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target Copy"
    DEPENDS Watcher
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    USES_TERMINAL 
)

###############################################
# Executable
###############################################

add_executable(App
    Main.c
    SDL.c
    Runtime.c
)

target_include_directories(App PRIVATE
    ${wamr_SOURCE_DIR}/core/iwasm/include
    ../Public
)

target_compile_options(App PRIVATE
    -Wall -Wextra
    -Wno-attributes
    -Wno-unused-parameter
    
    $<$<CONFIG:Debug>:-ggdb3>
    $<$<CONFIG:Debug>:-Og>
)

target_link_options(App PRIVATE
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(App PRIVATE SDL3::SDL3 vmlib m)
add_custom_command(TARGET App POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GAME_BINARY_DIR}/Game.wasm"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Game.wasm"
)

cmake_minimum_required(VERSION 3.16)
project(App C)

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
    WASM3
    GIT_REPOSITORY https://github.com/wasm3/wasm3.git
    GIT_TAG        79d412ea5fcf92f0efe658d52827a0e0a96ff442 # Last commit 2 years ago :pensive:.
)

FetchContent_MakeAvailable(SDL3 WASM3)

###############################################

add_library(CommonOptions INTERFACE)

target_include_directories(CommonOptions INTERFACE
    Public
)

target_compile_options(CommonOptions INTERFACE
    -Wall -Wextra
    -Wno-attributes
    -Wno-unused-parameter
    
    $<$<CONFIG:Debug>:-ggdb3>
    $<$<CONFIG:Debug>:-Og>
)

###############################################
# Library
###############################################

add_library(Game SHARED
    Code/App.c
)

set_target_properties(Game PROPERTIES 
    PREFIX ""
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(Game PRIVATE CommonOptions)

###############################################
# Watch
###############################################

add_executable(WatchExe
    Code/Watch.c
)

target_link_libraries(WatchExe PRIVATE CommonOptions)

add_custom_target(Watch
    COMMAND $<TARGET_FILE:WatchExe> 
            "${CMAKE_CURRENT_SOURCE_DIR}/Code" 
            "${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target Game"
    DEPENDS WatchExe
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    USES_TERMINAL 
)

###############################################
# Executable
###############################################

add_executable(App
    Code/Main.c
    Code/SDL.c
    Code/Host.c
)

target_link_options(App PRIVATE
    $<$<CONFIG:Release>:-s>
)

target_include_directories(App PRIVATE
    ${wasm3_SOURCE_DIR}/source
)

target_link_libraries(App PRIVATE SDL3::SDL3 m3 m CommonOptions)

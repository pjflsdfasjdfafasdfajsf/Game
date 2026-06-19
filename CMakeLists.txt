cmake_minimum_required(VERSION 3.16)
project(tiramisu C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        ca67f19c8f161cfff26ee52c57ff2ae8b9d42ef6 # latest commit as of June 18 2026
)

FetchContent_Declare(
    wasm3
    GIT_REPOSITORY https://github.com/wasm3/wasm3.git
    GIT_TAG        79d412ea5fcf92f0efe658d52827a0e0a96ff442
)

FetchContent_MakeAvailable(SDL3 wasm3)

file(GLOB SOURCES "src/*.cpp")

add_executable(tiramisu ${SOURCES})

target_include_directories(tiramisu PRIVATE
    include
    ${wasm3_SOURCE_DIR}/source
)

target_compile_options(tiramisu PRIVATE
    -Wall -Wextra
    -Wno-attributes
    -Wno-unused-parameter
    -fno-exceptions
    -fno-rtti
    
    $<$<CONFIG:Release>:-fno-unwind-tables>
    $<$<CONFIG:Release>:-fno-asynchronous-unwind-tables>
    
    $<$<CONFIG:Debug>:-ggdb3>
    $<$<CONFIG:Debug>:-Og>
)

target_link_options(tiramisu PRIVATE
    -fno-exceptions
    
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(tiramisu PRIVATE SDL3::SDL3 m3)

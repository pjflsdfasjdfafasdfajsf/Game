cmake_minimum_required(VERSION 3.16)
project(example-mod C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(example-mod src/main.c)
add_executable(dependency src/dependency.c)

foreach(target example-mod dependency)
    set_target_properties(${target} PROPERTIES SUFFIX ".wasm")
    target_compile_options(${target} PRIVATE -nostdlib)
    target_include_directories(${target} PRIVATE ../include)
    target_link_options(${target} PRIVATE
        -nostdlib
        -Wl,--no-entry
        -Wl,--export-dynamic
        -Wl,--allow-undefined
    )
endforeach()

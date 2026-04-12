cmake_minimum_required(VERSION 3.20)
project(NDM_test LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")

set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

add_compile_options(
    -Wall
    -Werror
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "src/*.c")
add_executable(server ${SERVER_SOURCES})
target_precompile_headers(server PRIVATE "src/pch.h")
target_compile_definitions(server PRIVATE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:RELEASE> _GNU_SOURCE)

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

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "src/server/*.c")
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "src/client/*.c")

add_executable(server WIN32 ${SERVER_SOURCES})
add_executable(client WIN32 ${CLIENT_SOURCES})

target_precompile_headers(server PRIVATE "src/server/pch.h")
target_precompile_headers(client PRIVATE "src/client/pch.h")

target_compile_definitions(server PRIVATE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:RELEASE>)
target_compile_definitions(client PRIVATE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:RELEASE>)

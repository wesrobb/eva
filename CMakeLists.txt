cmake_minimum_required(VERSION 3.15)

project(eva)

add_executable(eva main.c eva_macos.m)
target_link_libraries(eva "-framework Cocoa -framework Metal -framework MetalKit")
target_compile_options(eva PRIVATE -g)

add_custom_target(
    copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

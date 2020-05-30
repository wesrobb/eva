cmake_minimum_required(VERSION 3.15)

project(learn-appkit)

add_executable(learn-appkit main.m)
target_link_libraries(learn-appkit "-framework Cocoa")
target_compile_options(learn-appkit PRIVATE -g)

add_custom_target(
    copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

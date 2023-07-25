cmake_minimum_required(VERSION 3.15)

project(eva)

if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    add_executable(eva main.c eva_macos.m)
    target_compile_definitions(eva PRIVATE EVA_MACOS)
    target_link_libraries(eva "-framework Cocoa -framework Metal -framework MetalKit")
    target_compile_options(eva PRIVATE -g)
elseif(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(eva WIN32 main.c eva.h eva_windows.c)
    target_compile_definitions(eva PRIVATE EVA_WINDOWS)
endif()


# add_custom_target(
#     copy-compile-commands ALL
#     ${CMAKE_COMMAND} -E copy_if_different
#         ${CMAKE_BINARY_DIR}/compile_commands.json
#         ${CMAKE_CURRENT_LIST_DIR})
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.20)

project(md
    VERSION 0.1.0
    DESCRIPTION "A terminal Markdown renderer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(md
    src/main.cpp
    src/inline_parser.cpp
    src/block_parser.cpp
    src/renderer.cpp
    src/highlight.cpp
    src/terminal.cpp)

target_include_directories(md PRIVATE src)

target_compile_options(md PRIVATE
    -Wall -Wextra -Wpedantic
    # Partial designated-initializers (aggregate {.kind = ...}) are intentional;
    # the rest of BlockNode's members correctly value-initialize.
    -Wno-missing-field-initializers)

include(GNUInstallDirs)
install(TARGETS md RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

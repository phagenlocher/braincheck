cmake_minimum_required(VERSION 3.27.1)

project(braincheck
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Brainfuck Model Checker"
    )

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-unused-command-line-argument)
add_compile_options(-g -O3)
add_compile_options(-pthread) # Spot is being built with pthread

add_subdirectory(src)

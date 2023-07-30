cmake_minimum_required(VERSION 3.18.4)

project(braincheck
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Brainfuck Model Checker"
    )

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_options(-Wall -Wextra -pedantic -Werror)
add_compile_options(-O3)
add_compile_options(-pthread) # Spot is being built with pthread

add_subdirectory(src)
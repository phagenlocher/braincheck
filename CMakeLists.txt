cmake_minimum_required(VERSION 3.27.1)

# Emit compile_commands.json from the (in-source) build directory so clangd and
# other editors resolve the target include paths. Already covered by .gitignore.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(braincheck
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "Brainfuck Model Checker"
    )

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-unused-command-line-argument)
add_compile_options(-O3)

# Link a fully static executable: libc/libm plus libstdc++ and libgcc come from
# their static archives, so `ldd braincheck` reports nothing and the binary has
# no runtime dependencies.
add_link_options(-static -static-libstdc++ -static-libgcc)

add_subdirectory(src)

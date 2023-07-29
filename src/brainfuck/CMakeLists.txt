add_library(
    brainfuck STATIC
    program.cpp
    kripke.cpp
    model.cpp
    analysis.cpp
)

target_include_directories(brainfuck PRIVATE /usr/local/include)
target_link_directories(brainfuck PRIVATE /usr/local/lib)
target_link_libraries(brainfuck spot)
target_link_libraries(brainfuck bddx)
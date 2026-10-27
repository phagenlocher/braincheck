add_library(
    brainfuck STATIC
    analysis.cpp
    kripke.cpp
    memory_map.cpp
    model.cpp
    program.cpp
)

target_include_directories(brainfuck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../extern/spotlib/include)
target_link_directories(brainfuck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../extern/spotlib/lib)
target_link_libraries(brainfuck spot)
target_link_libraries(brainfuck bddx)
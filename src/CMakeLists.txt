if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
endif()

if(CMAKE_CXX_STANDARD LESS 17)
    message(WARNING "Incompatible C++ standard ${CMAKE_CXX_STANDARD}. braincheck requires C++17")
endif()

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(extern)
add_subdirectory(brainfuck)
add_subdirectory(argparse)

# Internal
target_include_directories(${PROJECT_NAME} PRIVATE brainfuck)
target_link_directories(${PROJECT_NAME} PRIVATE brainfuck)
target_link_libraries(${PROJECT_NAME} brainfuck)

target_include_directories(${PROJECT_NAME} PRIVATE argparse)
target_link_directories(${PROJECT_NAME} PRIVATE argparse)
target_link_libraries(${PROJECT_NAME} argparse)

# External
target_include_directories(${PROJECT_NAME} PRIVATE /usr/local/include)
target_link_directories(${PROJECT_NAME} PRIVATE /usr/local/lib)
target_link_libraries(${PROJECT_NAME} spot)
target_link_libraries(${PROJECT_NAME} bddx)

set(CLI11_TESTING OFF)
add_subdirectory(extern/CLI11)
mark_as_advanced(CLI_CXX_STD CLI_EXAMPLES CLI_SINGLE_FILE CLI_SINGLE_FILE_TESTS CLI_TESTING)
target_include_directories(argparse PRIVATE extern/CLI11)
target_include_directories(argparse PRIVATE brainfuck)
target_link_libraries(argparse PRIVATE CLI11::CLI11)
target_link_libraries(argparse PRIVATE brainfuck)

# Tests
add_compile_options(-lrt -lm)
add_executable(tests EXCLUDE_FROM_ALL tests.cpp)
target_include_directories(tests PRIVATE brainfuck)
target_include_directories(tests PRIVATE extern/minunit)
target_link_directories(tests PRIVATE extern/minunit)
target_link_libraries(tests PRIVATE brainfuck)
# Not using CTest since minunit does everything we need
add_custom_target(test)
add_custom_command(TARGET test POST_BUILD COMMAND tests)
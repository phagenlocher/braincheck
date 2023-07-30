find_program (AUTORECONF autoreconf)
if (NOT AUTORECONF)
    message (FATAL_ERROR "Missing autoreconf. Install autoconf package and try again.")
endif ()

find_program (AUTOMAKE automake)
if (NOT AUTOMAKE)
    message (FATAL_ERROR "Missing automake. Install autoconf package and try again.")
endif ()

find_program (LIBTOOLIZE NAMES libtoolize glibtoolize)
if (NOT LIBTOOLIZE)
    message (FATAL_ERROR "Missing libtoolize. Install libtool package and try again.")
endif ()

find_program (PATCH NAMES patch)
if (NOT PATCH)
    message (FATAL_ERROR "Missing patch. Install patch package and try again.")
endif ()

message(STATUS "Configuring spot in ${CMAKE_CURRENT_SOURCE_DIR}/spot")

execute_process(
    COMMAND autoreconf -vfi
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spot
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND ./configure --disable-devel --disable-python --enable-shared=no --enable-static=yes --enable-pthread --prefix "${CMAKE_CURRENT_SOURCE_DIR}/spotlib/"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spot
    COMMAND_ERROR_IS_FATAL ANY
)

# Yes, I know this is very error-prone and will probably break but if this is
# what it takes to not install latex dependencies then the risk is worth it!
execute_process(
    COMMAND patch ${CMAKE_CURRENT_SOURCE_DIR}/spot/Makefile ${CMAKE_CURRENT_SOURCE_DIR}/spot-makefile.patch
    COMMAND_ERROR_IS_FATAL ANY
)

message(STATUS "Building spot in ${CMAKE_CURRENT_SOURCE_DIR}/spot")

execute_process(
    COMMAND make
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spot
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND make install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spot
    COMMAND_ERROR_IS_FATAL ANY
)

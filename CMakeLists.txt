#build file for CMake, see http://www.cmake.org/

PROJECT(pawn)

ADD_SUBDIRECTORY(./compiler ${CMAKE_BINARY_DIR})
ADD_SUBDIRECTORY(./amx ${CMAKE_BINARY_DIR})

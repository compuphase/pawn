#build file for CMake, see http://www.cmake.org/

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(pawnc)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# check for optional include files
INCLUDE(${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)
CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
IF(HAVE_UNISTD_H)
  ADD_DEFINITIONS(-DHAVE_UNISTD_H)
ENDIF(HAVE_UNISTD_H)
CHECK_INCLUDE_FILE("inttypes.h" HAVE_INTTYPES_H)
IF(HAVE_INTTYPES_H)
  ADD_DEFINITIONS(-DHAVE_INTTYPES_H)
ENDIF(HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
IF(HAVE_STDINT_H)
  ADD_DEFINITIONS(-DHAVE_STDINT_H)
ENDIF(HAVE_STDINT_H)
CHECK_INCLUDE_FILE("alloca.h" HAVE_ALLOCA_H)
IF(HAVE_ALLOCA_H)
  ADD_DEFINITIONS(-DHAVE_ALLOCA_H)
ENDIF(HAVE_ALLOCA_H)

IF(UNIX)
  ADD_DEFINITIONS(-DENABLE_BINRELOC -D_GNU_SOURCE)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../linux)
  LINK_LIBRARIES(pthread m) #CMake adds the "lib" prefix, so "pthread" becomes "libpthread"
ENDIF(UNIX)
IF(WIN32)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)
IF(APPLE)
  # OS X defaults to 64-bit build, but 64-bit issues prevent that being a good idea
  SET(CMAKE_OSX_ARCHITECTURES "i386")
ENDIF(APPLE)

# The Pawn compiler
SET(PAWNCC_SRCS sc1.c sc2.c sc3.c sc4.c sc5.c sc6.c sc7.c
	scexpand.c sci18n.c sclist.c scmemfil.c scstate.c scvars.c
	lstring.c memfile.c
	${CMAKE_CURRENT_SOURCE_DIR}/../amx/keeloq.c)
IF(WIN32)
  SET(PAWNCC_SRCS ${PAWNCC_SRCS} pawncc.rc)
ENDIF(WIN32)
IF(UNIX)
  SET(PAWNCC_SRCS ${PAWNCC_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/../linux/binreloc.c)
ENDIF(UNIX)

FILE(READ svnrev.h SVNREV_STR)
STRING(REGEX REPLACE ".*#define[ \t]+SVN_REV[ \t]+([0-9]+).*" "\\1" PAWNC_REV "${SVNREV_STR}")
FILE(READ sc1.c SC1_STR)
STRING(REGEX REPLACE ".*#define[ \t]+VERSION_STR[ \t]+\"([0-9]+).*" "\\1" PAWNC_MAJOR "${SC1_STR}")
STRING(REGEX REPLACE ".*#define[ \t]+VERSION_STR[ \t]+\"([0-9]+).([0-9]+).*" "\\2" PAWNC_MINOR "${SC1_STR}")

SET(PAWNC_VER "${PAWNC_MAJOR}.${PAWNC_MINOR}.${PAWNC_REV}")

ADD_LIBRARY(pawnc SHARED ${PAWNCC_SRCS})
TARGET_COMPILE_DEFINITIONS(pawnc PUBLIC NO_MAIN)
IF(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  TARGET_COMPILE_DEFINITIONS(pawnc PUBLIC HAVE_VISIBILITY)
  TARGET_COMPILE_OPTIONS(pawnc PUBLIC -fvisibility=hidden)
ENDIF()
SET_TARGET_PROPERTIES(pawnc PROPERTIES
	SOVERSION "${PAWNC_MAJOR}"
	VERSION "${PAWNC_VER}")

CONFIGURE_FILE(pawnc.pc.in pawnc.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/pawnc.pc 
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

ADD_EXECUTABLE(pawncc ${PAWNCC_SRCS})

# Utility to create GraphViz "dot" files from the XML report (if the report contains states)
SET(STATEGRAPH_SRCS stategraph.c ezxml.c)
ADD_EXECUTABLE(stategraph ${STATEGRAPH_SRCS})
TARGET_COMPILE_DEFINITIONS(stategraph PUBLIC EZXML_NOMMAP)

# Simple Pawn disassembler
SET(PAWNDISASM_SRCS pawndisasm.c)
ADD_EXECUTABLE(pawndisasm ${PAWNDISASM_SRCS})

INSTALL(TARGETS pawnc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL(TARGETS pawncc stategraph pawndisasm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES sc.h memfile.h DESTINATION include/pawn/compiler)
INSTALL(FILES ../linux/sclinux.h ../linux/getch.h DESTINATION include/pawn/linux)

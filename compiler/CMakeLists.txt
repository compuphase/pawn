#build file for CMake, see http://www.cmake.org/

PROJECT(pawnc)

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
  ADD_DEFINITIONS(-DLINUX -DENABLE_BINRELOC -g)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../linux)
  LINK_LIBRARIES(pthread) #CMake adds the "lib" prefix, so "pthread" becomes "libpthread"
ENDIF(UNIX)

# The Pawn compiler shared library
SET(PAWNC_SRCS sc1.c sc2.c sc3.c sc4.c sc5.c sc6.c sc7.c
	scexpand.c sci18n.c sclist.c scmemfil.c scstate.c scvars.c
	lstring.c memfile.c libpawnc.c)
SET_SOURCE_FILES_PROPERTIES(sc1.c COMPILE_FLAGS -DNO_MAIN)
IF(WIN32)
  SET(PAWNC_SRCS ${PAWNC_SRCS} libpawnc.rc)
  SET_SOURCE_FILES_PROPERTIES(libpawnc.c COMPILE_FLAGS -DPAWNC_DLL)
  IF(BORLAND)
    # Borland linker uses a DEF file if one is in the output directory
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libpawnc.def.borland ${CMAKE_BINARY_DIR}/pawnc.def COPY_ONLY)
  ELSE(BORLAND)
    # Microsoft Visual C/C++ supports a DEF file as if it were a source file
    SET(PAWNC_SRCS ${PAWNC_SRCS} libpawnc.def)
  ENDIF(BORLAND)
ENDIF(WIN32)
IF(UNIX)
  SET(PAWNC_SRCS ${PAWNC_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/../linux/binreloc.c)
ENDIF(UNIX)
ADD_LIBRARY(pawnc SHARED ${PAWNC_SRCS})
IF(WATCOM)  #Watcom C/C++ does not support a .DEF file for the exports
  SET_TARGET_PROPERTIES(pawnc PROPERTIES LINK_FLAGS "/exp=libpawnc")
ENDIF(WATCOM)
IF(UNIX)
  # Unix exports all symbols by default; we want only a very select few exported symbols
  ADD_CUSTOM_COMMAND(TARGET pawnc POST_BUILD COMMAND strip ARGS -K pc_compile -K pc_addconstant -K pc_addtag -K pc_enablewarning ${CMAKE_BINARY_DIR}/libpawnc.so)
ENDIF(UNIX)


# The Pawn compiler driver (console program)
SET(PAWNCC_SRCS pawncc.c)
IF(WIN32)
  SET(PAWNCC_SRCS ${PAWNCC_SRCS} libpawnc.rc)
  IF(BORLAND)
    # Borland linker uses a DEF file if one is in the output directory
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/pawncc.def.borland ${CMAKE_BINARY_DIR}/pawncc.def COPY_ONLY)
  ELSE(BORLAND)
    # Microsoft Visual C/C++ supports a DEF file as if it were a source file
    SET(PAWNC_SRCS ${PAWNC_SRCS} pawncc.def)
  ENDIF(BORLAND)
ENDIF(WIN32)
ADD_EXECUTABLE(pawncc ${PAWNCC_SRCS})
TARGET_LINK_LIBRARIES(pawncc pawnc)

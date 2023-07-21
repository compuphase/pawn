# - Find the dyncall and dynload include files and libraries
#
#  Dyncall_FOUND - system has dyncall
#  Dyncall_INCLUDE_DIR - the dyncall include directory
#  DYNLOAD_INCLUDE_DIR - the dynload include directory
#  Dyncall_LIBRARIES - The libraries needed to use dyncall and dynload
#  Dyncall_DEFINITIONS - Compiler switches required for using dyncall
#

IF (Dyncall_INCLUDE_DIR AND Dyncall_LIBRARIES)
  # Already in cache, be silent
  SET(Dyncall_FIND_QUIETLY TRUE)
ENDIF (Dyncall_INCLUDE_DIR AND Dyncall_LIBRARIES)

FIND_PATH(Dyncall_INCLUDE_DIR dyncall.h
  PATH_SUFFIXES dyncall include
  PATHS
  ~/dyncall
  /usr/local
  /usr
  /opt/dyncall/include
  /opt/local
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.8
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.8
)

FIND_PATH(DYNLOAD_INCLUDE_DIR dynload.h
  PATH_SUFFIXES dynload include
  PATHS
  ~/dyncall
  /usr/local
  /usr
  /opt/dyncall/include
  /opt/local
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.8
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.8
)

FIND_LIBRARY(Dyncall_LIBRARIES
  NAMES dyncall dyncall_s libdyncall libdyncall_s dyncall_s.lib libdyncall_s.lib
  PATH_SUFFIXES dyncall lib lib/dyncall lib/dyncall/Debug lib/dyncall/Release
  PATHS
  ~/dyncall
  /usr/local
  /usr
  /opt/dyncall
  /opt/local
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.8
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.8
)

FIND_LIBRARY(Dyncall_EXTRA_LIBRARY
  NAMES dynload dynload_s libdynload libdynload_s dynload.lib dynload_s.lib libdynload_s.lib
  PATH_SUFFIXES dynload lib lib/dynload lib/dynload/Debug lib/dynload/Release
  PATHS
  ~/dyncall
  /usr/local
  /usr
  /opt/dyncall
  /opt/local
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/dyncall-0.8
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.7
  ${CMAKE_CURRENT_SOURCE_DIR}/../dyncall-0.8
)

IF(Dyncall_EXTRA_LIBRARY)
  SET(Dyncall_LIBRARIES ${Dyncall_LIBRARIES} ${Dyncall_EXTRA_LIBRARY})
ENDIF(Dyncall_EXTRA_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set Dyncall_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Dyncall DEFAULT_MSG Dyncall_LIBRARIES Dyncall_INCLUDE_DIR )

MARK_AS_ADVANCED(Dyncall_LIBRARIES Dyncall_INCLUDE_DIR )


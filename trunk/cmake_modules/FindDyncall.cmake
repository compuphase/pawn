# - Find the dyncall and dynload include files and libraries
#
#  DYNCALL_FOUND - system has dyncall
#  DYNCALL_INCLUDE_DIR - the dyncall include directory
#  DYNLOAD_INCLUDE_DIR - the dynload include directory
#  DYNCALL_LIBRARIES - The libraries needed to use dyncall and dynload
#  DYNCALL_DEFINITIONS - Compiler switches required for using dyncall
#

IF (DYNCALL_INCLUDE_DIR AND DYNCALL_LIBRARIES)
  # Already in cache, be silent
  SET(DYNCALL_FIND_QUIETLY TRUE)
ENDIF (DYNCALL_INCLUDE_DIR AND DYNCALL_LIBRARIES)

FIND_PATH(DYNCALL_INCLUDE_DIR dyncall.h
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

FIND_LIBRARY(DYNCALL_LIBRARIES
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

FIND_LIBRARY(DYNCALL_EXTRA_LIBRARY
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

IF(DYNCALL_EXTRA_LIBRARY)
  SET(DYNCALL_LIBRARIES ${DYNCALL_LIBRARIES} ${DYNCALL_EXTRA_LIBRARY})
ENDIF(DYNCALL_EXTRA_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set DYNCALL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DYNCALL DEFAULT_MSG DYNCALL_LIBRARIES DYNCALL_INCLUDE_DIR )

MARK_AS_ADVANCED(DYNCALL_LIBRARIES DYNCALL_INCLUDE_DIR )


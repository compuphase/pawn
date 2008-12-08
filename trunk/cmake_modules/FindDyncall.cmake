# - Find the dyncall and dynload include files and libraries
#
#  DYNCALL_FOUND - system has dyncall
#  DYNCALL_INCLUDE_DIR - the dyncall include directory
#  DYNCALL_LIBRARIES - The libraries needed to use dyncall and dynload
#  DYNCALL_DEFINITIONS - Compiler switches required for using dyncall
#

IF (DYNCALL_INCLUDE_DIR AND DYNCALL_LIBRARIES)
  # Already in cache, be silent
  SET(DYNCALL_FIND_QUIETLY TRUE)
ENDIF (DYNCALL_INCLUDE_DIR AND DYNCALL_LIBRARIES)

FIND_PATH(DYNCALL_INCLUDE_DIR dyncall.h
   PATH_SUFFIXES include
   PATHS
   ~/dyncall
   /usr/local
   /usr
   /opt/dyncall/include
   /opt/local
)

FIND_LIBRARY(DYNCALL_LIBRARIES NAMES dyncall dyncall_s libdyncall libdyncall_s
  PATH_SUFFIXES lib
  PATHS
  ~/dyncall
  /usr/local
  /usr
  /opt/dyncall
  /opt/local
)

GET_FILENAME_COMPONENT(_dyncallLibDir "${DYNCALL_LIBRARIES}" PATH)
FIND_LIBRARY(DYNCALL_EXTRA_LIBRARY NAMES dynload dynload_s libdynload libdynload_s
  HINTS "${_dyncallLibDir}"
)

IF(DYNCALL_EXTRA_LIBRARY)
  SET(DYNCALL_LIBRARIES ${DYNCALL_LIBRARIES} ${DYNCALL_EXTRA_LIBRARY})
ENDIF(DYNCALL_EXTRA_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set DYNCALL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DYNCALL DEFAULT_MSG DYNCALL_LIBRARIES DYNCALL_INCLUDE_DIR )

MARK_AS_ADVANCED(DYNCALL_LIBRARIES DYNCALL_INCLUDE_DIR )


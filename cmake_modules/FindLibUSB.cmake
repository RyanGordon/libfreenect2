# - Find libusb for portable USB support
# 
# If the LibUSB_ROOT environment variable
# is defined, it will be used as base path.
# The following standard variables get defined:
#  LibUSB_FOUND:    true if LibUSB was found
#  LibUSB_INCLUDE_DIR: the directory that contains the include file
#  LibUSB_LIBRARIES:  the libraries

#IF(PKG_CONFIG_FOUND)
#  IF(DEPENDS_DIR) #Otherwise use System pkg-config path
#    SET(ENV{PKG_CONFIG_PATH} "${DEPENDS_DIR}/libusb/lib/pkgconfig")
#  ENDIF()
#  SET(MODULE "libusb-1.0")
#  IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
#    SET(MODULE "libusb-1.0>=1.0.20")
#  ENDIF()
#  IF(LibUSB_FIND_REQUIRED)
#    SET(LibUSB_REQUIRED "REQUIRED")
#  ENDIF()
#  PKG_CHECK_MODULES(LibUSB ${LibUSB_REQUIRED} ${MODULE})
#
#  FIND_LIBRARY(LibUSB_LIBRARY
#    NAMES ${LibUSB_LIBRARIES}
#    HINTS ${LibUSB_LIBRARY_DIRS}
#  )
#  SET(LibUSB_LIBRARIES ${LibUSB_LIBRARY})
#
#  RETURN()
#ENDIF()

FIND_PATH(LibUSB_INCLUDE_DIRS
  NAMES libusb.h
  PATHS
    "${DEPENDS_DIR}/libusb"
    "${DEPENDS_DIR}/libusbx"
    "/usr/local/opt/libusb" # homebrew
    ENV LibUSB_ROOT
  PATH_SUFFIXES
    include
    libusb
    include/libusb-1.0
)

FIND_LIBRARY(LibUSB_LIBRARIES
  NAMES libusb-1.0
  PATHS
    "${DEPENDS_DIR}/libusb"
    "${DEPENDS_DIR}/libusbx"
    "/usr/local/opt/libusb" # homebrew
    ENV LibUSB_ROOT
  PATH_SUFFIXES
    lib
    lib64
)

IF(WIN32)
FIND_FILE(LibUSB_DLL
  libusb-1.0.dll
  PATHS
    "${DEPENDS_DIR}/libusb"
    "${DEPENDS_DIR}/libusbx"
    ENV LibUSB_ROOT
  PATH_SUFFIXES
    x64/Release/dll
    x64/Debug/dll
    Win32/Release/dll
    Win32/Debug/dll
    MS64
    MS64/dll
)
ENDIF()

IF(LibUSB_INCLUDE_DIRS AND LibUSB_LIBRARIES)
set(CMAKE_REQUIRED_INCLUDES ${LibUSB_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${LibUSB_LIBRARIES})
set(CMAKE_REQUIRED_DEFINITIONS)
set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibUSB DEFAULT_MSG LibUSB_LIBRARIES LibUSB_INCLUDE_DIRS)

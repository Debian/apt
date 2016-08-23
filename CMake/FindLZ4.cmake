# - Try to find LZ4
# Once done, this will define
#
#  LZ4_FOUND - system has LZ4
#  LZ4_INCLUDE_DIRS - the LZ4 include directories
#  LZ4_LIBRARIES - the LZ4 library
find_package(PkgConfig)

pkg_check_modules(LZ4_PKGCONF liblz4)

find_path(LZ4_INCLUDE_DIRS
  NAMES lz4frame.h
  PATHS ${LZ4_PKGCONF_INCLUDE_DIRS}
)


find_library(LZ4_LIBRARIES
  NAMES lz4
  PATHS ${LZ4_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZ4 DEFAULT_MSG LZ4_INCLUDE_DIRS LZ4_LIBRARIES)

mark_as_advanced(LZ4_INCLUDE_DIRS LZ4_LIBRARIES)

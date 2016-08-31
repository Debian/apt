# - Try to find LZMA
# Once done, this will define
#
#  LZMA_FOUND - system has LZMA
#  LZMA_INCLUDE_DIRS - the LZMA include directories
#  LZMA_LIBRARIES - the LZMA library
find_package(PkgConfig)

pkg_check_modules(LZMA_PKGCONF liblzma)

find_path(LZMA_INCLUDE_DIRS
  NAMES lzma.h
  PATHS ${LZMA_PKGCONF_INCLUDE_DIRS}
)


find_library(LZMA_LIBRARIES
  NAMES lzma
  PATHS ${LZMA_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZMA DEFAULT_MSG LZMA_INCLUDE_DIRS LZMA_LIBRARIES)

mark_as_advanced(LZMA_INCLUDE_DIRS LZMA_LIBRARIES)

# - Try to find ZSTD
# Once done, this will define
#
#  ZSTD_FOUND - system has ZSTD
#  ZSTD_INCLUDE_DIRS - the ZSTD include directories
#  ZSTD_LIBRARIES - the ZSTD library
find_package(PkgConfig)

pkg_check_modules(ZSTD_PKGCONF libzstd)

find_path(ZSTD_INCLUDE_DIRS
  NAMES zstd.h
  PATHS ${ZSTD_PKGCONF_INCLUDE_DIRS}
)


find_library(ZSTD_LIBRARIES
  NAMES zstd
  PATHS ${ZSTD_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZSTD DEFAULT_MSG ZSTD_INCLUDE_DIRS ZSTD_LIBRARIES)

mark_as_advanced(ZSTD_INCLUDE_DIRS ZSTD_LIBRARIES)

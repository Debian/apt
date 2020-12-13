# - Try to find XXHASH
# Once done, this will define
#
#  XXHASH_FOUND - system has XXHASH
#  XXHASH_INCLUDE_DIRS - the XXHASH include directories
#  XXHASH_LIBRARIES - the XXHASH library
find_package(PkgConfig)

pkg_check_modules(XXHASH_PKGCONF libxxhash)

find_path(XXHASH_INCLUDE_DIRS
  NAMES xxhash.h
  PATHS ${XXHASH_PKGCONF_INCLUDE_DIRS}
)


find_library(XXHASH_LIBRARIES
  NAMES xxhash
  PATHS ${XXHASH_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XXHASH DEFAULT_MSG XXHASH_INCLUDE_DIRS XXHASH_LIBRARIES)

mark_as_advanced(XXHASH_INCLUDE_DIRS XXHASH_LIBRARIES)

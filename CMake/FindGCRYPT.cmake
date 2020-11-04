# - Try to find GCRYPT
# Once done, this will define
#
#  GCRYPT_FOUND - system has GCRYPT
#  GCRYPT_INCLUDE_DIRS - the GCRYPT include directories
#  GCRYPT_LIBRARIES - the GCRYPT library
find_package(PkgConfig)

pkg_check_modules(GCRYPT_PKGCONF libgcrypt)

find_path(GCRYPT_INCLUDE_DIRS
  NAMES gcrypt.h
  PATHS ${GCRYPT_PKGCONF_INCLUDE_DIRS}
)


find_library(GCRYPT_LIBRARIES
  NAMES gcrypt
  PATHS ${GCRYPT_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GCRYPT DEFAULT_MSG GCRYPT_INCLUDE_DIRS GCRYPT_LIBRARIES)

mark_as_advanced(GCRYPT_INCLUDE_DIRS GCRYPT_LIBRARIES)

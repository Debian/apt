# - Try to find SECCOMP
# Once done, this will define
#
#  SECCOMP_FOUND - system has SECCOMP
#  SECCOMP_INCLUDE_DIRS - the SECCOMP include directories
#  SECCOMP_LIBRARIES - the SECCOMP library
find_package(PkgConfig)

pkg_check_modules(SECCOMP_PKGCONF libseccomp)

find_path(SECCOMP_INCLUDE_DIRS
  NAMES seccomp.h
  PATHS ${SECCOMP_PKGCONF_INCLUDE_DIRS}
)


find_library(SECCOMP_LIBRARIES
  NAMES seccomp
  PATHS ${SECCOMP_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SECCOMP DEFAULT_MSG SECCOMP_INCLUDE_DIRS SECCOMP_LIBRARIES)

mark_as_advanced(SECCOMP_INCLUDE_DIRS SECCOMP_LIBRARIES)

# - Try to find SYSTEMD
# Once done, this will define
#
#  SYSTEMD_FOUND - system has SYSTEMD
#  SYSTEMD_INCLUDE_DIRS - the SYSTEMD include directories
#  SYSTEMD_LIBRARIES - the SYSTEMD library
find_package(PkgConfig)

pkg_check_modules(SYSTEMD_PKGCONF libsystemd)

find_path(SYSTEMD_INCLUDE_DIRS
  NAMES systemd/sd-bus.h
  PATHS ${SYSTEMD_PKGCONF_INCLUDE_DIRS}
)

find_library(SYSTEMD_LIBRARIES
  NAMES systemd
  PATHS ${SYSTEMD_PKGCONF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Systemd DEFAULT_MSG SYSTEMD_INCLUDE_DIRS SYSTEMD_LIBRARIES)

mark_as_advanced(SYSTEMD_INCLUDE_DIRS SYSTEMD_LIBRARIES)

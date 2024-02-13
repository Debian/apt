# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>.
# Licensed under the same terms as APT; i.e. GPL 2 or later.

# set minimum version
project(apt)
cmake_minimum_required(VERSION 3.4.0)
# Generic header locations
include_directories(${PROJECT_BINARY_DIR}/include)


enable_testing()

option(WITH_DOC "Build documentation." ON)
option(WITH_TESTS "Build tests" ON)
option(USE_NLS "Localisation support." ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")

# Add coverage target
set(CMAKE_CXX_FLAGS_COVERAGE "-g -fprofile-arcs -ftest-coverage")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "-lgcov")
set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "-lgcov")

# Work around bug in GNUInstallDirs
if (EXISTS "/etc/debian_version")
  set(CMAKE_INSTALL_LIBEXECDIR "lib")
endif()

# Include stuff
include(Misc)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(GNUInstallDirs)
include(TestBigEndian)
find_package(Threads REQUIRED)
find_package(LFS REQUIRED)
find_package(Iconv REQUIRED)

find_package(Perl REQUIRED)

find_program(TRIEHASH_EXECUTABLE NAMES triehash)

if (NOT TRIEHASH_EXECUTABLE)
  message(FATAL_ERROR "Could not find triehash executable")
endif()

if(USE_NLS)
  find_package(Intl REQUIRED)
  link_libraries(${Intl_LIBRARIES})
  include_directories(${Intl_INCLUDE_DIRS})
endif()

# Add large file support
add_compile_options(${LFS_COMPILE_OPTIONS})
add_definitions(${LFS_DEFINITIONS})
link_libraries(${LFS_LIBRARIES})

# Set compiler flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

add_optional_compile_options(Wall)
add_optional_compile_options(Wextra)
add_optional_compile_options(Wcast-align)
add_optional_compile_options(Wlogical-op)
add_optional_compile_options(Wredundant-decls)
add_optional_compile_options(Wmissing-declarations)
add_optional_compile_options(Wunsafe-loop-optimizations)
add_optional_compile_options(Wctor-dtor-privacy)
add_optional_compile_options(Wdisabled-optimization)
add_optional_compile_options(Winit-self)
add_optional_compile_options(Wmissing-include-dirs)
add_optional_compile_options(Wnoexcept)
add_optional_compile_options(Wsign-promo)
add_optional_compile_options(Wundef)
add_optional_compile_options(Wdouble-promotion)
add_optional_compile_options(Wsuggest-override)
add_optional_compile_options(Werror=suggest-override)
add_optional_compile_options(Werror=return-type)
# apt-ftparchive dependencies
find_package(Berkeley REQUIRED)
if (BERKELEY_FOUND)
  set(HAVE_BDB 1)
endif()

find_package(GnuTLS REQUIRED)
if (GNUTLS_FOUND)
  set(HAVE_GNUTLS 1)
endif()

# (De)Compressor libraries
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
  set(HAVE_ZLIB 1)
endif()


find_package(BZip2 REQUIRED)
if (BZIP2_FOUND)
  set(HAVE_BZ2 1)
endif()

find_package(LZMA REQUIRED)
if (LZMA_FOUND)
  set(HAVE_LZMA 1)
endif()


find_package(LZ4 REQUIRED)
if (LZ4_FOUND)
  set(HAVE_LZ4 1)
endif()

find_package(ZSTD)
if (ZSTD_FOUND)
  set(HAVE_ZSTD 1)
endif()


find_package(Udev)
if (UDEV_FOUND)
  set(HAVE_UDEV 1)
endif()

find_package(Systemd)
if (SYSTEMD_FOUND)
  set(HAVE_SYSTEMD 1)
endif()

find_package(SECCOMP)
if (SECCOMP_FOUND)
  set(HAVE_SECCOMP 1)
endif()

find_package(GCRYPT REQUIRED)
find_package(XXHASH REQUIRED)

# Mount()ing and stat()ing and friends
check_symbol_exists(statfs sys/vfs.h HAVE_VFS_H)
check_include_files(sys/params.h HAVE_PARAMS_H)
check_symbol_exists(statfs sys/mount.h HAVE_MOUNT_H)
if (NOT HAVE_VFS_H AND NOT HAVE_MOUNT_H)
  message(FATAL_ERROR "Can find neither statvfs() nor statfs()")
endif()

check_function_exists(statvfs HAVE_STATVFS)
if (NOT HAVE_STATVFS)
  configure_file(CMake/statvfs.h.in ${PROJECT_BINARY_DIR}/include/sys/statvfs.h COPYONLY)
endif()

CHECK_STRUCT_HAS_MEMBER("struct statfs" f_type sys/vfs.h HAVE_STRUCT_STATFS_F_TYPE)

# Other checks
check_function_exists(getresuid HAVE_GETRESUID)
check_function_exists(getresgid HAVE_GETRESGID)
check_function_exists(setresuid HAVE_SETRESUID)
check_function_exists(setresgid HAVE_SETRESGID)
check_function_exists(ptsname_r HAVE_PTSNAME_R)
check_function_exists(timegm HAVE_TIMEGM)
test_big_endian(WORDS_BIGENDIAN)

# FreeBSD
add_definitions(-D_WITH_GETLINE=1)

CHECK_INCLUDE_FILES(machine/endian.h HAVE_MACHINE_ENDIAN_H)
CHECK_INCLUDE_FILES(sys/endian.h HAVE_SYS_ENDIAN_H)
CHECK_INCLUDE_FILES(endian.h HAVE_ENDIAN_H)
if (NOT HAVE_ENDIAN_H)
  if (HAVE_MACHINE_ENDIAN_H OR HAVE_SYS_ENDIAN_H)
    configure_file(CMake/endian.h.in ${PROJECT_BINARY_DIR}/include/endian.h)
  else()
    message(FATAL_ERROR "Cannot find endian.h")
  endif()
endif()


include(CheckTypeSize)
set(CMAKE_EXTRA_INCLUDE_FILES "signal.h")
check_type_size("sig_t" SIG_T LANGUAGE "CXX")
check_type_size("sighandler_t" SIGHANDLER_T LANGUAGE "CXX")
set(CMAKE_EXTRA_INCLUDE_FILES)
if (NOT HAVE_SIGHANDLER_T)
  if (HAVE_SIG_T)
    add_definitions(-Dsighandler_t=sig_t)
  else()
    message(FATAL_ERROR "Platform defines neither sig_t nor sighandler_t")
  endif()
endif()

# Handle resolving
check_function_exists(res_ninit HAVE_LIBC_RESOLV)
if(HAVE_LIBC_RESOLV)
    set(RESOLV_LIBRARIES)
else()
    set(RESOLV_LIBRARIES -lresolv)
endif()

# Configure some variables like package, version and architecture.
set(PACKAGE ${PROJECT_NAME})
set(PACKAGE_MAIL "APT Development Team <deity@lists.debian.org>")
set(PACKAGE_VERSION "2.4.12")
string(REGEX MATCH "^[0-9.]+" PROJECT_VERSION ${PACKAGE_VERSION})

if (NOT DEFINED DPKG_DATADIR)
  execute_process(COMMAND ${PERL_EXECUTABLE} -MDpkg -e "print $Dpkg::DATADIR;"
                  OUTPUT_VARIABLE DPKG_DATADIR_CMD OUTPUT_STRIP_TRAILING_WHITESPACE)
  message(STATUS "Found dpkg data dir: ${DPKG_DATADIR_CMD}")
  set(DPKG_DATADIR "${DPKG_DATADIR_CMD}" CACHE PATH "dpkg data directory")
endif()
if (NOT DEFINED COMMON_ARCH)
  execute_process(COMMAND dpkg-architecture -qDEB_HOST_ARCH
                  OUTPUT_VARIABLE COMMON_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if (NOT DEFINED ROOT_GROUP)
  execute_process(COMMAND id -gn root
                  OUTPUT_VARIABLE ROOT_GROUP OUTPUT_STRIP_TRAILING_WHITESPACE)
  message(STATUS "Found root group: ${ROOT_GROUP}")
endif()
set(ROOT_GROUP "${ROOT_GROUP}" CACHE STRING "Group of root (e.g.: wheel or root)")

# Set various directories
set(STATE_DIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/apt" CACHE PATH "Your /var/lib/apt")
set(CACHE_DIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/cache/apt" CACHE PATH "Your /var/cache/apt")
set(LOG_DIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/apt" CACHE PATH "Your /var/log/apt")
set(CONF_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/apt" CACHE PATH "Your /etc/apt")
set(LIBEXEC_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/apt" CACHE PATH "Your /usr/libexec/apt")
set(BIN_DIR "${CMAKE_INSTALL_FULL_BINDIR}")


# Configure our configuration headers (config.h and apti18n.h)
configure_file(CMake/config.h.in ${PROJECT_BINARY_DIR}/include/config.h)
configure_file(CMake/apti18n.h.in ${PROJECT_BINARY_DIR}/include/apti18n.h)

# Add our subdirectories
add_subdirectory(vendor)
add_subdirectory(apt-pkg)
add_subdirectory(apt-private)
add_subdirectory(cmdline)
add_subdirectory(completions)
add_subdirectory(doc)
add_subdirectory(dselect)
add_subdirectory(ftparchive)
add_subdirectory(methods)
add_subdirectory(test)

if (USE_NLS)
add_subdirectory(po)

# Link update-po4a into the update-po target
add_dependencies(update-po update-po4a)
endif()

# Create our directories.
install_empty_directories(
  ${CONF_DIR}/apt.conf.d
  ${CONF_DIR}/auth.conf.d
  ${CONF_DIR}/preferences.d
  ${CONF_DIR}/sources.list.d
  ${CONF_DIR}/trusted.gpg.d
  ${CACHE_DIR}/archives/partial
  ${STATE_DIR}/lists/partial
  ${STATE_DIR}/mirrors/partial
  ${STATE_DIR}/periodic
  ${LOG_DIR}
)

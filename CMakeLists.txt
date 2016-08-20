# Copyright (C) 2009, 2016 Julian Andres Klode <jak@debian.org>.
# Licensed under the same terms as APT; i.e. GPL 2 or later.

# set minimum version
project(apt)
cmake_minimum_required(VERSION 3.4.0)

enable_testing()

option(WITH_DOC "Build documentation." ON)
option(USE_NLS "Localisation support." ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")

# Work around bug in GNUInstallDirs
if (EXISTS "/etc/debian_version")
  set(CMAKE_INSTALL_LIBEXECDIR "lib")
endif()

# Include stuff
include(Misc)
include(Translations)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(GNUInstallDirs)
include(TestBigEndian)
find_package(Threads)
find_package(PkgConfig)
find_package(LFS REQUIRED)

# Add large file support
add_compile_options(${LFS_COMPILE_OPTIONS})
add_definitions(${LFS_DEFINITIONS})
link_libraries(${LFS_LIBRARIES})

# Set compiler flags
set(CMAKE_CXX_STANDARD 11)
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

# apt-ftparchive dependencies
find_package(BerkeleyDB REQUIRED)
if (BERKELEY_DB_FOUND)
  set(HAVE_BDB 1)
endif()


# apt-transport-https dependencies
find_package(CURL REQUIRED)
if (CURL_FOUND)
  set(HAVE_CURL 1)
endif()

# (De)Compressor libraries
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
  set(HAVE_ZLIB 1)
endif()


find_package(BZip2)
if (BZIP2_FOUND)
  set(HAVE_BZ2 1)
endif()

pkg_check_modules(LZMA liblzma)
if (LZMA_FOUND)
  set(HAVE_LZMA 1)
endif()

pkg_check_modules(LZ4 liblz4)
if (LZ4_FOUND)
  set(HAVE_LZ4 1)
endif()

# Mount()ing and stat()ing and friends

check_function_exists(statvfs HAVE_STATVFS)
if (NOT HAVE_STATVFS)
  check_symbol_exists(statfs sys/vfs.h HAVE_VFS_H)
  check_symbol_exists(statfs sys/mount.h HAVE_MOUNT_H)
  if (NOT HAVE_VFS_H AND NOT HAVE_MOUNT_H)
    message(FATAL_ERROR "Can find neither statvfs() nor statfs()")
  endif()
  configure_file(CMake/statvfs.h.in ${PROJECT_BINARY_DIR}/include/statvfs.h COPYONLY)
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

if (CMAKE_USE_PTHREADS_INIT)
  set(HAVE_PTHREAD 1)
endif()

# Configure some variables like package, version and architecture.
set(PACKAGE ${PROJECT_NAME})
set(PACKAGE_MAIL "APT Development Team <deity@lists.debian.org>")
set(PACKAGE_VERSION "1.3~rc2")

if (NOT DEFINED COMMON_ARCH)
  execute_process(COMMAND dpkg-architecture -qDEB_HOST_ARCH
                  OUTPUT_VARIABLE COMMON_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Configure our configuration headers (config.h and apti18n.h)
configure_file(CMake/config.h.in ${PROJECT_BINARY_DIR}/include/config.h)
configure_file(CMake/apti18n.h.in ${PROJECT_BINARY_DIR}/include/apti18n.h)

# Generic header locations
include_directories(${PROJECT_BINARY_DIR}/include)

# Add our subdirectories
add_subdirectory(vendor)
add_subdirectory(apt-pkg)
add_subdirectory(apt-private)
add_subdirectory(apt-inst)
add_subdirectory(cmdline)
add_subdirectory(completions)
add_subdirectory(doc)
add_subdirectory(dselect)
add_subdirectory(ftparchive)
add_subdirectory(methods)
add_subdirectory(po)
add_subdirectory(test)

# Link update-po4a into the update-po target
add_dependencies(update-po update-po4a)

# Create our directories.
install_empty_directories(
  ${CMAKE_INSTALL_FULL_SYSCONFDIR}/apt/apt.conf.d
  ${CMAKE_INSTALL_FULL_SYSCONFDIR}/apt/preferences.d
  ${CMAKE_INSTALL_FULL_SYSCONFDIR}/apt/sources.list.d
  ${CMAKE_INSTALL_FULL_SYSCONFDIR}/apt/trusted.gpg.d
  ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/cache/apt/archives/partial
  ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/apt/lists/partial
  ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/apt/mirrors/partial
  ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/apt/periodic
  ${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/apt
)

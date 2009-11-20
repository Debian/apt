# Copyright (C) 2009 Julian Andres Klode <jak@debian.org>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

# set minimum version
project(apt)
cmake_minimum_required(VERSION 2.6.0)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# Add our subdirectories
add_subdirectory(apt-pkg)
add_subdirectory(apt-inst)
add_subdirectory(cmdline)
add_subdirectory(ftparchive)
add_subdirectory(methods)


# Configuring the project
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)
find_package(Threads)


check_include_files(db.h HAVE_BDB)
check_include_files(sys/vfs.h HAVE_VFS_H)
check_include_files(sys/mount.h HAVE_MOUNT_H)
check_function_exists(timegm HAVE_TIMEGM)

if (CMAKE_USE_PTHREADS_INIT)
	set(HAVE_PTHREAD	 1)
endif(CMAKE_USE_PTHREADS_INIT)

# Helper macros and functions
macro(add_cxx_include result files)
  set(${result} "")
  foreach (file_name ${files})
     set(${result} "${${result}}#include<${file_name}>\n")
  endforeach()
endmacro(add_cxx_include files result)


function(check_type_exists type files variable)
  add_cxx_include(includes "${files}")
  CHECK_CXX_SOURCE_COMPILES("${includes} ${type} typeVar; int main() {}" ${variable})
endfunction()


check_type_exists(socklen_t sys/socket.h HAVE_SOCKLEN_T)

if (NOT HAVE_SOCKLEN_T)
	add_definitions(-Dsocklen_t=size_t)
endif(NOT HAVE_SOCKLEN_T)


option(USE_NLS "Localisation support." ON)
# Configure some variables like package, version and architecture.
set(PACKAGE "apt")
set(VERSION "0.7.90")
execute_process(COMMAND dpkg-architecture -qDEB_HOST_ARCH
                OUTPUT_VARIABLE COMMON_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)

# Configure our configuration headers (config.h and apti18n.h)
configure_file(buildlib/config.h.cmake ${PROJECT_BINARY_DIR}/include/config.h)
configure_file(buildlib/apti18n.h.cmake ${PROJECT_BINARY_DIR}/include/apti18n.h)

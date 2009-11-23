# Copyright (C) 2009 Julian Andres Klode <jak@debian.org>
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

# set minimum version
project(apt)
cmake_minimum_required(VERSION 2.6.0)


option(WITH_DOC "Build documentation." ON)
option(USE_NLS "Localisation support." ON)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# Configuring the project
include(buildlib/translations.cmake)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(TestBigEndian)
find_package(Threads)


# Check some include files
check_include_files(dxb.h HAVE_BDB)
if (NOT HAVE_BDB)
	message(FATAL_ERROR "Berkeley DB is missing (db.h)")
endif (NOT HAVE_BDB)
# Check for cURL
check_include_files(curl/curl.h HAVE_CURL)
if (NOT HAVE_CURL)
	message(FATAL_ERROR "cURL is missing (curl/curl.h)")
endif (NOT HAVE_CURL)

# Other checks
check_function_exists(timegm HAVE_TIMEGM)
test_big_endian(WORDS_BIGENDIAN)

if (CMAKE_USE_PTHREADS_INIT)
	set(HAVE_PTHREAD 1)
endif(CMAKE_USE_PTHREADS_INIT)

# Configure some variables like package, version and architecture.
set(PACKAGE "apt")

execute_process(COMMAND dpkg-parsechangelog -l${PROJECT_SOURCE_DIR}/debian/changelog
                COMMAND sed -nr "s/Version: (.*)/\\1/p"
                OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND dpkg-architecture -qDEB_HOST_ARCH
                OUTPUT_VARIABLE COMMON_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)

# Configure our configuration headers (config.h and apti18n.h)
configure_file(buildlib/config.h.cmake ${PROJECT_BINARY_DIR}/include/config.h)
configure_file(buildlib/apti18n.h.cmake ${PROJECT_BINARY_DIR}/include/apti18n.h)


# Add our subdirectories
add_subdirectory(apt-pkg)
add_subdirectory(apt-inst)
add_subdirectory(cmdline)
add_subdirectory(ftparchive)
add_subdirectory(methods)

# - Try to find Berkeley DB
# Once done this will define
#
#  BERKELEY_FOUND - system has Berkeley DB
#  BERKELEY_INCLUDE_DIRS - the Berkeley DB include directory
#  BERKELEY_LIBRARIES - Link these to use Berkeley DB
#  BERKELEY_DEFINITIONS - Compiler switches required for using Berkeley DB

# Copyright (c) 2006, Alexander Dymo, <adymo@kdevelop.org>
# Copyright (c) 2016, Julian Andres Klode <jak@debian.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# We need NO_DEFAULT_PATH here, otherwise CMake helpfully picks up the wrong
# db.h on BSD systems instead of the Berkeley DB one.
find_path(BERKELEY_INCLUDE_DIRS db.h
  ${CMAKE_INSTALL_FULL_INCLUDEDIR}/db5
  /usr/local/include/db5
  /usr/include/db5

  ${CMAKE_INSTALL_FULL_INCLUDEDIR}/db4
  /usr/local/include/db4
  /usr/include/db4

  ${CMAKE_INSTALL_FULL_INCLUDEDIR}
  /usr/local/include
  /usr/include

  NO_DEFAULT_PATH
)

find_library(BERKELEY_LIBRARIES NAMES db db-5)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Berkeley "Could not find Berkeley DB >= 4.1" BERKELEY_INCLUDE_DIRS BERKELEY_LIBRARIES)
# show the BERKELEY_INCLUDE_DIRS and BERKELEY_LIBRARIES variables only in the advanced view
mark_as_advanced(BERKELEY_INCLUDE_DIRS BERKELEY_LIBRARIES)

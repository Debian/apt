# -*- make -*-
BASE=..
SUBDIR=test

# Bring in the default rules
include ../buildlib/defaults.mak

# Program for testing methods
PROGRAM=mthdcat
SLIBS = 
SOURCE = mthdcat.cc
include $(PROGRAM_H)

# Scratch program to test incomplete code fragments in
PROGRAM=scratch-test
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = scratch.cc
include $(PROGRAM_H)

# Version compare tester
PROGRAM=versiontest
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = versiontest.cc
include $(PROGRAM_H)

# Version compare tester
PROGRAM=srcrfind
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = srcrfind.cc
include $(PROGRAM_H)

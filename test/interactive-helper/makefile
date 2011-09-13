# -*- make -*-
BASE=../..
SUBDIR=test/interactive-helper

# Bring in the default rules
include ../../buildlib/defaults.mak

# Program for testing methods
PROGRAM=mthdcat
SLIBS = 
SOURCE = mthdcat.cc
include $(PROGRAM_H)

# Version compare tester
PROGRAM=testextract
SLIBS = -lapt-pkg -lapt-inst
LIB_MAKES = apt-pkg/makefile apt-inst/makefile
SOURCE = testextract.cc
include $(PROGRAM_H)

# Program for testing the tar/deb extractor
PROGRAM=testdeb
SLIBS = -lapt-pkg -lapt-inst
SOURCE = testdeb.cc
include $(PROGRAM_H)

# Program for testing tar extraction
PROGRAM=extract-control
SLIBS = -lapt-pkg -lapt-inst
SOURCE = extract-control.cc
include $(PROGRAM_H)

# Program for testing udevcdrom
PROGRAM=test_udevcdrom
SLIBS = -lapt-pkg 
SOURCE = test_udevcdrom.cc
include $(PROGRAM_H)

# Program for checking rpm versions
#PROGRAM=rpmver
#SLIBS = -lapt-pkg -lrpm
#SOURCE = rpmver.cc
#include $(PROGRAM_H)

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

# Program for testing methods
PROGRAM=uritest
SLIBS = -lapt-pkg
SOURCE = uri.cc
include $(PROGRAM_H)

# Scratch program to test incomplete code fragments in
PROGRAM=scratch-test
SLIBS = -lapt-inst -lapt-pkg
LIB_MAKES = apt-pkg/makefile apt-inst/makefile
SOURCE = scratch.cc
include $(PROGRAM_H)

# Version compare tester
PROGRAM=versiontest
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = versiontest.cc
include $(PROGRAM_H)

# Version compare tester
PROGRAM=testextract
SLIBS = -lapt-pkg -lapt-inst
LIB_MAKES = apt-pkg/makefile apt-inst/makefile
SOURCE = testextract.cc
include $(PROGRAM_H)

# Program for testing the config file parser
PROGRAM=conftest_clear
SLIBS = -lapt-pkg
SOURCE = conf_clear.cc
include $(PROGRAM_H)

# Program for testing the config file parser
PROGRAM=conftest
SLIBS = -lapt-pkg
SOURCE = conf.cc
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

# Program for testing hashes
PROGRAM=hash
SLIBS = -lapt-pkg 
SOURCE = hash.cc
include $(PROGRAM_H)

# Program for checking rpm versions
PROGRAM=rpmver
SLIBS = -lapt-pkg -lrpm
SOURCE = rpmver.cc
include $(PROGRAM_H)

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
SOURCE = scratch.cc
include $(PROGRAM_H)

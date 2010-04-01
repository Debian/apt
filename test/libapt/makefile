# -*- make -*-
BASE=../..
SUBDIR=test/libapt
BASENAME=_libapt_test

# Bring in the default rules
include ../../buildlib/defaults.mak

# Program for testing getLanguageCode
PROGRAM = getLanguages${BASENAME}
SLIBS = -lapt-pkg
SOURCE = getlanguages_test.cc
include $(PROGRAM_H)

# Program for testing ParseDepends
PROGRAM = ParseDepends${BASENAME}
SLIBS = -lapt-pkg
SOURCE = parsedepends_test.cc
include $(PROGRAM_H)

# Program for testing GetListOfFilesInDir
PROGRAM = GetListOfFilesInDir${BASENAME}
SLIBS = -lapt-pkg
SOURCE = getlistoffilesindir_test.cc
include $(PROGRAM_H)

# Program for testing CommandLine reconstruction
PROGRAM = commandlineasstring${BASENAME}
SLIBS = -lapt-pkg
SOURCE = commandlineasstring_test.cc
include $(PROGRAM_H)

# Program for testing debians version comparing
PROGRAM = compareversion${BASENAME}
SLIBS = -lapt-pkg
SOURCE = compareversion_test.cc
include $(PROGRAM_H)

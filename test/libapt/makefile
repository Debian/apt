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

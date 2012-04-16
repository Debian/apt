# -*- make -*-
BASE=../..
SUBDIR=test/libapt
BASENAME=_libapt_test

# Bring in the default rules
include ../../buildlib/defaults.mak

.PHONY: test
test:
	./run-tests

# Program for testing getLanguageCode
PROGRAM = getLanguages${BASENAME}
SLIBS = -lapt-pkg
SOURCE = getlanguages_test.cc
include $(PROGRAM_H)

PROGRAM = getArchitectures${BASENAME}
SLIBS = -lapt-pkg
SOURCE = getarchitectures_test.cc
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
PROGRAM = CommandlineAsString${BASENAME}
SLIBS = -lapt-pkg
SOURCE = commandlineasstring_test.cc
include $(PROGRAM_H)

# Program for testing debians version comparing
PROGRAM = CompareVersion${BASENAME}
SLIBS = -lapt-pkg
SOURCE = compareversion_test.cc
include $(PROGRAM_H)

# test the GlobalError stack class
PROGRAM = GlobalError${BASENAME}
SLIBS = -lapt-pkg
SOURCE = globalerror_test.cc
include $(PROGRAM_H)

# test the different Hashsum classes
PROGRAM = HashSums${BASENAME}
SLIBS = -lapt-pkg
SOURCE = hashsums_test.cc
include $(PROGRAM_H)

# test the strutils stuff
PROGRAM = StrUtil${BASENAME}
SLIBS = -lapt-pkg
SOURCE = strutil_test.cc
include $(PROGRAM_H)

# test the URI parsing stuff
PROGRAM = URI${BASENAME}
SLIBS = -lapt-pkg
SOURCE = uri_test.cc
include $(PROGRAM_H)

# test the Configuration class
PROGRAM = Configuration${BASENAME}
SLIBS = -lapt-pkg
SOURCE = configuration_test.cc
include $(PROGRAM_H)

# test cdroms core FindPackages
PROGRAM = CdromFindPackages${BASENAME}
SLIBS = -lapt-pkg
SOURCE = cdromfindpackages_test.cc
include $(PROGRAM_H)

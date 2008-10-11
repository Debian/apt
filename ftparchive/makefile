# -*- make -*-
BASE=..
SUBDIR=ftparchive

# Bring in the default rules
include ../buildlib/defaults.mak

# The apt-ftparchive program
ifdef BDBLIB
PROGRAM=apt-ftparchive
SLIBS = -lapt-pkg -lapt-inst $(BDBLIB) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-inst/makefile
SOURCE = apt-ftparchive.cc cachedb.cc writer.cc contents.cc override.cc \
         multicompress.cc
include $(PROGRAM_H)
else
PROGRAM=apt-ftparchive
MESSAGE="Must have db2 to build apt-ftparchive"
include $(FAIL_H)
endif # ifdef BDBLIB

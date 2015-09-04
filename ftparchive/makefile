# -*- make -*-
BASE=..
SUBDIR=ftparchive

# Bring in the default rules
include ../buildlib/defaults.mak

# The apt-ftparchive program
ifdef BDBLIB
APT_DOMAIN:=apt-utils
PROGRAM=apt-ftparchive
SLIBS = -lapt-pkg -lapt-inst -lapt-private $(BDBLIB) $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-inst/makefile apt-private/makefile
SOURCE = apt-ftparchive.cc cachedb.cc writer.cc contents.cc override.cc \
         multicompress.cc sources.cc byhash.cc
include $(PROGRAM_H)
else
PROGRAM=apt-ftparchive
MESSAGE="Must have libdb to build apt-ftparchive"
include $(FAIL_H)
endif # ifdef BDBLIB

# -*- make -*-
BASE=..
SUBDIR=methods

# Bring in the default rules
include ../buildlib/defaults.mak
BIN := $(BIN)/methods

# The file method
PROGRAM=file
SLIBS = -lapt-pkg 
SOURCE = file.cc
include $(PROGRAM_H)

# The copy method
PROGRAM=copy
SLIBS = -lapt-pkg 
SOURCE = copy.cc
include $(PROGRAM_H)

# The gzip method
PROGRAM=gzip
SLIBS = -lapt-pkg 
SOURCE = gzip.cc
include $(PROGRAM_H)

# The cdrom method
PROGRAM=cdrom
SLIBS = -lapt-pkg 
SOURCE = cdrom.cc
include $(PROGRAM_H)

# The http method
PROGRAM=http
SLIBS = -lapt-pkg 
SOURCE = http.cc
include $(PROGRAM_H)

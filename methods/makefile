# -*- make -*-
BASE=..
SUBDIR=methods

# Bring in the default rules
include ../buildlib/defaults.mak
BIN := $(BIN)/methods

# The file method
PROGRAM=file
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = file.cc
include $(PROGRAM_H)

# The copy method
PROGRAM=copy
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = copy.cc
include $(PROGRAM_H)

# The gzip method
PROGRAM=gzip
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = gzip.cc
include $(PROGRAM_H)

# The cdrom method
PROGRAM=cdrom
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = cdrom.cc
include $(PROGRAM_H)

# The http method
PROGRAM=http
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = http.cc
include $(PROGRAM_H)

# The ftp method
PROGRAM=ftp
SLIBS = -lapt-pkg 
LIB_MAKES = apt-pkg/makefile
SOURCE = ftp.cc
include $(PROGRAM_H)

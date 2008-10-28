# -*- make -*-
BASE=..
SUBDIR=cmdline

# Bring in the default rules
include ../buildlib/defaults.mak

# The apt-cache program
PROGRAM=apt-cache
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-cache.cc
include $(PROGRAM_H)

# The apt-get program
PROGRAM=apt-get
SLIBS = -lapt-pkg -lutil $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-get.cc acqprogress.cc
include $(PROGRAM_H)

# The apt-config program
PROGRAM=apt-config
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-config.cc
include $(PROGRAM_H)

# The apt-cdrom program
PROGRAM=apt-cdrom
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-cdrom.cc 
include $(PROGRAM_H)

# The apt-sortpkgs program
PROGRAM=apt-sortpkgs
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-sortpkgs.cc
include $(PROGRAM_H)

# The apt-extracttemplates program
PROGRAM=apt-extracttemplates
SLIBS = -lapt-pkg -lapt-inst $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-extracttemplates.cc 
include $(PROGRAM_H)

# The apt-key program
SOURCE=apt-key
TO=$(BIN)
TARGET=program
include $(COPY_H)

# The apt-mark program
SOURCE=apt-mark
TO=$(BIN)
TARGET=program
include $(COPY_H)

# -*- make -*-
BASE=..
SUBDIR=cmdline

# Bring in the default rules
include ../buildlib/defaults.mak

# The apt-cache program
PROGRAM=apt-cache
SLIBS = -lapt-pkg
SOURCE = apt-cache.cc
include $(PROGRAM_H)

# The apt-get program
PROGRAM=apt-get
SLIBS = -lapt-pkg
SOURCE = apt-get.cc acqprogress.cc
include $(PROGRAM_H)

# The apt-config program
PROGRAM=apt-config
SLIBS = -lapt-pkg
SOURCE = apt-config.cc
include $(PROGRAM_H)

# The apt-cdrom program
PROGRAM=apt-cdrom
SLIBS = -lapt-pkg
SOURCE = apt-cdrom.cc
include $(PROGRAM_H)

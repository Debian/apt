# -*- make -*-
BASE=..
SUBDIR=apt-inst

# Header location
SUBDIRS = contrib deb
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name
LIBRARY=apt-inst
LIBEXT=$(GLIBC_VER)$(LIBSTDCPP_VER)
MAJOR=1.1
MINOR=0
SLIBS=$(PTHREADLIB) -lapt-pkg
APT_DOMAIN:=libapt-inst$(MAJOR)

# Source code for the contributed non-core things
SOURCE = contrib/extracttar.cc contrib/arfile.cc

# Source code for the main library
SOURCE+= filelist.cc database.cc dirstream.cc extract.cc \
         deb/dpkgdb.cc deb/debfile.cc

# Public header files
HEADERS = extracttar.h arfile.h filelist.h database.h extract.h \
          dpkgdb.h dirstream.h debfile.h

HEADERS := $(addprefix apt-pkg/,$(HEADERS))
include $(LIBRARY_H)

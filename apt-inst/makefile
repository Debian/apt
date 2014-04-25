# -*- make -*-
BASE=..
SUBDIR=apt-inst

# Header location
SUBDIRS = contrib deb
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name and version (indirectly used from init.h)
include ../buildlib/libversion.mak

# The library name
LIBRARY=apt-inst
MAJOR=1.5
MINOR=0
SLIBS=$(PTHREADLIB) -lapt-pkg
APT_DOMAIN:=libapt-inst$(MAJOR)
LIBRARYDEPENDS=$(LIB)/libapt-pkg.so

SOURCE = $(wildcard *.cc */*.cc)
HEADERS = $(addprefix apt-pkg/,$(notdir $(wildcard *.h */*.h)))

include $(LIBRARY_H)

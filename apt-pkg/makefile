# -*- make -*-
BASE=..
SUBDIR=apt-pkg

# Header location
SUBDIRS = deb edsp contrib
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name and version (indirectly used from init.h)
include ../buildlib/libversion.mak

CPPFLAGS+=-DAPT_PKG_EXPOSE_STRING_VIEW
LIBRARY=apt-pkg
MAJOR=$(LIBAPTPKG_MAJOR)
MINOR=$(LIBAPTPKG_RELEASE)
SLIBS=$(PTHREADLIB) $(INTLLIBS) -lutil -ldl -lresolv
ifeq ($(HAVE_ZLIB),yes)
SLIBS+= -lz
endif
ifeq ($(HAVE_BZ2),yes)
SLIBS+= -lbz2
endif
ifeq ($(HAVE_LZMA),yes)
SLIBS+= -llzma
endif
ifeq ($(HAVE_LZ4),yes)
SLIBS+= -llz4
endif
APT_DOMAIN:=libapt-pkg$(LIBAPTPKG_MAJOR)

SOURCE = $(sort $(wildcard *.cc */*.cc))
HEADERS = $(addprefix apt-pkg/,$(notdir $(sort $(wildcard *.h */*.h))))

include $(LIBRARY_H)

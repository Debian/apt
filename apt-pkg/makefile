# -*- make -*-
BASE=..
SUBDIR=apt-pkg

# Header location
SUBDIRS = deb contrib
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name
LIBRARY=apt-pkg
MAJOR=2.0
MINOR=0
SLIBS=$(PTHREADLIB)

# Source code for the contributed non-core things
SOURCE = contrib/mmap.cc contrib/error.cc contrib/strutl.cc \
         contrib/configuration.cc contrib/progress.cc contrib/cmndline.cc

# Source code for the main library
SOURCE+= pkgcache.cc version.cc fileutl.cc pkgcachegen.cc depcache.cc \
         orderlist.cc tagfile.cc sourcelist.cc packagemanager.cc \
	 pkgrecords.cc algorithms.cc init.cc templates.cc
	 
# Source code for the debian specific components	 
SOURCE+= deb/deblistparser.cc deb/debrecords.cc

# Public apt-pkg header files
HEADERS = algorithms.h depcache.h mmap.h pkgcachegen.h cacheiterators.h \
          error.h orderlist.h sourcelist.h configuration.h fileutl.h \
          packagemanager.h tagfile.h deblistparser.h init.h pkgcache.h \
          version.h progress.h pkgrecords.h debrecords.h cmndline.h
HEADERS := $(addprefix apt-pkg/,$(HEADERS))

# Private header files
HEADERS+= strutl.h system.h

include $(LIBRARY_H)

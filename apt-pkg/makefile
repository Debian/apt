# -*- make -*-
BASE=..
SUBDIR=apt-pkg

# Header location
SUBDIRS = deb contrib
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name, don't forget to update init.h
LIBRARY=apt-pkg
LIBEXT=$(GLIBC_VER)$(LIBSTDCPP_VER)
MAJOR=3.1
MINOR=2
SLIBS=$(PTHREADLIB)

# Source code for the contributed non-core things
SOURCE = contrib/mmap.cc contrib/error.cc contrib/strutl.cc \
         contrib/configuration.cc contrib/progress.cc contrib/cmndline.cc \
	 contrib/md5.cc contrib/cdromutl.cc contrib/crc-16.cc \
	 contrib/fileutl.cc contrib/sha1.cc
HEADERS = mmap.h error.h configuration.h fileutl.h  cmndline.h \
	  md5.h crc-16.h cdromutl.h strutl.h sptr.h sha1.h

# Source code for the core main library
SOURCE+= pkgcache.cc version.cc depcache.cc \
         orderlist.cc tagfile.cc sourcelist.cc packagemanager.cc \
	 pkgrecords.cc algorithms.cc acquire.cc\
	 acquire-worker.cc acquire-method.cc init.cc clean.cc \
	 srcrecords.cc cachefile.cc versionmatch.cc policy.cc \
	 pkgsystem.cc indexfile.cc pkgcachegen.cc acquire-item.cc
HEADERS+= algorithms.h depcache.h pkgcachegen.h cacheiterators.h \
	  orderlist.h sourcelist.h packagemanager.h tagfile.h \
	  init.h pkgcache.h version.h progress.h pkgrecords.h \
	  acquire.h acquire-worker.h acquire-item.h acquire-method.h \
	  clean.h srcrecords.h cachefile.h versionmatch.h policy.h \
	  pkgsystem.h indexfile.h

# Source code for the debian specific components
# In theory the deb headers do not need to be exported..
SOURCE+= deb/deblistparser.cc deb/debrecords.cc deb/dpkgpm.cc \
         deb/debsrcrecords.cc deb/debversion.cc deb/debsystem.cc \
	 deb/debindexfile.cc deb/debindexfile.cc
HEADERS+= debversion.h debsrcrecords.h dpkgpm.h debrecords.h \
	  deblistparser.h debsystem.h debindexfile.h

HEADERS := $(addprefix apt-pkg/,$(HEADERS))

# Private header files
HEADERS+= system.h

include $(LIBRARY_H)

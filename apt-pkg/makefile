# -*- make -*-
BASE=..
SUBDIR=apt-pkg

# Header location
SUBDIRS = deb contrib
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name, don't forget to update init.h and the copy in 
# methods/makefile - FIXME
LIBRARY=apt-pkg
LIBEXT=$(GLIBC_VER)$(LIBSTDCPP_VER)
MAJOR=4.6
MINOR=0
SLIBS=$(PTHREADLIB) $(INTLLIBS) -lutil
APT_DOMAIN:=libapt-pkg$(MAJOR)

# Source code for the contributed non-core things
SOURCE = contrib/mmap.cc contrib/error.cc contrib/strutl.cc \
         contrib/configuration.cc contrib/progress.cc contrib/cmndline.cc \
	 contrib/md5.cc contrib/sha1.cc contrib/sha256.cc contrib/hashes.cc \
	 contrib/cdromutl.cc contrib/crc-16.cc \
	 contrib/fileutl.cc 
HEADERS = mmap.h error.h configuration.h fileutl.h  cmndline.h \
	  md5.h crc-16.h cdromutl.h strutl.h sptr.h sha1.h sha256.h hashes.h

# Source code for the core main library
SOURCE+= pkgcache.cc version.cc depcache.cc \
         orderlist.cc tagfile.cc sourcelist.cc packagemanager.cc \
	 pkgrecords.cc algorithms.cc acquire.cc\
	 acquire-worker.cc acquire-method.cc init.cc clean.cc \
	 srcrecords.cc cachefile.cc versionmatch.cc policy.cc \
	 pkgsystem.cc indexfile.cc pkgcachegen.cc acquire-item.cc \
	 indexrecords.cc vendor.cc vendorlist.cc cdrom.cc indexcopy.cc
HEADERS+= algorithms.h depcache.h pkgcachegen.h cacheiterators.h \
	  orderlist.h sourcelist.h packagemanager.h tagfile.h \
	  init.h pkgcache.h version.h progress.h pkgrecords.h \
	  acquire.h acquire-worker.h acquire-item.h acquire-method.h \
	  clean.h srcrecords.h cachefile.h versionmatch.h policy.h \
	  pkgsystem.h indexfile.h metaindex.h indexrecords.h vendor.h \
          vendorlist.h cdrom.h indexcopy.h

# Source code for the debian specific components
# In theory the deb headers do not need to be exported..
SOURCE+= deb/deblistparser.cc deb/debrecords.cc deb/dpkgpm.cc \
         deb/debsrcrecords.cc deb/debversion.cc deb/debsystem.cc \
	 deb/debindexfile.cc deb/debindexfile.cc deb/debmetaindex.cc
HEADERS+= debversion.h debsrcrecords.h dpkgpm.h debrecords.h \
	  deblistparser.h debsystem.h debindexfile.h debmetaindex.h

HEADERS := $(addprefix apt-pkg/,$(HEADERS))

# Private header files
HEADERS+= system.h

include $(LIBRARY_H)

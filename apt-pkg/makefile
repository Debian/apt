# -*- make -*-
BASE=..
SUBDIR=apt-pkg

# Header location
SUBDIRS = deb contrib
HEADER_TARGETDIRS = apt-pkg

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name and version (indirectly used from init.h)
include ../buildlib/libversion.mak
LIBRARY=apt-pkg
MAJOR=$(LIBAPTPKG_MAJOR)
MINOR=$(LIBAPTPKG_RELEASE)
SLIBS=$(PTHREADLIB) $(INTLLIBS) -lutil -ldl -lz
APT_DOMAIN:=libapt-pkg$(LIBAPTPKG_MAJOR)

# Source code for the contributed non-core things
SOURCE = contrib/mmap.cc contrib/error.cc contrib/strutl.cc \
         contrib/configuration.cc contrib/progress.cc contrib/cmndline.cc \
	 contrib/md5.cc contrib/sha1.cc contrib/sha256.cc contrib/hashes.cc \
	 contrib/cdromutl.cc contrib/crc-16.cc contrib/netrc.cc \
	 contrib/fileutl.cc 
HEADERS = mmap.h error.h configuration.h fileutl.h  cmndline.h netrc.h\
	  md5.h crc-16.h cdromutl.h strutl.h sptr.h sha1.h sha256.h hashes.h \
	  macros.h weakptr.h

# Source code for the core main library
SOURCE+= pkgcache.cc version.cc depcache.cc \
         orderlist.cc tagfile.cc sourcelist.cc packagemanager.cc \
	 pkgrecords.cc algorithms.cc acquire.cc\
	 acquire-worker.cc acquire-method.cc init.cc clean.cc \
	 srcrecords.cc cachefile.cc versionmatch.cc policy.cc \
	 pkgsystem.cc indexfile.cc pkgcachegen.cc acquire-item.cc \
	 indexrecords.cc vendor.cc vendorlist.cc cdrom.cc indexcopy.cc \
	 aptconfiguration.cc cachefilter.cc cacheset.cc
HEADERS+= algorithms.h depcache.h pkgcachegen.h cacheiterators.h \
	  orderlist.h sourcelist.h packagemanager.h tagfile.h \
	  init.h pkgcache.h version.h progress.h pkgrecords.h \
	  acquire.h acquire-worker.h acquire-item.h acquire-method.h \
	  clean.h srcrecords.h cachefile.h versionmatch.h policy.h \
	  pkgsystem.h indexfile.h metaindex.h indexrecords.h vendor.h \
	  vendorlist.h cdrom.h indexcopy.h aptconfiguration.h \
	  cachefilter.h cacheset.h

# Source code for the debian specific components
# In theory the deb headers do not need to be exported..
SOURCE+= deb/deblistparser.cc deb/debrecords.cc deb/dpkgpm.cc \
         deb/debsrcrecords.cc deb/debversion.cc deb/debsystem.cc \
	 deb/debindexfile.cc deb/debindexfile.cc deb/debmetaindex.cc
HEADERS+= debversion.h debsrcrecords.h dpkgpm.h debrecords.h \
	  deblistparser.h debsystem.h debindexfile.h debmetaindex.h

HEADERS := $(addprefix apt-pkg/,$(HEADERS))

include $(LIBRARY_H)

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
MAJOR=2.5
MINOR=0
SLIBS=$(PTHREADLIB)

# Source code for the contributed non-core things
SOURCE = contrib/mmap.cc contrib/error.cc contrib/strutl.cc \
         contrib/configuration.cc contrib/progress.cc contrib/cmndline.cc \
	 contrib/md5.cc contrib/cdromutl.cc contrib/crc-16.cc

# Source code for the main library
SOURCE+= pkgcache.cc version.cc fileutl.cc pkgcachegen.cc depcache.cc \
         orderlist.cc tagfile.cc sourcelist.cc packagemanager.cc \
	 pkgrecords.cc algorithms.cc acquire.cc acquire-item.cc \
	 acquire-worker.cc acquire-method.cc init.cc clean.cc \
	 srcrecords.cc cachefile.cc

# Source code for the debian specific components
SOURCE+= deb/deblistparser.cc deb/debrecords.cc deb/dpkgpm.cc deb/dpkginit.cc \
         deb/debsrcrecords.cc

# Public apt-pkg header files
HEADERS = algorithms.h depcache.h mmap.h pkgcachegen.h cacheiterators.h \
          error.h orderlist.h sourcelist.h configuration.h fileutl.h \
          packagemanager.h tagfile.h deblistparser.h init.h pkgcache.h \
          version.h progress.h pkgrecords.h debrecords.h cmndline.h \
	  acquire.h acquire-worker.h acquire-item.h acquire-method.h md5.h \
	  dpkgpm.h dpkginit.h cdromutl.h strutl.h clean.h srcrecords.h \
	  debsrcrecords.h cachefile.h crc-16.h

HEADERS := $(addprefix apt-pkg/,$(HEADERS))

# Private header files
HEADERS+= system.h

include $(LIBRARY_H)

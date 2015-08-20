# -*- make -*-
BASE=..
SUBDIR=cmdline

# Bring in the default rules
include ../buildlib/defaults.mak

# The apt program
PROGRAM=apt
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt.cc
include $(PROGRAM_H)

# The apt-cache program
PROGRAM=apt-cache
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-cache.cc
include $(PROGRAM_H)

# The apt-get program
PROGRAM=apt-get
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-get.cc 
include $(PROGRAM_H)

# The apt-config program
PROGRAM=apt-config
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-config.cc
include $(PROGRAM_H)

# The apt-cdrom program
PROGRAM=apt-cdrom
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-cdrom.cc 
include $(PROGRAM_H)

# The apt-mark program
PROGRAM=apt-mark
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-mark.cc
include $(PROGRAM_H)

# The apt-helper
PROGRAM=apt-helper
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS) -lresolv
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-helper.cc
include $(PROGRAM_H)

# The apt-report-mirror-failure program
#SOURCE=apt-report-mirror-failure
#TO=$(BIN)
#TARGET=program
#include $(COPY_H)

#
# the following programs are shipped in apt-utils
#
APT_DOMAIN:=apt-utils

# The apt-sortpkgs program
PROGRAM=apt-sortpkgs
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-sortpkgs.cc
include $(PROGRAM_H)

# The apt-extracttemplates program
PROGRAM=apt-extracttemplates
SLIBS = -lapt-pkg -lapt-inst -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-inst/makefile apt-private/makefile
SOURCE = apt-extracttemplates.cc 
include $(PROGRAM_H)

# The internal solver acting as an external
PROGRAM=apt-internal-solver
SLIBS = -lapt-pkg -lapt-private $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile apt-private/makefile
SOURCE = apt-internal-solver.cc
include $(PROGRAM_H)

# This just dumps out the state
PROGRAM=apt-dump-solver
SLIBS = -lapt-pkg $(INTLLIBS)
LIB_MAKES = apt-pkg/makefile
SOURCE = apt-dump-solver.cc
include $(PROGRAM_H)

# The apt-key program
apt-key: apt-key.in
	sed -e "s#&keyring-filename;#$(shell ../vendor/getinfo keyring-filename)#" \
		-e "s#&keyring-removed-filename;#$(shell ../vendor/getinfo keyring-removed-filename)#" \
		-e "s#&keyring-master-filename;#$(shell ../vendor/getinfo keyring-master-filename)#" \
		-e "s#&keyring-uri;#$(shell ../vendor/getinfo keyring-uri)#" \
		-e "s#&keyring-package;#$(shell ../vendor/getinfo keyring-package)#" $< > $@
	chmod 755 $@

SOURCE=apt-key
TO=$(BIN)
TARGET=program
include $(COPY_H)

clean: clean/apt-key

clean/apt-key:
	rm -f apt-key

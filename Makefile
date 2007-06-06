# -*- make -*-

# This is the top level make file for APT, it recurses to each lower
# level make file and runs it with the proper target
ifndef NOISY
.SILENT:
endif

.PHONY: default
default: startup all

.PHONY: headers library clean veryclean all binary program doc
all headers library clean veryclean binary program doc dirs:
	$(MAKE) -C apt-pkg $@
	$(MAKE) -C apt-inst $@
	$(MAKE) -C methods $@
	$(MAKE) -C cmdline $@
	$(MAKE) -C ftparchive $@
	$(MAKE) -C dselect $@
	$(MAKE) -C doc $@
	$(MAKE) -C po $@

# Some very common aliases
.PHONY: maintainer-clean dist-clean distclean pristine sanity 
maintainer-clean dist-clean distclean pristine sanity: veryclean

# The startup target builds the necessary configure scripts. It should
# be used after a CVS checkout.
CONVERTED=environment.mak include/config.h include/apti18n.h build/doc/Doxyfile makefile
include buildlib/configure.mak
$(BUILDDIR)/include/config.h: buildlib/config.h.in
$(BUILDDIR)/include/apti18n.h: buildlib/apti18n.h.in
$(BUILDDIR)/environment.mak: buildlib/environment.mak.in
$(BUILDDIR)/makefile: buildlib/makefile.in

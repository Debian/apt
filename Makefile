# -*- make -*-

# This is the top level make file for APT, it recurses to each lower
# level make file and runs it with the proper target
ifndef NOISY
.SILENT:
endif

.PHONY: default
default: startup all

.PHONY: fast headers library clean veryclean all binary program doc test update-po
all headers library clean veryclean binary program doc manpages docbook test update-po startup dirs:
	$(MAKE) -C vendor $@
	$(MAKE) -C apt-pkg $@
	$(MAKE) -C apt-inst $@
	$(MAKE) -C apt-private $@
	$(MAKE) -C methods $@
	$(MAKE) -C cmdline $@
	$(MAKE) -C ftparchive $@
	$(MAKE) -C dselect $@
	$(MAKE) -C doc $@
	$(MAKE) -C po $@
	# FIXME: -C test has issue swith parallel builds, investigate!
	-$(MAKE) -C test $@

fast:
	$(MAKE) -C vendor all
	$(MAKE) -C apt-pkg all
	$(MAKE) -C apt-inst all
	$(MAKE) -C apt-private all
	$(MAKE) -C methods all
	$(MAKE) -C cmdline all
	$(MAKE) -C ftparchive all
	$(MAKE) -C test all

all headers library clean veryclean binary program doc manpages docbook test update-po: startup dirs

dirs: startup

# Some very common aliases
.PHONY: maintainer-clean dist-clean distclean pristine sanity
maintainer-clean dist-clean distclean pristine sanity: veryclean
veryclean: clean

# The startup target builds the necessary configure scripts. It should
# be used after a CVS checkout.
CONVERTED=environment.mak include/config.h include/apti18n.h build/doc/Doxyfile makefile
include buildlib/configure.mak
$(BUILDDIR)/include/config.h: buildlib/config.h.in
$(BUILDDIR)/include/apti18n.h: buildlib/apti18n.h.in
$(BUILDDIR)/environment.mak: buildlib/environment.mak.in
$(BUILDDIR)/makefile: buildlib/makefile.in

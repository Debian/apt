# -*- make -*-

# This is the top level make file for APT, it recurses to each lower
# level make file and runs it with the proper target
ifndef NOISY
.SILENT:
endif

.PHONY: headers library clean veryclean all binary program doc
all headers library clean veryclean binary program doc:
	$(MAKE) -C apt-pkg $@
	$(MAKE) -C methods $@
#	$(MAKE) -C methods/ftp $@
	$(MAKE) -C cmdline $@
	$(MAKE) -C deity $@
	$(MAKE) -C gui $@
	$(MAKE) -C doc $@

# Some very common aliases
.PHONY: maintainer-clean dist-clean distclean pristine sanity 
maintainer-clean dist-clean distclean pristine sanity: veryclean

# The startup target builds the necessary configure scripts. It should
# be used after a CVS checkout.
CONVERTED=environment.mak include/config.h makefile
$(BUILD)/include/config.h: buildlib/config.h.in
$(BUILD)/environment.mak: buildlib/environment.mak.in
include buildlib/configure.mak

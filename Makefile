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

.PHONY: maintainer-clean dist-clean distclean pristine sanity 
maintainer-clean dist-clean distclean pristine sanity: veryclean


# The startup target builds the necessary configure scripts. It should
# be used after a CVS checkout.
.PHONY: startup
BUILD=build
CONVERTED=$(BUILD)/environment.mak $(BUILD)/include/config.h $(BUILD)/makefile
startup: configure $(CONVERTED)

configure: aclocal.m4 configure.in
	autoconf	

aclocal.m4:
	aclocal -I buildlib

$(BUILD)/config.status: configure
	test -e $(BUILD) || mkdir $(BUILD)	
	(HERE=`pwd`; cd $(BUILD) && $$HERE/configure)
$(BUILD)/include/config.h: buildlib/config.h.in
$(BUILD)/environment.mak: buildlib/environment.mak.in
$(CONVERTED): $(BUILD)/config.status
	(cd $(BUILD) && ./config.status)

# -*- make -*-

# This is the top level test makefile for APT, it recurses to each lower
# level make file and runs it with the proper target
ifndef NOISY
.SILENT:
endif

.PHONY: startup headers library clean veryclean all binary program doc test update-po
startup all clean veryclean binary program dirs test update-po manpages debiandoc:
	$(MAKE) -C libapt $@
	$(MAKE) -C interactive-helper $@

# Some very common aliases
.PHONY: maintainer-clean dist-clean distclean pristine sanity
maintainer-clean dist-clean distclean pristine sanity: veryclean

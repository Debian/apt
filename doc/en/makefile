# -*- make -*-
BASE=../..
SUBDIR=doc/en

# Bring in the default rules
include ../../buildlib/defaults.mak

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC
# generate a list of accepted man page translations
SOURCE = $(patsubst ../%.xml,%,$(wildcard ../*.?.xml))
INCLUDES = ../apt.ent ../apt-verbatim.ent apt-vendor.ent
STYLESHEET=../manpage-style.xsl

LOCAL := manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

apt-vendor.ent: ../../vendor/current/apt-vendor.ent
	ln -sf $(shell readlink -f $^) $@

# Install generation hooks
manpages: $($(LOCAL)-LIST) apt.7

$($(LOCAL)-LIST) :: % : ../%.xml $(STYLESHEET) $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

apt.7: ../apt.7
	cp -a ../apt.7 apt.7

# Clean rule
.PHONY: clean/$(LOCAL)
veryclean: clean/$(LOCAL)
clean: clean/$(LOCAL)
clean/$(LOCAL):
	-rm -rf $($(@F)-LIST) apt.7 apt-vendor.ent
endif

# Chain to the manpage rule
SOURCE = apt.7
include $(MANPAGE_H)

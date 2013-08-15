# -*- make -*-
BASE=../..
SUBDIR=doc/en

# Bring in the default rules
include ../../buildlib/defaults.mak

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC
# generate a list of accepted man page translations
SOURCE = $(patsubst ../%.xml,%,$(wildcard ../*.?.xml))
INCLUDES = ../apt.ent ../apt-verbatim.ent
STYLESHEET=../manpage-style.xsl

LOCAL := manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
manpages: $($(LOCAL)-LIST) apt.8

$($(LOCAL)-LIST) :: % : ../%.xml $(STYLESHEET) $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

apt.8: ../apt.8
	cp -a ../apt.8 apt.8

# Clean rule
.PHONY: clean/$(LOCAL)
veryclean: clean/$(LOCAL)
clean: clean/$(LOCAL)
clean/$(LOCAL):
	-rm -rf $($(@F)-LIST) apt.8
endif

# Chain to the manpage rule
SOURCE = apt.8
include $(MANPAGE_H)

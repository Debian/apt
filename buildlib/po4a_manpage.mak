# -*- make -*-

# This handles man pages with po4a. We convert to the respective
# output in the source directory then copy over to the final dest. This
# means po4a is only needed if compiling from bzr

# Input
# $(LC)     - The language code of the translation

# See defaults.mak for information about LOCAL

# generate a list of accepted man page translations
SOURCE = $(patsubst %.xml,%,$(wildcard *.$(LC).?.xml))
INCLUDES = apt.ent apt-verbatim.ent apt-vendor.ent

manpages:

%.xsl: ../%.xsl
	cp -a $< .

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC

STYLESHEET=manpage-style.xsl

LOCAL := po4a-manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
manpages: $($(LOCAL)-LIST)
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

apt-verbatim.ent: ../apt-verbatim.ent
	cp -a ../apt-verbatim.ent .

apt-vendor.ent: ../apt-vendor.ent
	cp -a ../apt-vendor.ent .

$($(LOCAL)-LIST) :: % : %.xml $(STYLESHEET) $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) \
		--stringparam l10n.gentext.default.language $(LC) \
		-o $@ $(STYLESHEET) $< || exit 200 # why xsltproc doesn't respect the -o flag here???
	test -f $(subst .$(LC),,$@) || echo 'FIXME: xsltproc respects the -o flag now, workaround can be removed'
	mv -f $(subst .$(LC),,$@) $@

# Clean rule
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL)
clean/$(LOCAL):
	rm -f $($(@F)-LIST) apt.ent apt-verbatim.ent
veryclean/$(LOCAL):
	# we are nuking the directory we are working in as it is auto-generated
	rm -rf '$(abspath .)'

HAVE_PO4A=yes
endif

# take care of the rest
INCLUDES :=

ifndef HAVE_PO4A
# Strip from the source list any man pages we don't have compiled already
SOURCE := $(wildcard $(SOURCE))
endif

# Chain to the manpage rule
ifneq ($(words $(SOURCE)),0)
include $(MANPAGE_H)
endif

# DocBook XML Documents
SOURCE := $(wildcard *.$(LC).dbk)
include $(DOCBOOK_H)

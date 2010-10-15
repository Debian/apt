# -*- make -*-

# This handles man pages with po4a. We convert to the respective
# output in the source directory then copy over to the final dest. This
# means po4a is only needed if compiling from bzr

# Input
# $(LC)     - The language code of the translation

# See defaults.mak for information about LOCAL

# generate a list of accepted man page translations
SOURCE = $(patsubst %.xml,%,$(wildcard *.$(LC).?.xml))
INCLUDES = apt.ent apt-verbatim.ent

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC

STYLESHEET=../manpage-style.xsl

LOCAL := po4a-manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
doc: $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

apt-verbatim.ent: ../apt-verbatim.ent
	cp ../apt-verbatim.ent .

$($(LOCAL)-LIST) :: % : %.xml $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $< || exit 200 # why xsltproc doesn't respect the -o flag here???
	test -f $(subst .$(LC),,$@) || echo FIXME: xsltproc respect the -o flag now, workaround can be removed
	mv -f $(subst .$(LC),,$@) $@

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST) apt.ent apt-verbatim.ent apt.$(LC).8 \
		$(addsuffix .xml,$($(@F)-LIST)) \
		offline.$(LC).sgml guide.$(LC).sgml

HAVE_PO4A=yes
endif

# take care of the rest
SOURCE := $(SOURCE) $(wildcard apt.$(LC).8)
INCLUDES :=

ifndef HAVE_PO4A
# Strip from the source list any man pages we dont have compiled already
SOURCE := $(wildcard $(SOURCE))
endif

# Chain to the manpage rule
ifneq ($(words $(SOURCE)),0)
include $(MANPAGE_H)
endif

# Debian Doc SGML Documents
SOURCE := $(wildcard *.$(LC).sgml)
DEBIANDOC_HTML_OPTIONS=-l $(LC).UTF-8
include $(DEBIANDOC_H)

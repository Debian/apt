# -*- make -*-

# This handles man pages in DocBook SGMLL format. We convert to the respective
# output in the source directory then copy over to the final dest. This
# means yodl is only needed if compiling from CVS

# Input
# $(SOURCE) - The documents to use, in the form foo.sect, ie apt-cache.8
#             the sgml files are called apt-cache.8.sgml

# See defaults.mak for information about LOCAL

# Some local definitions
ifdef DOCBOOK2MAN

LOCAL := sgml-manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
doc: $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

$($(LOCAL)-LIST) :: % : %.sgml $(INCLUDES)
	echo Creating man page $@
	$(DOCBOOK2MAN) $<

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

HAVE_SGML=yes
endif

INCLUDES :=

ifndef HAVE_SGML
# Strip from the source list any man pages we dont have compiled already
SOURCE := $(wildcard $(SOURCE))
endif

# Chain to the manpage rule
ifneq ($(words $(SOURCE)),0)
include $(MANPAGE_H)
endif

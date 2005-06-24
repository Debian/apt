# -*- make -*-

# This handles man pages in DocBook XML format. We convert to the respective
# output in the source directory then copy over to the final dest. This
# means xmlto is only needed if compiling from Arch

# Input
# $(SOURCE) - The documents to use, in the form foo.sect, ie apt-cache.8
#             the XML files are called apt-cache.8.xml

# See defaults.mak for information about LOCAL

# Some local definitions
ifdef XMLTO

LOCAL := xml-manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
doc: $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

$($(LOCAL)-LIST) :: % : %.xml $(INCLUDES)
	echo Creating man page $@
	$(XMLTO) man $<

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

HAVE_XMLTO=yes
endif

INCLUDES :=

ifndef HAVE_XMLTO
# Strip from the source list any man pages we dont have compiled already
SOURCE := $(wildcard $(SOURCE))
endif

# Chain to the manpage rule
ifneq ($(words $(SOURCE)),0)
include $(MANPAGE_H)
endif

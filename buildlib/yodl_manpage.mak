# -*- make -*-

# This handles man pages in YODL format. We convert to the respective
# output in the source directory then copy over to the final dest. This
# means yodl is only needed if compiling from CVS

# Input
# $(SOURCE) - The documents to use, in the form foo.sect, ie apt-cache.8
#             the yodl files are called apt-cache.8.yo

# See defaults.mak for information about LOCAL

# Some local definitions
ifdef YODL_MAN

LOCAL := yodl-manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
doc: $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

$($(LOCAL)-LIST) :: % : %.yo
	echo Creating man page $@
	yodl2man -o $@ $<

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

else

# Strip from the source list any man pages we dont have compiled already
SOURCE := $(wildcard $(SOURCE))

endif

# Chain to the manpage rule
ifneq ($(words $(SOURCE)),0)
include $(MANPAGE_H)
endif

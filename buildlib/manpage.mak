# -*- make -*-

# This installs man pages into the doc directory

# Input
# $(SOURCE) - The documents to use

# All output is writtin to files in the build doc directory

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(addprefix $(DOC)/,$(SOURCE))

# Install generation hooks
doc: $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

MKDIRS += $(DOC)

$($(LOCAL)-LIST) : $(DOC)/% : %
	echo Installing man page $< to $(@D)
	cp $< $(@D)

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

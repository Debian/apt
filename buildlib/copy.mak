# -*- make -*-

# This installs arbitrary files into a directory

# Input
# $(SOURCE) - The documents to use
# $(TO)     - The directory to put them in
# $(TARGET) - The global target to add the local target as a dependency
#             to.
# All output is writtin to files in the build/$(TO) directory

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := copy-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(addprefix $(TO)/,$(SOURCE))

# Install generation hooks
$(TARGET): $($(LOCAL)-LIST)
veryclean: veryclean/$(LOCAL)

MKDIRS += $(dir $($(LOCAL)-LIST))

$($(LOCAL)-LIST) : $(TO)/% : %
	echo Installing $< to $(@D)
	cp $< $(@D)

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

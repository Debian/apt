# -*- make -*-

# This prints a failure message but does not abort the make

# Input
# $(MESSAGE) - The message to show
# $(PROGRAM) - The program/libary/whatever.

# See defaults.mak for information about LOCAL

LOCAL := $(PROGRAM)
$(LOCAL)-MSG := $(MESSAGE)

# Install hooks
program: $(PROGRAM)

.PHONY: $(PROGRAM)
$(PROGRAM) :
	echo $($@-MSG)
	

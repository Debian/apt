# -*- make -*-

# This creates a python shared module.

# Input
# $(SOURCE) - The source code to use
# $(MODULE) - The name of the module without module or .so 

# All output is writtin to .opic files in the build directory to
# signify the PIC output.

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := $(MODULE)module.so
$(LOCAL)-OBJS := $(addprefix $(OBJ)/,$(addsuffix .opic,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-DEP := $(addprefix $(DEP)/,$(addsuffix .opic.d,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-SLIBS := $(SLIBS)
$(LOCAL)-MODULE := $(MODULE)

# Install the command hooks
library: $(LIB)/$(MODULE)module.so
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

# Make Directories
MKDIRS += $(OBJ) $(DEP) $(LIB) 

# The clean rules
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL)
clean/$(LOCAL):
	-rm -f $($(@F)-OBJS) $($(@F)-DEP)
veryclean/$(LOCAL): clean/$(LOCAL)
	-rm -f $($(@F)-HEADERS) $(LIB)/$($(@F)-MODULE)module.so*

# The binary build rule.
ifdef PYTHONLIB
ifndef ONLYSTATICLIBS
$(LIB)/$(MODULE)module.so: $($(LOCAL)-OBJS)
	-rm -f $(LIB)/$($(@F)-MODULE)module.so* 2> /dev/null
	echo Building shared Python module $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PICFLAGS) $(LFLAGS) $(LFLAGS_SO)\
	   -o $@ -shared \
	   $(filter %.opic,$^) \
	   $($(@F)-SLIBS) $(PYTHONLIB)
else
.PHONY: $(LIB)/$(MODULE)module.so
$(LIB)/$(MODULE)module.so: 
	echo Don't know how to make a python module here, not building $@
endif # ifndef ONLYSTATICLIBS
else
.PHONY: $(LIB)/$(MODULE)module.so
$(LIB)/$(MODULE)module.so: 
	echo No python support, not building $@
endif  # ifdef PYTHONLIB

# Compilation rules
vpath %.cc $(SUBDIRS)
$(OBJ)/%.opic: %.cc
	echo Compiling $< to $@
	$(CXX) -c $(INLINEDEPFLAG) $(CPPFLAGS) $(CXXFLAGS) $(PICFLAGS) -o $@ $<
	$(DoDep)

# Include the dependencies that are available
The_DFiles = $(wildcard $($(LOCAL)-DEP))
ifneq ($(words $(The_DFiles)),0)
include $(The_DFiles)
endif 

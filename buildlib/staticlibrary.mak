# -*- make -*-

# This creates a static library.

# Input
# $(SOURCE) - The source code to use
# $(HEADERS) - Exported header files and private header files
# $(LIBRARY) - The name of the library without lib or .so 

# All output is writtin to .o files in the build directory

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := lib$(LIBRARY).a
$(LOCAL)-OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-DEP := $(addprefix $(DEP)/,$(addsuffix .o.d,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-HEADERS := $(addprefix $(INCLUDE)/,$(HEADERS))
$(LOCAL)-LIB := $(LIB)/lib$(LIBRARY).a

# Install the command hooks
headers: $($(LOCAL)-HEADERS)
library: $($(LOCAL)-LIB)
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

# Make Directories
MKDIRS += $(OBJ) $(DEP) $(LIB) $(dir $($(LOCAL)-HEADERS))

# The clean rules
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL)
clean/$(LOCAL):
	-rm -f $($(@F)-OBJS) $($(@F)-DEP)
veryclean/$(LOCAL): clean/$(LOCAL)
	-rm -f $($(@F)-HEADERS) $($(@F)-LIB)

# Build rules for the two symlinks
.PHONY: $($(LOCAL)-LIB)
	
# The binary build rule
$($(LOCAL)-LIB): $($(LOCAL)-HEADERS) $($(LOCAL)-OBJS)
	echo Building library $@
	-rm $@ > /dev/null 2>&1
	$(AR) cq $@ $(filter %.o,$^)
ifneq ($(words $(RANLIB)),0)
	$(RANLIB) $@
endif

# Compilation rules
vpath %.cc $(SUBDIRS)
$(OBJ)/%.o: %.cc
	echo Compiling $< to $@
	$(CXX) -c $(INLINEDEPFLAG) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<
	$(DoDep)

# Include the dependencies that are available
The_DFiles = $(wildcard $($(LOCAL)-DEP))
ifneq ($(words $(The_DFiles)),0)
include $(The_DFiles)
endif 

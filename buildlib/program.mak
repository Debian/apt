# -*- make -*-

# This creates a program

# Input
# $(SOURCE) - The source code to use
# $(PROGRAM) - The name of the program
# $(SLIBS) - Shared libs to link against

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := $(PROGRAM)
$(LOCAL)-OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-DEP := $(addprefix $(DEP)/,$(addsuffix .d,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-BIN := $(BIN)/$(PROGRAM)
$(LOCAL)-SLIBS := $(SLIBS)

# Install the command hooks
program: $(BIN)/$(PROGRAM)
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

# The clean rules
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL)
clean/$(LOCAL):
	-rm -f $($(@F)-OBJS) $($(@F)-DEP)
veryclean/$(LOCAL): clean/$(LOCAL)
	-rm -f $($(@F)-BIN)

# The binary build rule
$($(LOCAL)-BIN): $($(LOCAL)-OBJS)
	echo Building program $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LFLAGS) -o $@ $(filter %.o,$^) $($(LOCAL)-SLIBS)

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

# -*- make -*-

# This creates a program

# Input
# $(SOURCE) - The source code to use
# $(PROGRAM) - The name of the program
# $(SLIBS) - Shared libs to link against
# $(LIB_MAKES) - Shared libary make files to depend on - to ensure we get
# remade when the shared library version increases.

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := $(PROGRAM)
$(LOCAL)-OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-DEP := $(addprefix $(DEP)/,$(addsuffix .o.d,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-BIN := $(BIN)/$(PROGRAM)
$(LOCAL)-SLIBS := $(SLIBS)
$(LOCAL)-MKS := $(addprefix $(BASE)/,$(LIB_MAKES))

# Install the command hooks
program: $(BIN)/$(PROGRAM)
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

TYPE = src
include $(PODOMAIN_H)

# Make Directories
MKDIRS += $(OBJ) $(DEP) $(BIN)

# The clean rules
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL) 
clean/$(LOCAL):
	-rm -f $($(@F)-OBJS) $($(@F)-DEP)
veryclean/$(LOCAL): clean/$(LOCAL)
	-rm -f $($(@F)-BIN)

# The convience binary build rule
.PHONY: $(PROGRAM)
$(PROGRAM): $($(LOCAL)-BIN)

# The binary build rule
$($(LOCAL)-BIN): $($(LOCAL)-OBJS) $($(LOCAL)-MKS)
	echo Building program $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LFLAGS) -o $@ $(filter %.o,$^) $($(@F)-SLIBS) $(LEFLAGS)

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

# -*- make -*-

# This creates a shared library.

# Input
# $(SOURCE) - The source code to use
# $(HEADERS) - Exported header files and private header files
# $(LIBRARY) - The name of the library without lib or .so 
# $(MAJOR) - The major version number of this library
# $(MINOR) - The minor version number of this library
# $(APT_DOMAIN) - The text domain for this library

# All output is writtin to .opic files in the build directory to
# signify the PIC output.

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := lib$(LIBRARY)$(LIBEXT).so.$(MAJOR).$(MINOR)
$(LOCAL)-OBJS := $(addprefix $(OBJ)/,$(addsuffix .opic,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-DEP := $(addprefix $(DEP)/,$(addsuffix .opic.d,$(notdir $(basename $(SOURCE)))))
$(LOCAL)-HEADERS := $(addprefix $(INCLUDE)/,$(HEADERS))
$(LOCAL)-SONAME := lib$(LIBRARY)$(LIBEXT).so.$(MAJOR)
$(LOCAL)-SLIBS := $(SLIBS)
$(LOCAL)-LIBRARY := $(LIBRARY)

TYPE = src
include $(PODOMAIN_H)

# Install the command hooks
headers: $($(LOCAL)-HEADERS)
library: $(LIB)/lib$(LIBRARY).so $(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR)
clean: clean/$(LOCAL)
veryclean: veryclean/$(LOCAL)

# Make Directories
MKDIRS += $(OBJ) $(DEP) $(LIB) $(dir $($(LOCAL)-HEADERS))

# The clean rules
.PHONY: clean/$(LOCAL) veryclean/$(LOCAL)
clean/$(LOCAL):
	-rm -f $($(@F)-OBJS) $($(@F)-DEP)
veryclean/$(LOCAL): clean/$(LOCAL)
	-rm -f $($(@F)-HEADERS) $(LIB)/lib$($(@F)-LIBRARY)*.so*

# Build rules for the two symlinks
.PHONY: $(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR) $(LIB)/lib$(LIBRARY).so
$(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR): $(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR).$(MINOR)
	ln -sf $(<F) $@
$(LIB)/lib$(LIBRARY).so: $(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR).$(MINOR)
	ln -sf $(<F) $@

# The binary build rule
$(LIB)/lib$(LIBRARY)$(LIBEXT).so.$(MAJOR).$(MINOR): $($(LOCAL)-HEADERS) $($(LOCAL)-OBJS)
	-rm -f $(LIB)/lib$($(@F)-LIBRARY)*.so* 2> /dev/null
	echo Building shared library $@
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PICFLAGS) $(LFLAGS) $(LFLAGS_SO)\
	   -o $@ $(SONAME_MAGIC)$($(@F)-SONAME) -shared \
	   $(filter %.opic,$^) \
	   $($(@F)-SLIBS) 

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

# -*- make -*-

# This file configures the default environment for the make system
# The way it works is fairly simple, each module is defined in it's
# own *.mak file. It expects a set of variables to be set to values
# for it to operate as expected. When included the module generates
# the requested rules based on the contents of its control variables.

# This works out very well and allows a good degree of flexability.
# To accomidate some of the features we introduce the concept of 
# local variables. To do this we use the 'Computed Names' feature of
# gmake. Each module declares a LOCAL scope and access it with,
#   $($(LOCAL)-VAR)
# This works very well but it is important to rembember that within
# a rule the LOCAL var is unavailble, it will have to be constructed
# from the information in the rule invokation. For stock rules like 
# clean this is simple, we use a local clean rule called clean/$(LOCAL)
# and then within the rule $(@F) gets back $(LOCAL)! Other rules will
# have to use some other mechanism (filter perhaps?) The reason such
# lengths are used is so that each directory can contain several 'instances'
# of any given module. I notice that the very latest gmake has the concept
# of local variables for rules. It is possible this feature in conjunction
# with the generated names will provide a very powerfull solution indeed!

# A build directory is used by default, all generated items get put into
# there. However unlike automake this is not done with a VPATH build
# (vpath builds break the distinction between #include "" and #include <>)
# but by explicly setting the BUILD variable. Make is invoked from
# within the source itself which is much more compatible with compilation
# environments.
ifndef NOISY
.SILENT:
endif

# Search for the build directory
ifdef BUILD
BUILD_POSSIBLE := $(BUILD) $(BASE)/$(BUILD)
else
BUILD_POSSIBLE := $(BASE) $(BASE)/build-$(shell uname -m) $(BASE)/build
endif

BUILDX:= $(foreach i,$(BUILD_POSSIBLE),$(wildcard $(i)/environment.mak*))
BUILDX:= $(patsubst %/,%,$(firstword $(dir $(BUILDX))))

ifeq ($(words $(BUILDX)),0)
error-all:
	echo Can't find the build directory in $(BUILD_POSSIBLE) -- use BUILD=
endif

override BUILD := $(BUILDX)

# Base definitions
INCLUDE := $(BUILD)/include
BIN := $(BUILD)/bin
LIB := $(BIN)
OBJ := $(BUILD)/obj/$(SUBDIR)
DEP := $(OBJ)
DOC := $(BUILD)/docs

# Module types
LIBRARY_H = $(BASE)/buildlib/library.mak
DEBIANDOC_H = $(BASE)/buildlib/debiandoc.mak
MANPAGE_H = $(BASE)/buildlib/manpage.mak
PROGRAM_H = $(BASE)/buildlib/program.mak
COPY_H = $(BASE)/buildlib/copy.mak
YODL_MANPAGE_H = $(BASE)/buildlib/yodl_manpage.mak

ifdef STATICLIBS
LIBRARY_H += $(BASE)/buildlib/staticlibrary.mak
endif

ifdef ONLYSTATICLIBS
LIBRARY_H = $(BASE)/buildlib/staticlibrary.mak
endif

# Source location control
# SUBDIRS specifies sub components of the module that
# may be located in subdrictories of the source dir. 
# This should be declared before including this file
SUBDIRS+=

# Header file control. 
# TARGETDIRS indicitates all of the locations that public headers 
# will be published to.
# This should be declared before including this file
HEADER_TARGETDIRS+=

# Options
include $(BUILD)/environment.mak
CPPFLAGS+= -I$(INCLUDE)
LDFLAGS+= -L$(LIB)

# Phony rules. Other things hook these by appending to the dependency
# list
.PHONY: headers library clean veryclean all binary program doc
.PHONY: maintainer-clean dist-clean distclean pristine sanity
all: binary doc
binary: library program
maintainer-clean dist-clean distclean pristine sanity: veryclean
headers library clean veryclean program:

veryclean:
	echo Very Clean done for $(SUBDIR)
clean:
	echo Clean done for $(SUBDIR)
	
# Header file control. We want all published interface headers to go
# into the build directory from thier source dirs. We setup some
# search paths here
vpath %.h $(SUBDIRS)
$(INCLUDE)/%.h $(addprefix $(INCLUDE)/,$(addsuffix /%.h,$(HEADER_TARGETDIRS))) : %.h
	cp $< $@

# Dependency generation. We want to generate a .d file using gnu cpp.
# For GNU systems the compiler can spit out a .d file while it is compiling,
# this is specified with the INLINEDEPFLAG. Other systems might have a 
# makedep program that can be called after compiling, that's illistrated
# by the DEPFLAG case.
# Compile rules are expected to call this macro after calling the compiler
ifdef INLINEDEPFLAG
 define DoDep
	sed -e "1s/.*:/$(subst /,\\/,$@):/" $(basename $(@F)).d > $(DEP)/$(@F).d
	-rm -f $(basename $(@F)).d
 endef
else
 ifdef DEPFLAG
  define DoDep
	$(CXX) $(DEPFLAG) $(CPPFLAGS) -o $@ $<
	sed -e "1s/.*:/$(subst /,\\/,$@):/" $(basename $(@F)).d > $(DEP)/$(@F).d
	-rm -f $(basename $(@F)).d
  endef
 else
  define DoDep
  endef
 endif
endif	

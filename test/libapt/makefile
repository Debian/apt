# -*- make -*-
BASE=../..
SUBDIR=test/libapt
BASENAME=_libapt_test
APT_DOMAIN=none

# Bring in the default rules
include ../../buildlib/defaults.mak

.PHONY: test
ifeq (file-okay,$(shell $(CC) -I $(BASE)/build/include -M gtest_runner.cc >/dev/null 2>&1 && echo 'file-okay'))
test: $(BIN)/gtest$(BASENAME)
	MALLOC_PERTURB_=21 MALLOC_CHECK_=2 LD_LIBRARY_PATH=$(LIB) $(BIN)/gtest$(BASENAME)

$(BIN)/gtest$(BASENAME): $(LIB)/gtest.a

PROGRAM = gtest${BASENAME}
SLIBS = -lapt-pkg -lapt-private -lapt-inst -pthread $(LIB)/gtest.a
LIB_MAKES = apt-pkg/makefile apt-private/makefile apt-inst/makefile
SOURCE = gtest_runner.cc $(wildcard *-helpers.cc *_test.cc)
include $(PROGRAM_H)


MKDIRS += $(OBJ) $(LIB)
LOCAL=gtest
SOURCE=gtest-all
gtest-OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(SOURCE)))

# The rest of the file is based on the example found in
# /usr/share/doc/libgtest-dev/examples/make/Makefile
GTEST_DIR = /usr/src/gtest

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
#CPPFLAGS += -isystem $(GTEST_DIR)/include

# Flags passed to the C++ compiler.
CXXFLAGS += -pthread
# disable some flags for gtest again
CXXFLAGS+= -Wno-missing-declarations
CXXFLAGS+= -Wno-missing-field-initializers
CXXFLAGS+= -Wno-suggest-attribute=pure -Wno-suggest-attribute=const -Wno-suggest-attribute=noreturn
CXXFLAGS+= -Wno-undef

# All Google Test headers.  Usually you shouldn't change this definition.
GTEST_HEADERS = /usr/include/gtest/*.h \
                /usr/include/gtest/internal/*.h

# House-keeping build targets.
.PHONY: clean/gtest veryclean/gtest
clean: clean/gtest
clean/gtest:
	rm -f $(gtest-OBJS)
veryclean: veryclean/gtest
veryclean/gtest: clean/gtest
	rm -f $(LIB)/gtest.a

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

# Builds gtest.a
# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  This is fine as Google Test
# compiles fast and for ordinary users its source rarely changes.
$(gtest-OBJS): $(GTEST_SRCS_)
	echo Compiling $@
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c -o $@ $(GTEST_DIR)/src/$(notdir $(basename $@)).cc

$(LIB)/gtest.a: $(OBJ)/gtest-all.o
	echo Building static library $@
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $^

else
test:
	@echo "APT uses Googles C++ testing framework for its unit tests"
	@echo "On Debian systems this is available in the 'libgtest-dev' package."
	@echo "Please install it before attempting to run the unit tests."
	$(CC) -I $(BASE)/build/include -M gtest_runner.cc
	exit 100
endif

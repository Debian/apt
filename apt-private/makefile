# -*- make -*-
BASE=..
SUBDIR=apt-private

# Header location
HEADER_TARGETDIRS = apt-private

# Bring in the default rules
include ../buildlib/defaults.mak

# The library name
LIBRARY=apt-private
MAJOR=0.0
MINOR=0
SLIBS=$(PTHREADLIB) -lapt-pkg
CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

SOURCE = $(sort $(wildcard *.cc))
HEADERS = $(addprefix apt-private/,$(sort $(wildcard *.h)))

include $(LIBRARY_H)

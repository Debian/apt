# -*- make -*-

# This make fragment is included by the toplevel make to handle configure
# and setup. It defines a target called startup that when run will init
# the build directory, generate configure from configure.in, create aclocal
# and has rules to run config.status should one of the .in files change.

# Input
#  BUILD - The build director
#  CONVERTED - List of files output by configure $(BUILD) is prepended
#              The caller must provide depends for these files
# It would be a fairly good idea to run this after a cvs checkout.
BUILD=build

.PHONY: startup
startup: configure $(addprefix $(BUILD)/,$(CONVERTED))

configure: aclocal.m4 configure.in
	autoconf	
aclocal.m4:
	aclocal -I buildlib
$(BUILD)/config.status: configure
	test -e $(BUILD) || mkdir $(BUILD)	
	(HERE=`pwd`; cd $(BUILD) && $$HERE/configure)
$(CONVERTED): $(BUILD)/config.status
	(cd $(BUILD) && ./config.status)

# We include the environment if it exists and re-export it to configure. This
# allows someone to edit it and not have their changes blown away.
Env = $(wildcard $(BUILD)/environment.mak)
ifneq ($(words $(Env)),0)
include $(Env)
export CFLAGS CXXFLAGS CPPFLAGS LDFLAGS PICFLAGS
endif


# -*- make -*-

# This make fragment is included by the toplevel make to handle configure
# and setup. It defines a target called startup that when run will init
# the build directory, generate configure from configure.in, create aclocal
# and has rules to run config.status should one of the .in files change.

# Input
#  BUILDDIR - The build directory
#  CONVERTED - List of files output by configure $(BUILD) is prepended
#              The caller must provide depends for these files
# It would be a fairly good idea to run this after a cvs checkout.
BUILDDIR=build

.PHONY: startup
startup: configure $(BUILDDIR)/config.status $(addprefix $(BUILDDIR)/,$(CONVERTED))

# use the files provided from the system instead of carry around
# and use (most of the time outdated) copycats
buildlib/config.sub:
	ln -sf /usr/share/misc/config.sub buildlib/config.sub
buildlib/config.guess:
	ln -sf /usr/share/misc/config.guess buildlib/config.guess	
configure: aclocal.m4 configure.in buildlib/config.guess buildlib/config.sub
	autoconf

aclocal.m4: $(wildcard buildlib/*.m4)
	aclocal -I buildlib
	
$(BUILDDIR)/config.status: configure
	/usr/bin/test -e $(BUILDDIR) || mkdir $(BUILDDIR)	
	(HERE=`pwd`; cd $(BUILDDIR) && $$HERE/configure)
	
$(addprefix $(BUILDDIR)/,$(CONVERTED)):
	(cd $(BUILDDIR) && ./config.status)

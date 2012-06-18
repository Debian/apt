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

.PHONY: startup missing-config-files
startup: configure $(BUILDDIR)/config.status $(addprefix $(BUILDDIR)/,$(CONVERTED))

# use the files provided from the system instead of carry around
# and use (most of the time outdated) copycats
ifeq (file-okay,$(shell test -r buildlib/config.sub && echo 'file-okay'))
buildlib/config.sub:
else
   ifeq (file-okay,$(shell test -r /usr/share/misc/config.sub && echo 'file-okay'))
buildlib/config.sub:
	ln -sf /usr/share/misc/config.sub buildlib/config.sub
   else
buildlib/config.sub: missing-config-files
   endif
endif

ifeq (file-okay,$(shell test -r buildlib/config.guess && echo 'file-okay'))
buildlib/config.guess:
else
   ifeq (file-okay,$(shell test -r /usr/share/misc/config.guess && echo 'file-okay'))
buildlib/config.guess:
	ln -sf /usr/share/misc/config.guess buildlib/config.guess
   else
buildlib/config.guess: missing-config-files
   endif
endif

missing-config-files:
	@echo "APT needs 'config.guess' and 'config.sub' in buildlib/ for configuration."
	@echo "On Debian systems these are available in the 'autotools-dev' package."
	@echo
	@echo "The latest versions can be acquired from the upstream git repository:"
	@echo "http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD"
	@echo "http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD"
	exit 100

configure: aclocal.m4 configure.in buildlib/config.guess buildlib/config.sub
	autoconf

aclocal.m4: $(wildcard buildlib/*.m4)
	aclocal -I buildlib

$(BUILDDIR)/config.status: configure
	/usr/bin/test -e $(BUILDDIR) || mkdir $(BUILDDIR)
	(HERE=`pwd`; cd $(BUILDDIR) && $$HERE/configure)

$(addprefix $(BUILDDIR)/,$(CONVERTED)): $(BUILDDIR)/config.status

# -*- make -*-
BASE=..
SUBDIR=vendor

# Bring in the default rules
include ../buildlib/defaults.mak

all headers library binary program doc manpages docbook test update-po startup dirs: current
all: all/subdirs
binary: binary/subdirs
doc: doc/subdirs
clean: clean/subdirs
veryclean: veryclean/subdirs
dirs: dirs/subdirs
manpages: manpages/subdirs

all/subdirs binary/subdirs doc/subdirs dirs/subdirs manpages/subdirs clean/subdirs veryclean/subdirs:
	test ! -e current/makefile || $(MAKE) -C current $(patsubst %/subdirs,%,$@)
	test ! -e current/makefile.auto || $(MAKE) -C current -f makefile.auto $(patsubst %/subdirs,%,$@)

current:
	rm -f $@
	# search for an exact match to use the correct sources.list example
	find -mindepth 1 -maxdepth 1 -type d | cut -d'/' -f 2 | while read DISTRO; do \
		if dpkg-vendor --is $$DISTRO; then \
			ln -s $$DISTRO $@; \
			break; \
		fi; \
	done
	# if we haven't found a specific, look for a deriving
	# we do ubuntu and debian last as those are the biggest families
	# and would therefore potentially 'shadow' smaller families
	# (especially debian as it sorts quiet early)
	if ! test -e $@; then \
		find -mindepth 1 -maxdepth 1 -type d | cut -d'/' -f 2 | while read DISTRO; do \
			if [ "$$DISTRO" = 'debian' -o "$$DISTRO" = 'ubuntu' ]; then continue; fi; \
			if dpkg-vendor --derives-from $$DISTRO; then \
				ln -s $$DISTRO $@; \
				break; \
			fi; \
		done; \
		test -e $@ || \
			(dpkg-vendor --derives-from ubuntu && cp ln -s ubuntu $@ ) || \
			ln -s debian $@; \
	fi
	if test ! -e current/makefile; then \
		sed "s#@@VENDOR@@#$(notdir $(shell readlink -f current))#" vendor.makefile > current/makefile.auto; \
	fi

.PHONY: clean veryclean all binary vendor

clean: clean/current
clean/current: clean/subdirs
	rm -f current/makefile.auto current

veryclean: veryclean/current
veryclean/current: veryclean/subdirs
	rm -f current/makefile.auto current

# -*- make -*-
BASE=..
SUBDIR=vendor

# Bring in the default rules
include ../buildlib/defaults.mak

all headers library binary program doc manpages debiandoc test update-po startup dirs: current
all: all/subdirs
binary: binary/subdirs
doc: doc/subdirs
clean: clean/subdirs
veryclean: veryclean/subdirs
dirs: dirs/subdirs
manpages: manpages/subdirs

all/subdirs binary/subdirs doc/subdirs dirs/subdirs manpages/subdirs:
	$(MAKE) -C current $(patsubst %/subdirs,%,$@)

clean/subdirs veryclean/subdirs:
	test ! -e current || $(MAKE) -C current $(patsubst %/subdirs,%,$@)

current:
	rm -f $@
	# search for an exact match to use the correct sources.list example
	find -mindepth 1 -maxdepth 1 -type d | cut -d'/' -f 2 | while read DISTRO; do \
		if dpkg-vendor --is $$DISTRO; then \
			ln -s $$DISTRO $@; \
			break; \
		fi; \
	done
	# if we haven't found a specific, look for a deriving in hardcoded order
	test -e $@ || \
		(dpkg-vendor --derives-from ubuntu && cp ln -s ubuntu $@ ) || \
		ln -s debian $@

.PHONY: clean veryclean all binary vendor

clean: clean/current

clean/current:
	test ! -e current || $(MAKE) -C current clean
	rm -f current

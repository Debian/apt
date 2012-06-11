# -*- make -*-
BASE=..
SUBDIR=doc
SUBDIRS= $(dir $(wildcard */makefile))

# Bring in the default rules
include ../buildlib/defaults.mak

# Debian Doc SGML Documents
SOURCE = $(wildcard *.sgml)
DEBIANDOC_HTML_OPTIONS=-l en.UTF-8
include $(DEBIANDOC_H)

MANPAGEPO = $(patsubst %.po,%,$(notdir $(wildcard po/*.po)))
MANPAGEPOLIST = $(patsubst %,manpages-translation-%,$(MANPAGEPO))

doc: manpages

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC
# generate a list of accepted man page translations
SOURCE = $(patsubst %.xml,%,$(wildcard *.?.xml))
INCLUDES = apt.ent apt-verbatim.ent
STYLESHEET=manpage-style.xsl

LOCAL := manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
manpages: $(MANPAGEPOLIST) $($(LOCAL)-LIST)

$($(LOCAL)-LIST) :: % : %.xml $(STYLESHEET) $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

$(MANPAGEPOLIST) :: manpages-translation-% : %/makefile po4a
	$(MAKE) -C $(dir $<) doc

.PHONY: manpages dirs-manpage-subdirs $(MANPAGEPOLIST)
dirs: dirs-manpage-subdirs
dirs-manpage-subdirs:
	for i in $(MANPAGEPO); do \
		test -d $$i || mkdir $$i; \
		test -f $$i/makefile || sed "s#@@LANG@@#$$i#" lang.makefile > $$i/makefile; \
	done

# Clean rule
.PHONY: veryclean/$(LOCAL)
veryclean: veryclean/$(LOCAL)
veryclean/$(LOCAL):
	-rm -rf $($(@F)-LIST)

endif

# Chain to the manpage rule
SOURCE = apt.8
include $(MANPAGE_H)

examples/sources.list: examples/sources.list.in apt-verbatim.ent
	sed -e 's#&stable-codename;#$(shell grep --max-count=1 '^<!ENTITY stable-codename "' apt-verbatim.ent | cut -d'"' -f 2)#g' examples/sources.list.in > examples/sources.list

# Examples
SOURCE = examples/apt.conf examples/sources.list examples/configure-index examples/apt-https-method-example.conf
TO = $(DOC)
TARGET = binary
include $(COPY_H)

.PHONY: clean clean-subdirs veryclean veryclean-subdirs all binary doc

clean: clean-subdirs
veryclean: veryclean-subdirs

clean-subdirs:
	for dir in $(SUBDIRS); do\
		$(MAKE) -C $$dir clean; \
	done

veryclean-subdirs:
	for dir in $(SUBDIRS); do\
		rm -rf $$dir; \
	done

.PHONY: update-po po4a stats

ifdef PO4A
doc: po4a

update-po:
	po4a --previous --no-backups --force --no-translations \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf

po4a:
	po4a --previous --no-backups \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf
endif

stats:
	for i in po/*.po; do echo -n "$$i: "; msgfmt --output-file=/dev/null --statistics $$i; done

ifdef DOXYGEN
DOXYGEN_SOURCES = $(shell find $(BASE)/apt-pkg -not -name .\\\#* -and \( -name \*.cc -or -name \*.h \) )

clean: doxygen-clean

doxygen-clean:
	rm -fr $(BUILD)/doc/doxygen
	rm -f $(BUILD)/doc/doxygen-stamp

$(BUILD)/doc/doxygen-stamp: $(DOXYGEN_SOURCES) $(BUILD)/doc/Doxyfile
	rm -fr $(BUILD)/doc/doxygen
	mkdir $(BUILD)/doc/doxygen  # some versions seem to not create this directory #628799
	$(DOXYGEN) $(BUILD)/doc/Doxyfile
	touch $(BUILD)/doc/doxygen-stamp

doc: $(BUILD)/doc/doxygen-stamp
endif

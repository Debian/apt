# -*- make -*-
BASE=..
SUBDIR=doc

# Bring in the default rules
include ../buildlib/defaults.mak

# DocBook XML Documents
SOURCE = $(wildcard *.dbk)
LC = en
include $(DOCBOOK_H)

doc: manpages docbook

examples/sources.list: ../vendor/current/sources.list
	ln -sf '$(abspath $^)' $@

apt-vendor.ent: ../vendor/current/apt-vendor.ent
	ln -sf '$(abspath $^)' $@

# Examples
SOURCE = examples/apt.conf examples/sources.list examples/configure-index examples/apt-https-method-example.conf
TO = $(DOC)
TARGET = binary
include $(COPY_H)

.PHONY: clean clean/subdirs veryclean veryclean/subdirs manpages/subdirs docbook/subdirs all binary doc stats

clean: clean/subdirs clean/examples
veryclean: veryclean/subdirs clean/examples
manpages: apt-vendor.ent manpages/subdirs
docbook: apt-vendor.ent docbook/subdirs

DOCUMENTATIONPO = $(patsubst %.po,%,$(notdir $(wildcard po/*.po)))
DOCDIRLIST = $(addsuffix /makefile,$(DOCUMENTATIONPO))

dirs: $(DOCDIRLIST)
$(DOCDIRLIST) :: %/makefile : lang.makefile
	test -d $(dir $@) || mkdir $(dir $@)
	sed "s#@@LANG@@#$(subst /,,$(dir $@))#" $< > $@

docbook/subdirs manpages/subdirs clean/subdirs veryclean/subdirs:
	for dir in en $(dir $(DOCDIRLIST)); do \
		$(MAKE) -C $$dir $(patsubst %/subdirs,%,$@); \
	done

clean/examples:
	rm -f examples/sources.list
	rm -f apt-vendor.ent

stats:
	for i in po/*.po; do echo -n "$$i: "; msgfmt --output-file=/dev/null --statistics $$i; done

ifdef PO4A
MANPAGEPOLIST = $(addprefix manpages-translation-,$(DOCUMENTATIONPO))
DOCBOOKPOLIST = $(addprefix docbook-translation-,$(DOCUMENTATIONPO))

.PHONY: update-po po4a $(MANPAGEPOLIST) $(DOCBOOKPOLIST) $(DOCDIRLIST)

po4a: manpages/subdirs docbook/subdirs

update-po:
	po4a --previous --no-backups --force --no-translations \
		--msgmerge-opt --add-location=file \
		--porefs noline,wrap \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf
	sed -n '/^#$$/,$$p' po/apt-doc.pot > po/apt-doc.pot.headerfree
	#cat po/apt-doc.pot > po/apt-doc.pot.headerfree
	cat po/header.pot.txt po/apt-doc.pot.headerfree > po/apt-doc.pot
	rm -f po/apt-doc.pot.headerfree

manpages/subdirs: $(MANPAGEPOLIST)
$(MANPAGEPOLIST) :: manpages-translation-% : %/makefile po4a.conf
	po4a --previous --no-backups --translate-only $(dir $<)apt.ent \
		--msgmerge-opt --add-location=file \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.1.xml,%.$(subst /,,$(dir $<)).1.xml,$(wildcard *.1.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.2.xml,%.$(subst /,,$(dir $<)).2.xml,$(wildcard *.2.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.3.xml,%.$(subst /,,$(dir $<)).3.xml,$(wildcard *.3.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.4.xml,%.$(subst /,,$(dir $<)).4.xml,$(wildcard *.4.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.5.xml,%.$(subst /,,$(dir $<)).5.xml,$(wildcard *.5.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.6.xml,%.$(subst /,,$(dir $<)).6.xml,$(wildcard *.6.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.7.xml,%.$(subst /,,$(dir $<)).7.xml,$(wildcard *.7.xml))) \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.8.xml,%.$(subst /,,$(dir $<)).8.xml,$(wildcard *.8.xml))) \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf

docbook/subdirs: $(DOCBOOKPOLIST)
$(DOCBOOKPOLIST) :: docbook-translation-% : %/makefile po4a.conf
	po4a --previous --no-backups --translate-only $(dir $<)apt.ent \
		--msgmerge-opt --add-location=file \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.dbk,%.$(subst /,,$(dir $<)).dbk,$(wildcard *.dbk))) \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf
endif

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

docbook: $(BUILD)/doc/doxygen-stamp
endif

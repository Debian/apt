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

doc: manpages debiandoc

# Do not use XMLTO, build the manpages directly with XSLTPROC
ifdef XSLTPROC
# generate a list of accepted man page translations
SOURCE = $(patsubst %.xml,%,$(wildcard *.?.xml))
INCLUDES = apt.ent apt-verbatim.ent
STYLESHEET=manpage-style.xsl

LOCAL := manpage-$(firstword $(SOURCE))
$(LOCAL)-LIST := $(SOURCE)

# Install generation hooks
manpages: $($(LOCAL)-LIST)

$($(LOCAL)-LIST) :: % : %.xml $(STYLESHEET) $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

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

.PHONY: clean clean-subdirs veryclean veryclean-subdirs all binary doc stats

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

stats:
	for i in po/*.po; do echo -n "$$i: "; msgfmt --output-file=/dev/null --statistics $$i; done

ifdef PO4A
DOCUMENTATIONPO = $(patsubst %.po,%,$(notdir $(wildcard po/*.po)))
MANPAGEPOLIST = $(addprefix manpages-translation-,$(DOCUMENTATIONPO))
DEBIANDOCPOLIST = $(addprefix debiandoc-translation-,$(DOCUMENTATIONPO))

MANPAGEDIRLIST = $(addsuffix /makefile,$(DOCUMENTATIONPO))

.PHONY: update-po po4a $(MANPAGEPOLIST) $(MANPAGEDIRLIST)

manpages: $(MANPAGEPOLIST)
debiandoc: $(DEBIANDOCPOLIST)
po4a: $(MANPAGEPOLIST) $(DEBIANDOCPOLIST)

update-po:
	po4a --previous --no-backups --force --no-translations \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf

$(MANPAGEPOLIST) :: manpages-translation-% : %/makefile po4a.conf
	po4a --previous --no-backups --translate-only $(dir $<)apt.ent \
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
	$(MAKE) -C $(dir $<) manpages

$(DEBIANDOCPOLIST) :: debiandoc-translation-% : %/makefile po4a.conf
	po4a --previous --no-backups --translate-only $(dir $<)apt.ent \
		$(patsubst %,--translate-only $(dir $<)%,$(patsubst %.sgml,%.$(subst /,,$(dir $<)).sgml,$(wildcard *.sgml))) \
		--package-name='$(PACKAGE)-doc' --package-version='$(PACKAGE_VERSION)' \
		--msgid-bugs-address='$(PACKAGE_MAIL)' po4a.conf
	$(MAKE) -C $(dir $<) debiandoc

dirs: $(MANPAGEDIRLIST)
$(MANPAGEDIRLIST) :: %/makefile : lang.makefile
	test -d $(dir $@) || mkdir $(dir $@)
	sed "s#@@LANG@@#$(subst /,,$(dir $@))#" $< > $@
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

debiandoc: $(BUILD)/doc/doxygen-stamp
endif

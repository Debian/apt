# -*- make -*-
BASE=..
SUBDIR=doc
SUBDIRS= fr ja pl pt_BR es

# Bring in the default rules
include ../buildlib/defaults.mak

# Debian Doc SGML Documents
SOURCE = dpkg-tech.sgml design.sgml files.sgml guide.sgml guide.it.sgml \
         cache.sgml method.sgml offline.sgml
include $(DEBIANDOC_H)

# XML man pages
SOURCE = apt-cache.8 apt-get.8 apt-cdrom.8 apt.conf.5 sources.list.5 \
         apt-config.8 apt_preferences.5  \
         apt-sortpkgs.1 apt-ftparchive.1 apt-extracttemplates.1 \
	 apt-key.8 apt-secure.8 apt-mark.8
INCLUDES = apt.ent	 
include $(XML_MANPAGE_H)

# Examples
SOURCE = examples/apt.conf examples/sources.list examples/configure-index examples/apt-https-method-example.conf
TO = $(DOC)
TARGET = binary
include $(COPY_H)

#.PHONY: headers library clean veryclean all binary program doc doc.pt_BR doc.fr
doc: doc.fr doc.ja doc.pl doc.pt_BR doc.es

doc.fr: %.fr:
	$(MAKE) -C fr $*

doc.pt_BR: %.pt_BR:
	$(MAKE) -C pt_BR $*

doc.es: %.es:
	$(MAKE) -C es $*

doc.ja: %.ja:
	$(MAKE) -C ja $*

doc.pl: %.pl:
	$(MAKE) -C pl $*

ifdef DOXYGEN
DOXYGEN_SOURCES = $(shell find $(BASE)/apt-pkg -not -name .\\\#* -and \( -name \*.cc -or -name \*.h \) )

clean: doxygen-clean

doxygen-clean:
	rm -fr $(BUILD)/doc/doxygen
	rm -f $(BUILD)/doc/doxygen-stamp

$(BUILD)/doc/Doxyfile: Doxyfile.in
	(cd $(BUILD) && ./config.status doc/Doxyfile)

$(BUILD)/doc/doxygen-stamp: $(DOXYGEN_SOURCES) $(BUILD)/doc/Doxyfile
	rm -fr $(BUILD)/doc/doxygen
	$(DOXYGEN) $(BUILD)/doc/Doxyfile
	touch $(BUILD)/doc/doxygen-stamp

doc: $(BUILD)/doc/doxygen-stamp

endif

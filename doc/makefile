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
	 apt-key.8 apt-secure.8
INCLUDES = apt.ent	 
include $(XML_MANPAGE_H)

# Examples
SOURCE = examples/apt.conf examples/sources.list examples/configure-index 
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

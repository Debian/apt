# -*- make -*-
BASE=..
SUBDIR=doc

# Bring in the default rules
include ../buildlib/defaults.mak

# Debian Doc SGML Documents
SOURCE = dpkg-tech.sgml design.sgml files.sgml guide.sgml guide.it.sgml \
         cache.sgml method.sgml offline.sgml
include $(DEBIANDOC_H)

# Man pages
SOURCE = apt-cache.8 apt-get.8 apt-cdrom.8 apt.conf.5 sources.list.5 \
         apt-config.8 apt-sortpkgs.1 apt-ftparchive.1 apt_preferences.5 \
	 apt-extracttemplates.1 vendors.list.5
INCLUDES = apt.ent	 
include $(SGML_MANPAGE_H)

# Examples
SOURCE = examples/apt.conf examples/sources.list examples/configure-index 
TO = $(DOC)
TARGET = doc
include $(COPY_H)

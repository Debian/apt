# -*- make -*-
BASE=..
SUBDIR=doc

# Bring in the default rules
include ../buildlib/defaults.mak

# SGML Documents
SOURCE = dpkg-tech.sgml design.sgml files.sgml guide.sgml cache.sgml \
         method.sgml
include $(DEBIANDOC_H)

# Man pages
SOURCE = apt-cache.8 apt-get.8 apt-cdrom.8 apt.conf.5 sources.list.5 apt-config.8
include $(YODL_MANPAGE_H)

# Examples
SOURCE = examples/apt.conf examples/sources.list
TO = $(DOC)
include $(COPY_H)

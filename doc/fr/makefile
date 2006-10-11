# -*- make -*-
BASE=../..
SUBDIR=doc/fr

# Bring in the default rules
include ../../buildlib/defaults.mak

# Do not use XMLTO, build the manpages directly with XSLTPROC
XSLTPROC=/usr/bin/xsltproc
STYLESHEET=./style.fr.xsl


# Man pages
SOURCE = apt-cache.fr.8 apt-get.fr.8 apt-cdrom.fr.8 apt.conf.fr.5 \
         sources.list.fr.5 apt-config.fr.8 apt-sortpkgs.fr.1 \
	 apt-ftparchive.fr.1 apt_preferences.fr.5 apt-extracttemplates.fr.1 \
	 apt-key.fr.8 apt-secure.fr.8

INCLUDES = apt.ent.fr

doc: $(SOURCE) 

$(SOURCE) ::  % : %.xml $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

apt-cache.fr.8:: apt-cache.8 
	cp $< $@

apt-get.fr.8:: apt-get.8
	cp $< $@

apt-cdrom.fr.8:: apt-cdrom.8 
	cp $< $@

apt.conf.fr.5:: apt.conf.5 
	cp $< $@

apt-config.fr.8:: apt-config.8 
	cp $< $@

sources.list.fr.5:: sources.list.5
	cp $< $@

apt-sortpkgs.fr.1:: apt-sortpkgs.1
	cp $< $@

apt-ftparchive.fr.1:: apt-ftparchive.1 
	cp $< $@

apt_preferences.fr.5:: apt_preferences.5
	cp $< $@

apt-extracttemplates.fr.1:: apt-extracttemplates.1
	cp $< $@

apt-key.fr.8:: apt-key.8 
	cp $< $@

apt-secure.fr.8:: apt-secure.8 
	cp $< $@



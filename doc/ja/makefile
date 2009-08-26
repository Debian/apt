# -*- make -*-
BASE=../..
SUBDIR=doc/ja

# Bring in the default rules
include ../../buildlib/defaults.mak

# Do not use XMLTO, build the manpages directly with XSLTPROC
XSLTPROC=/usr/bin/xsltproc
STYLESHEET=./style.ja.xsl


# Man pages
SOURCE = apt-cache.ja.8 apt-get.ja.8 apt-cdrom.ja.8 apt.conf.ja.5 \
	 sources.list.ja.5 apt-config.ja.8 apt-sortpkgs.ja.1 \
	 apt-ftparchive.ja.1 apt_preferences.ja.5 apt-extracttemplates.ja.1 \
	 apt-key.ja.8 apt-secure.ja.8 apt-mark.ja.8

INCLUDES = apt.ent

doc: $(SOURCE)

clean: clean-manpages

clean-manpages:
	rm -f $(SOURCE) \
	      $(patsubst %.ja.1,%.1,$(SOURCE)) \
	      $(patsubst %.ja.5,%.5,$(SOURCE)) \
	      $(patsubst %.ja.8,%.8,$(SOURCE))

$(SOURCE) ::  % : %.xml $(INCLUDES)
	echo Creating man page $@
	$(XSLTPROC) -o $@ $(STYLESHEET) $<

apt-cache.ja.8:: apt-cache.8
	cp $< $@

apt-get.ja.8:: apt-get.8
	cp $< $@

apt-cdrom.ja.8:: apt-cdrom.8
	cp $< $@

apt.conf.ja.5:: apt.conf.5
	cp $< $@

apt-config.ja.8:: apt-config.8
	cp $< $@

sources.list.ja.5:: sources.list.5
	cp $< $@

apt-sortpkgs.ja.1:: apt-sortpkgs.1
	cp $< $@

apt-ftparchive.ja.1:: apt-ftparchive.1
	cp $< $@

apt_preferences.ja.5:: apt_preferences.5
	cp $< $@

apt-extracttemplates.ja.1:: apt-extracttemplates.1
	cp $< $@

apt-key.ja.8:: apt-key.8
	cp $< $@

apt-secure.ja.8:: apt-secure.8
	cp $< $@

apt-mark.ja.8:: apt-mark.8
	cp $< $@




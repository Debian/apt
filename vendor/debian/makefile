# -*- make -*-
BASE=../..
SUBDIR=vendor/debian

# Bring in the default rules
include ../../buildlib/defaults.mak

doc: sources.list

sources.list: sources.list.in ../../doc/apt-verbatim.ent
	sed -e 's#&stable-codename;#$(shell grep --max-count=1 '^<!ENTITY stable-codename "' ../../doc/apt-verbatim.ent | cut -d'"' -f 2)#g' $^ > $@

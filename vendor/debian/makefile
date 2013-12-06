# -*- make -*-
BASE=../..
SUBDIR=vendor/debian

# Bring in the default rules
include ../../buildlib/defaults.mak

doc binary manpages: sources.list

sources.list: sources.list.in ../../doc/apt-verbatim.ent
	sed -e 's#&stable-codename;#$(shell ../getinfo debian-stable-codename)#g' $< > $@

# -*- make -*-
BASE=../..
SUBDIR=vendor/tanglu

# Bring in the default rules
include ../../buildlib/defaults.mak

doc binary manpages: sources.list

sources.list: sources.list.in ../../doc/apt-verbatim.ent
	sed -e 's#&tanglu-codename;#$(shell ../getinfo tanglu-codename)#g' $< > $@

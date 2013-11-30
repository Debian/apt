# -*- make -*-
BASE=../..
SUBDIR=vendor/ubuntu

# Bring in the default rules
include ../../buildlib/defaults.mak

doc: sources.list

sources.list: sources.list.in ../../doc/apt-verbatim.ent
	sed -e 's#&ubuntu-codename;#$(shell grep --max-count=1 '^<!ENTITY ubuntu-codename "' ../../doc/apt-verbatim.ent | cut -d'"' -f 2)#g' $^ > $@

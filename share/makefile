
BASE=..
SUBDIR=share

# Bring in the default rules
include ../buildlib/defaults.mak

binary: apt-auth-failure.note.h

apt-auth-failure.note.h: apt-auth-failure.note
	intltool-extract -t gettext/rfc822deb apt-auth-failure.note
	echo "share/apt-auth-failure.note.h" >> $(BUILD)/po/domains/apt/apt-auth-failure.note.srclist

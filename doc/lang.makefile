# -*- make -*-
BASE=../..
SUBDIR=doc/@@LANG@@

# Bring in the default rules
include ../../buildlib/defaults.mak

# Language Code of this translation
LC=@@LANG@@

include $(PO4A_MANPAGE_H)

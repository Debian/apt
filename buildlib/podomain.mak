# -*- make -*-

# This creates the file listing used by xgettext and friends based on the
# declared domain of the make file. It also arranges to set the DOMAIN 
# CPPFLAG for the compilation.

MY_DOMAIN := $(PACKAGE)
ifdef APT_DOMAIN
$($(LOCAL)-OBJS): CPPFLAGS := $(CPPFLAGS) -DAPT_DOMAIN='"$(APT_DOMAIN)"'
MY_DOMAIN := $(APT_DOMAIN)
endif

MKDIRS += $(PO_DOMAINS)/$(MY_DOMAIN)
$(PO_DOMAINS)/$(MY_DOMAIN)/$(LOCAL).$(TYPE)list: SRC := $(addprefix $(SUBDIR)/,$(SOURCE))
$(PO_DOMAINS)/$(MY_DOMAIN)/$(LOCAL).$(TYPE)list: makefile
	(echo $(SRC) | xargs -n1 echo) > $@
binary program clean: $(PO_DOMAINS)/$(MY_DOMAIN)/$(LOCAL).$(TYPE)list

veryclean: veryclean/$(LOCAL)
veryclean/po/$(LOCAL): LIST := $(PO_DOMAINS)/$(MY_DOMAIN)/$(LOCAL).$(TYPE)list
veryclean/po/$(LOCAL):
	rm -f $(LIST)

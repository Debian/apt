# -*- make -*-

# This processes debian-doc sgml to produce html and plain text output

# Input
# $(SOURCE) - The documents to use

# All output is writtin to files in the build doc directory

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := debiandoc-$(firstword $(SOURCE))
$(LOCAL)-HTML := $(addsuffix .html,$(addprefix $(DOC)/,$(basename $(SOURCE))))
$(LOCAL)-TEXT := $(addsuffix .text,$(addprefix $(DOC)/,$(basename $(SOURCE))))

#---------

# Rules to build HTML documentations
ifdef DEBIANDOC_HTML

# Install generation hooks
doc: $($(LOCAL)-HTML)
veryclean: veryclean/html/$(LOCAL)

vpath %.sgml $(SUBDIRS)
$(DOC)/%.html: %.sgml
	echo Creating html for $< to $@
	-rm -rf $@
	(HERE=`pwd`; cd $(@D) && $(DEBIANDOC_HTML) $(DEBIANDOC_HTML_OPTIONS) $$HERE/$<)

# Clean rule
.PHONY: veryclean/html/$(LOCAL)
veryclean/html/$(LOCAL):
	-rm -rf $($(@F)-HTML)
	
endif

#---------

# Rules to build Text documentations
ifdef DEBIANDOC_TEXT

# Install generation hooks
doc: $($(LOCAL)-TEXT)
veryclean: veryclean/text/$(LOCAL)

vpath %.sgml $(SUBDIRS)
$(DOC)/%.text: %.sgml
	echo Creating text for $< to $@
	$(DEBIANDOC_TEXT) -O $< > $@

# Clean rule
.PHONY: veryclean/text/$(LOCAL)
veryclean/text/$(LOCAL):
	-rm -rf $($(@F)-TEXT)
	
endif

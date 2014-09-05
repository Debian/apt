# -*- make -*-

# This processes DocBook XML to produce html and plain text output

# Input
# $(SOURCE) - The documents to use

# All output is written to files in the build doc directory

# See defaults.mak for information about LOCAL

# Some local definitions
LOCAL := docbook-$(firstword $(SOURCE))
$(LOCAL)-HTML := $(addsuffix .html,$(addprefix $(DOC)/,$(basename $(SOURCE))))
$(LOCAL)-TEXT := $(addsuffix .text,$(addprefix $(DOC)/,$(basename $(SOURCE))))
INCLUDES = apt.ent apt-verbatim.ent apt-vendor.ent

docbook:


#---------

# Rules to build HTML documentations
ifdef XSLTPROC

DOCBOOK_HTML_STYLESHEET := docbook-html-style.xsl

# Install generation hooks
docbook: $($(LOCAL)-HTML)
veryclean: veryclean/html/$(LOCAL)

vpath %.dbk $(SUBDIRS)
vpath $(DOCBOOK_HTML_STYLESHEET) $(SUBDIRS)
$(DOC)/%.html: %.dbk $(DOCBOOK_HTML_STYLESHEET) $(INCLUDES)
	echo Creating html for $< to $@
	-rm -rf $@
	mkdir -p $@
	$(DOCBOOK) \
		--stringparam base.dir $@/ \
		--stringparam l10n.gentext.default.language $(LC) \
		$(<D)/$(DOCBOOK_HTML_STYLESHEET) $< || exit 199

# Clean rule
.PHONY: veryclean/html/$(LOCAL)
veryclean/html/$(LOCAL):
	-rm -rf $($(@F)-HTML)

endif

#---------

# Rules to build Text documentations
ifdef XSLTPROC

DOCBOOK_TEXT_STYLESHEET := docbook-text-style.xsl

# Install generation hooks
docbook: $($(LOCAL)-TEXT)
veryclean: veryclean/text/$(LOCAL)

vpath %.dbk $(SUBDIRS)
vpath $(DOCBOOK_TEXT_STYLESHEET) $(SUBDIRS)
$(DOC)/%.text: %.dbk $(DOCBOOK_TEXT_STYLESHEET) $(INCLUDES)
	echo Creating text for $< to $@
	$(DOCBOOK) \
		--stringparam l10n.gentext.default.language $(LC) \
		$(<D)/$(DOCBOOK_TEXT_STYLESHEET) $< | \
		LC_ALL=C.UTF-8 $(DOCBOOK2TEXT) > $@ || exit 198

# Clean rule
.PHONY: veryclean/text/$(LOCAL)
veryclean/text/$(LOCAL):
	-rm -rf $($(@F)-TEXT)

endif

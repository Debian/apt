# -*- make -*-

# This will compile the gettext message catalogues.

# The other make files drop file lists in build/po/domains/DOMAIN/* which
# is then picked up by this make file to define all the domains and all the
# source files that compose each domain. It then produces the POT files 
# and then filters the PO files through the per-domain POT file to create
# the input to msgformat, which is then dumped into a locale directory.

BASE=..
SUBDIR=po

# Bring in the default rules
include ../buildlib/defaults.mak

CATALOGS := $(addsuffix .gmo, $(shell sed -e '/^\#/ d' -e '/^$$/ d' LINGUAS | tr '\n' ' '))
DOMAINS := $(sort $(notdir $(wildcard $(PO_DOMAINS)/*)))
POTFILES := $(addsuffix .pot,$(addprefix $(PO)/,$(DOMAINS)))

# Construct a list of all mo files for all domains under $(PO_DOMAINS)
MOFILES := $(patsubst %.gmo,%.mo,$(CATALOGS))
MOFILES := $(foreach D,$(DOMAINS),$(addprefix $(PO_DOMAINS)/$(D)/,$(MOFILES)))
LANG_POFILES := $(patsubst %.mo,%.po,$(MOFILES))
LINGUAS := $(patsubst %.gmo,%,$(CATALOGS))

GETDOMAIN = $(word 1,$(subst /, ,$(1)))

# Generate the list of files from the bits the other make files dropped 
# and produce the .pot file.
$(POTFILES) : $(PO)/%.pot :
	echo "Generating POT file $@"
	echo $@ : $(wildcard $(PO)/domains/$*/*.*list) $(addprefix $(BASE)/,$(shell cat $(wildcard $(PO)/domains/$*/*.srclist))) > $@.d.tmp
	cmp --silent $@.d.tmp $@.d || mv $@.d.tmp $@.d
# From sh source
	cat $(PO)/domains/$*/*.shlist 2> /dev/null | (cd $(BASE) && xargs -n1 bash --dump-po-strings) > $(PO)/domains/$*/sh.pot.tmp
	cmp --silent $(PO)/domains/$*/sh.pot.tmp $(PO)/domains/$*/sh.pot || \
		mv $(PO)/domains/$*/sh.pot.tmp $(PO)/domains/$*/sh.pot
# From C/C++ source
	cat $(PO)/domains/$*/*.srclist | sort > $(PO)/POTFILES_$*.in
	$(XGETTEXT) --default-domain=$* --directory=$(BASE) \
	  --add-comments --foreign --keyword=_ --keyword=N_ \
          --no-location \
	  --keyword=P_:1,2 \
	  --files-from=$(PO)/POTFILES_$*.in -o $(PO)/domains/$*/c.pot.tmp
	cmp --silent $(PO)/domains/$*/c.pot.tmp $(PO)/domains/$*/c.pot || \
		mv $(PO)/domains/$*/c.pot.tmp $(PO)/domains/$*/c.pot
	rm -f $(PO)/POTFILES_$*.in
	$(MSGCOMM) --omit-header --more-than=0 $(PO)/domains/$*/c.pot $(PO)/domains/$*/sh.pot --output=$@.tmp
	cmp --silent $@ $@.tmp || mv $@.tmp $@
	# copy into the domain dirs to make rosetta happy
	rm -f $(PO)/domains/$*/*.pot
	cp $@ $(PO)/domains/$*

# Filter the complete translation with the domain specific file to produce
# only the subtext needed for this domain
# We cannot express the dependencies required for this directly with a pattern
# rule, so we use the .d hack.
$(LANG_POFILES) : $(PO_DOMAINS)/%.po : $(POTFILES)
	printf "%s " "Generating $@"
	echo $@ : $(notdir $@) $(PO)/$(call GETDOMAIN,$*).pot > $(PO)/$(call GETDOMAIN,$*)_$(notdir $@).d
	$(MSGMERGE) --no-location $(notdir $@) $(PO)/$(call GETDOMAIN,$*).pot -o $@

$(MOFILES) : $(PO_DOMAINS)/%.mo : $(PO_DOMAINS)/%.po
	printf "%s: " "Generating $(LOCALE)/$(notdir $*)/LC_MESSAGES/$(call GETDOMAIN,$*).mo"
	$(GMSGFMT) --statistics -o $@ $<
	mkdir -p $(LOCALE)/$(notdir $*)/LC_MESSAGES/
	cp $@ $(LOCALE)/$(notdir $*)/LC_MESSAGES/$(call GETDOMAIN,$*).mo

stats: all-linguas-enabled
	for i in *.pot *.po; do echo -n "$$i: "; msgfmt --output-file=/dev/null --statistics $$i; done

binary: $(POTFILES) $(MOFILES) all-linguas-enabled

$(PACKAGE)-all.pot: $(POTFILES)
	# we create our partial pot files without a header to avoid changing dates in *.mo files,
	# but we want a header for our master-pot file, so we use a dummy pot with nothing but the header
	$(XGETTEXT) --default-domain=$(PO)/$(PACKAGE)-dummy.pot --foreign --language=c \
		-o $(PO)/$(PACKAGE)-dummy.pot --force-po --package-name='$(PACKAGE)' \
		--package-version='$(PACKAGE_VERSION)' --msgid-bugs-address='$(PACKAGE_MAIL)' /dev/null
	$(MSGCOMM) --more-than=0 $(PO)/$(PACKAGE)-dummy.pot $(POTFILES) --output=$(PACKAGE)-all.pot
	rm -f $(PO)/$(PACKAGE)-dummy.pot

.PHONY: update-po all-linguas-enabled
update-po: $(PACKAGE)-all.pot all-linguas-enabled
	for lang in ${LINGUAS}; do \
	echo "Updating $$lang.po"; \
	$(MSGMERGE) $$lang.po $(PACKAGE)-all.pot -o $$lang.new.po; \
	cmp $$lang.new.po $$lang.po || cp $$lang.new.po $$lang.po; \
	rm -f $$lang.new.po; \
	done

all-linguas-enabled: LINGUAS
ifdef NOISY
	echo "Available Translations: $(shell ls *.po | cut -d'.' -f 1 | sort | tr '\n' ' ')"
	echo "Mentioned Translations: $(shell sed -e '/^\#\#/ d' -e '/^$$/ d' -e 's/# //' LINGUAS | tr ' ' '\n' | sort | tr '\n' ' ')"
	echo "Build Translations:     $(shell echo "$(LINGUAS)" | tr ' ' '\n' | sort | tr '\n' ' ')"
endif
	test "$(shell ls *.po | cut -d'.' -f 1 | sort | tr '\n' ' ')" = \
	   "$(shell sed -e '/^\#\#/ d' -e '/^$$/ d' -e 's/# //' LINGUAS | tr ' ' '\n' | sort | tr '\n' ' ')" || \
	   ( echo "ERROR: Not all available translations are mentioned in po/LINGUAS!"; exit 100)

clean: clean/local
clean/local:
	rm -f $(MOFILES) $(LANG_POFILES) $(PO)/*.d

# Include the dependencies that are available
The_DFiles = $(wildcard $(PO)/*.d)
ifneq ($(words $(The_DFiles)),0)
include $(The_DFiles)
endif

# otherwise the output is completely screwed
.NOTPARALLEL:

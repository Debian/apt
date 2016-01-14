# -*- make -*-
BASE=../..
SUBDIR=vendor/@@VENDOR@@

# Bring in the default rules
include ../../buildlib/defaults.mak

doc binary manpages: sources.list

sources.list: sources.list.in ../../doc/apt-verbatim.ent
	while read line; do \
		if [ "$${line}" = '&sourceslist-list-format;' ]; then \
			$(BASE)/vendor/getinfo vendor sourceslist-list-format ; \
		else \
			echo "$${line}"; \
		fi \
	done < $< | sed -e 's#&debian-stable-codename;#$(shell ../getinfo debian-stable-codename)#g' \
		-e 's#&debian-oldstable-codename;#$(shell ../getinfo debian-oldstable-codename)#g' \
		-e 's#&debian-testing-codename;#$(shell ../getinfo debian-testing-codename)#g' \
		-e 's#&ubuntu-codename;#$(shell ../getinfo ubuntu-codename)#g' \
		-e 's#&current-codename;#$(shell ../getinfo current-codename)#g' \
		> $@

clean: clean/sources.list
veryclean: clean/sources.list

clean/sources.list:
	rm -f sources.list

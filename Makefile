# -*- make -*-

# This is the top level make file for APT, it recurses to each lower
# level make file and runs it with the proper target
.SILENT:

.PHONY: headers library clean veryclean all binary program doc
all headers library clean veryclean binary program doc:
	$(MAKE) -C deity $@
	$(MAKE) -C apt-pkg $@
	$(MAKE) -C cmdline $@
	$(MAKE) -C doc $@

# -*- make -*-
# Version number of libapt-pkg.
# Please increase MAJOR with each ABI break,
# with each non-ABI break to the lib, please increase RELEASE.
# The versionnumber is extracted from apt-pkg/macros.h - see also there.
LIBAPTPKG_MAJOR=$(shell awk -v ORS='.' '/^\#define APT_PKG_M/ {print $$3}' $(BASE)/apt-pkg/contrib/macros.h | sed 's/\.$$//')
LIBAPTPKG_RELEASE=$(shell grep '^\#define APT_PKG_RELEASE' $(BASE)/apt-pkg/contrib/macros.h | cut -d ' ' -f 3)

# Version number of libapt-inst
# Please increase MAJOR with each ABI break,
# with each non-ABI break to the lib, please increase MINOR.
# The versionnumber is extracted from apt-inst/makefile - see also there.
LIBAPTINST_MAJOR=$(shell grep '^MAJOR=' $(BASE)/apt-inst/makefile |cut -d '=' -f 2)
LIBAPTINST_MINOR=$(shell grep '^MINOR=' $(BASE)/apt-inst/makefile |cut -d '=' -f 2)

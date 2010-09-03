// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
    Collection of functor classes */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>

#include <apti18n.h>

#include <string>

#include <regex.h>
									/*}}}*/
namespace APT {
namespace CacheFilter {
PackageNameMatchesRegEx::PackageNameMatchesRegEx(std::string const &Pattern) {/*{{{*/
	pattern = new regex_t;
	int const Res = regcomp(pattern, Pattern.c_str(), REG_EXTENDED | REG_ICASE | REG_NOSUB);
	if (Res == 0)
		return;

	delete pattern;
	pattern = NULL;
	char Error[300];
	regerror(Res, pattern, Error, sizeof(Error));
	_error->Error(_("Regex compilation error - %s"), Error);
}
									/*}}}*/
bool PackageNameMatchesRegEx::operator() (pkgCache::PkgIterator const &Pkg) {/*{{{*/
	if (unlikely(pattern == NULL))
		return false;
	else
		return regexec(pattern, Pkg.Name(), 0, 0, 0) == 0;
}
									/*}}}*/
bool PackageNameMatchesRegEx::operator() (pkgCache::GrpIterator const &Grp) {/*{{{*/
	if (unlikely(pattern == NULL))
		return false;
	else
		return regexec(pattern, Grp.Name(), 0, 0, 0) == 0;
}
									/*}}}*/
PackageNameMatchesRegEx::~PackageNameMatchesRegEx() {			/*{{{*/
	if (pattern == NULL)
		return;
	regfree(pattern);
	delete pattern;
}
									/*}}}*/
}
}

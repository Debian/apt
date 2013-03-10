// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
    Collection of functor classes */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefilter.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/strutl.h>

#include <string>

#include <regex.h>
#include <fnmatch.h>

#include <apti18n.h>
									/*}}}*/
namespace APT {
namespace CacheFilter {
PackageNameMatchesRegEx::PackageNameMatchesRegEx(std::string const &Pattern) : d(NULL) {/*{{{*/
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

// CompleteArch to <kernel>-<cpu> tuple					/*{{{*/
//----------------------------------------------------------------------
/* The complete architecture, consisting of <kernel>-<cpu>. */
static std::string CompleteArch(std::string const &arch) {
	if (arch.find('-') != std::string::npos) {
		// ensure that only -any- is replaced and not something like company-
		std::string complete = std::string("-").append(arch).append("-");
		complete = SubstVar(complete, "-any-", "-*-");
		complete = complete.substr(1, complete.size()-2);
		return complete;
	}
	else if (arch == "any")			return "*-*";
	else					return "linux-" + arch;
}
									/*}}}*/
PackageArchitectureMatchesSpecification::PackageArchitectureMatchesSpecification(std::string const &pattern, bool const isPattern) :/*{{{*/
					literal(pattern), complete(CompleteArch(pattern)), isPattern(isPattern), d(NULL) {
}
									/*}}}*/
bool PackageArchitectureMatchesSpecification::operator() (char const * const &arch) {/*{{{*/
	if (strcmp(literal.c_str(), arch) == 0 ||
	    strcmp(complete.c_str(), arch) == 0)
		return true;
	std::string const pkgarch = CompleteArch(arch);
	if (isPattern == true)
		return fnmatch(complete.c_str(), pkgarch.c_str(), 0) == 0;
	return fnmatch(pkgarch.c_str(), complete.c_str(), 0) == 0;
}
									/*}}}*/
bool PackageArchitectureMatchesSpecification::operator() (pkgCache::PkgIterator const &Pkg) {/*{{{*/
	return (*this)(Pkg.Arch());
}
									/*}}}*/
bool PackageArchitectureMatchesSpecification::operator() (pkgCache::VerIterator const &Ver) {/*{{{*/
	return (*this)(Ver.ParentPkg());
}
									/*}}}*/
PackageArchitectureMatchesSpecification::~PackageArchitectureMatchesSpecification() {	/*{{{*/
}
									/*}}}*/

}
}

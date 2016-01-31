// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
    Collection of functor classes */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/macros.h>

#include <string>
#include <string.h>
#include <regex.h>
#include <fnmatch.h>

#include <apti18n.h>
									/*}}}*/
namespace APT {
namespace CacheFilter {
APT_CONST Matcher::~Matcher() {}
APT_CONST PackageMatcher::~PackageMatcher() {}

// Name matches RegEx							/*{{{*/
PackageNameMatchesRegEx::PackageNameMatchesRegEx(std::string const &Pattern) {
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
bool PackageNameMatchesRegEx::operator() (pkgCache::PkgIterator const &Pkg) {
	if (unlikely(pattern == NULL))
		return false;
	else
		return regexec(pattern, Pkg.Name(), 0, 0, 0) == 0;
}
bool PackageNameMatchesRegEx::operator() (pkgCache::GrpIterator const &Grp) {
	if (unlikely(pattern == NULL))
		return false;
	else
		return regexec(pattern, Grp.Name(), 0, 0, 0) == 0;
}
PackageNameMatchesRegEx::~PackageNameMatchesRegEx() {
	if (pattern == NULL)
		return;
	regfree(pattern);
	delete pattern;
}
									/*}}}*/
// Name matches Fnmatch							/*{{{*/
PackageNameMatchesFnmatch::PackageNameMatchesFnmatch(std::string const &Pattern) :
   Pattern(Pattern) {}
bool PackageNameMatchesFnmatch::operator() (pkgCache::PkgIterator const &Pkg) {
   return fnmatch(Pattern.c_str(), Pkg.Name(), FNM_CASEFOLD) == 0;
}
bool PackageNameMatchesFnmatch::operator() (pkgCache::GrpIterator const &Grp) {
   return fnmatch(Pattern.c_str(), Grp.Name(), FNM_CASEFOLD) == 0;
}
									/*}}}*/
// Architecture matches <libc>-<kernel>-<cpu> specification		/*{{{*/
//----------------------------------------------------------------------
/* The complete architecture, consisting of <libc>-<kernel>-<cpu>. */
static std::string CompleteArch(std::string const &arch, bool const isPattern) {
   auto const found = arch.find('-');
   if (found != std::string::npos)
   {
      // ensure that only -any- is replaced and not something like company-
      std::string complete = std::string("-").append(arch).append("-");
      size_t pos = 0;
      char const * const search = "-any-";
      auto const search_len = strlen(search) - 2;
      while((pos = complete.find(search, pos)) != std::string::npos) {
	 complete.replace(pos + 1, search_len, "*");
	 pos += 2;
      }
      complete = complete.substr(1, complete.size()-2);
      if (arch.find('-', found+1) != std::string::npos)
	 // <libc>-<kernel>-<cpu> format
	 return complete;
      // <kernel>-<cpu> format
      else if (isPattern)
	 return "*-" + complete;
      else
	 return "gnu-" + complete;
   }
   else if (arch == "any")
      return "*-*-*";
   else if (isPattern)
      return "*-linux-" + arch;
   else
      return "gnu-linux-" + arch;
}
PackageArchitectureMatchesSpecification::PackageArchitectureMatchesSpecification(std::string const &pattern, bool const pisPattern) :
					literal(pattern), complete(CompleteArch(pattern, pisPattern)), isPattern(pisPattern) {
}
bool PackageArchitectureMatchesSpecification::operator() (char const * const &arch) {
	if (strcmp(literal.c_str(), arch) == 0 ||
	    strcmp(complete.c_str(), arch) == 0)
		return true;
	std::string const pkgarch = CompleteArch(arch, !isPattern);
	if (isPattern == true)
		return fnmatch(complete.c_str(), pkgarch.c_str(), 0) == 0;
	return fnmatch(pkgarch.c_str(), complete.c_str(), 0) == 0;
}
bool PackageArchitectureMatchesSpecification::operator() (pkgCache::PkgIterator const &Pkg) {
	return (*this)(Pkg.Arch());
}
PackageArchitectureMatchesSpecification::~PackageArchitectureMatchesSpecification() {
}
									/*}}}*/
// Package is new install						/*{{{*/
PackageIsNewInstall::PackageIsNewInstall(pkgCacheFile * const Cache) : Cache(Cache) {}
APT_PURE bool PackageIsNewInstall::operator() (pkgCache::PkgIterator const &Pkg) {
   return (*Cache)[Pkg].NewInstall();
}
PackageIsNewInstall::~PackageIsNewInstall() {}
									/*}}}*/
// Generica like True, False, NOT, AND, OR				/*{{{*/
APT_CONST bool TrueMatcher::operator() (pkgCache::PkgIterator const &) { return true; }
APT_CONST bool TrueMatcher::operator() (pkgCache::GrpIterator const &) { return true; }
APT_CONST bool TrueMatcher::operator() (pkgCache::VerIterator const &) { return true; }

APT_CONST bool FalseMatcher::operator() (pkgCache::PkgIterator const &) { return false; }
APT_CONST bool FalseMatcher::operator() (pkgCache::GrpIterator const &) { return false; }
APT_CONST bool FalseMatcher::operator() (pkgCache::VerIterator const &) { return false; }

NOTMatcher::NOTMatcher(Matcher * const matcher) : matcher(matcher) {}
bool NOTMatcher::operator() (pkgCache::PkgIterator const &Pkg) { return ! (*matcher)(Pkg); }
bool NOTMatcher::operator() (pkgCache::GrpIterator const &Grp) { return ! (*matcher)(Grp); }
bool NOTMatcher::operator() (pkgCache::VerIterator const &Ver) { return ! (*matcher)(Ver); }
NOTMatcher::~NOTMatcher() { delete matcher; }

ANDMatcher::ANDMatcher() {}
ANDMatcher::ANDMatcher(Matcher * const matcher1) {
   AND(matcher1);
}
ANDMatcher::ANDMatcher(Matcher * const matcher1, Matcher * const matcher2) {
   AND(matcher1).AND(matcher2);
}
ANDMatcher::ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3) {
   AND(matcher1).AND(matcher2).AND(matcher3);
}
ANDMatcher::ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4) {
   AND(matcher1).AND(matcher2).AND(matcher3).AND(matcher4);
}
ANDMatcher::ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5) {
   AND(matcher1).AND(matcher2).AND(matcher3).AND(matcher4).AND(matcher5);
}
ANDMatcher& ANDMatcher::AND(Matcher * const matcher) { matchers.push_back(matcher); return *this; }
bool ANDMatcher::operator() (pkgCache::PkgIterator const &Pkg) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Pkg) == false)
	 return false;
   return true;
}
bool ANDMatcher::operator() (pkgCache::GrpIterator const &Grp) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Grp) == false)
	 return false;
   return true;
}
bool ANDMatcher::operator() (pkgCache::VerIterator const &Ver) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Ver) == false)
	 return false;
   return true;
}
ANDMatcher::~ANDMatcher() {
   for (std::vector<Matcher *>::iterator M = matchers.begin(); M != matchers.end(); ++M)
      delete *M;
}

ORMatcher::ORMatcher() {}
ORMatcher::ORMatcher(Matcher * const matcher1) {
   OR(matcher1);
}
ORMatcher::ORMatcher(Matcher * const matcher1, Matcher * const matcher2) {
   OR(matcher1).OR(matcher2);
}
ORMatcher::ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3) {
   OR(matcher1).OR(matcher2).OR(matcher3);
}
ORMatcher::ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4) {
   OR(matcher1).OR(matcher2).OR(matcher3).OR(matcher4);
}
ORMatcher::ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5) {
   OR(matcher1).OR(matcher2).OR(matcher3).OR(matcher4).OR(matcher5);
}
ORMatcher& ORMatcher::OR(Matcher * const matcher) { matchers.push_back(matcher); return *this; }
bool ORMatcher::operator() (pkgCache::PkgIterator const &Pkg) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Pkg) == true)
	 return true;
   return false;
}
bool ORMatcher::operator() (pkgCache::GrpIterator const &Grp) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Grp) == true)
	 return true;
   return false;
}
bool ORMatcher::operator() (pkgCache::VerIterator const &Ver) {
   for (std::vector<Matcher *>::const_iterator M = matchers.begin(); M != matchers.end(); ++M)
      if ((**M)(Ver) == true)
	 return true;
   return false;
}
ORMatcher::~ORMatcher() {
   for (std::vector<Matcher *>::iterator M = matchers.begin(); M != matchers.end(); ++M)
      delete *M;
}
									/*}}}*/

}
}

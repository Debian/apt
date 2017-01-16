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

#include <algorithm>
#include <string>
#include <unordered_map>
#include <string.h>
#include <regex.h>
#include <fnmatch.h>

#include <apti18n.h>
									/*}}}*/
namespace APT {

APT_HIDDEN std::unordered_map<std::string, std::vector<std::string>> ArchToTupleMap;

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
// Architecture matches <abi>-<libc>-<kernel>-<cpu> specification	/*{{{*/
//----------------------------------------------------------------------

static std::vector<std::string> ArchToTuple(std::string arch) {
   // Strip leading linux- from arch if present
   // dpkg says this may disappear in the future
   if (APT::String::Startswith(arch, std::string("linux-")))
      arch = arch.substr(6);

   auto it = ArchToTupleMap.find(arch);
   if (it != ArchToTupleMap.end())
   {
      std::vector<std::string> result = it->second;
      // Hack in support for triplets
      if (result.size() == 3)
	 result.emplace(result.begin(), "base");
      return result;
   } else
   {
      return {};
   }
}

static std::vector<std::string> PatternToTuple(std::string const &arch) {
   std::vector<std::string> tuple = VectorizeString(arch, '-');
   if (std::find(tuple.begin(), tuple.end(), std::string("any")) != tuple.end() ||
       std::find(arch.begin(), arch.end(), '*') != arch.end()) {
      while (tuple.size() < 4) {
	 tuple.emplace(tuple.begin(), "any");
      }
      return tuple;
   } else
      return ArchToTuple(arch);
}

/* The complete architecture, consisting of <abi>-<libc>-<kernel>-<cpu>. */
static std::string CompleteArch(std::string const &arch, bool const isPattern) {
   auto tuple = isPattern ? PatternToTuple(arch) : ArchToTuple(arch);

   // Bah, the commandline will try and pass us stuff like amd64- -- we need
   // that not to match an architecture, but the code below would turn it into
   // a valid tuple. Let's just use an invalid tuple here.
   if (APT::String::Endswith(arch, "-") || APT::String::Startswith(arch, "-"))
      return "invalid-invalid-invalid-invalid";

   if (tuple.empty()) {
      // Fallback for unknown architectures
      // Patterns never fail if they contain wildcards, so by this point, arch
      // has no wildcards.
      tuple = VectorizeString(arch, '-');
      switch (tuple.size()) {
	 case 1:
	    tuple.emplace(tuple.begin(), "linux");
	    /* fall through */
	 case 2:
	    tuple.emplace(tuple.begin(), "gnu");
	    /* fall through */
	 case 3:
	    tuple.emplace(tuple.begin(), "base");
	    /* fall through */
	    break;
      }
   }

   std::replace(tuple.begin(), tuple.end(), std::string("any"), std::string("*"));
   return APT::String::Join(tuple, "-");
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

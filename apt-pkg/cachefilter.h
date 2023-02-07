// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
   Collection of functor classes */
									/*}}}*/
#ifndef APT_CACHEFILTER_H
#define APT_CACHEFILTER_H
// Include Files							/*{{{*/
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/string_view.h>

#include <memory>
#include <string>
#include <vector>

#include <regex.h>

class pkgCacheFile;
									/*}}}*/
namespace APT {
namespace CacheFilter {

class APT_PUBLIC Matcher {
public:
   virtual bool operator() (pkgCache::PkgIterator const &/*Pkg*/) = 0;
   virtual bool operator() (pkgCache::GrpIterator const &/*Grp*/) = 0;
   virtual bool operator() (pkgCache::VerIterator const &/*Ver*/) = 0;
   virtual ~Matcher();
};

class APT_PUBLIC PackageMatcher : public Matcher {
public:
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE = 0;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE { return (*this)(Ver.ParentPkg()); }
   bool operator() (pkgCache::GrpIterator const &/*Grp*/) APT_OVERRIDE { return false; }
   ~PackageMatcher() APT_OVERRIDE;
};

// Generica like True, False, NOT, AND, OR				/*{{{*/
class APT_PUBLIC TrueMatcher : public Matcher {
public:
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE;
};

class APT_PUBLIC FalseMatcher : public Matcher {
public:
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE;
};

class APT_PUBLIC NOTMatcher : public Matcher {
   Matcher * const matcher;
public:
   explicit NOTMatcher(Matcher * const matcher);
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE;
   ~NOTMatcher() APT_OVERRIDE;
};

class APT_PUBLIC ANDMatcher : public Matcher {
   std::vector<Matcher *> matchers;
public:
   // 5 ought to be enough for everybody… c++11 variadic templates would be nice
   ANDMatcher();
   explicit ANDMatcher(Matcher * const matcher1);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5);
   ANDMatcher& AND(Matcher * const matcher);
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE;
   ~ANDMatcher() APT_OVERRIDE;
};
class APT_PUBLIC ORMatcher : public Matcher {
   std::vector<Matcher *> matchers;
public:
   // 5 ought to be enough for everybody… c++11 variadic templates would be nice
   ORMatcher();
   explicit ORMatcher(Matcher * const matcher1);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5);
   ORMatcher& OR(Matcher * const matcher);
   bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
   bool operator() (pkgCache::VerIterator const &Ver) APT_OVERRIDE;
   ~ORMatcher() APT_OVERRIDE;
};
									/*}}}*/
class APT_PUBLIC PackageNameMatchesRegEx : public PackageMatcher {			/*{{{*/
	regex_t* pattern;
public:
	explicit PackageNameMatchesRegEx(std::string const &Pattern);
	bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
	~PackageNameMatchesRegEx() APT_OVERRIDE;
};
									/*}}}*/
class APT_PUBLIC PackageNameMatchesFnmatch : public PackageMatcher {		/*{{{*/
	const std::string Pattern;
public:
	explicit PackageNameMatchesFnmatch(std::string const &Pattern);
	bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	bool operator() (pkgCache::GrpIterator const &Grp) APT_OVERRIDE;
	~PackageNameMatchesFnmatch() APT_OVERRIDE = default;
};
									/*}}}*/
class APT_PUBLIC PackageArchitectureMatchesSpecification : public PackageMatcher {	/*{{{*/
/** \class PackageArchitectureMatchesSpecification
   \brief matching against architecture specification strings

   The strings are of the format <libc>-<kernel>-<cpu> where either component,
   or the whole string, can be the wildcard "any" as defined in
   debian-policy §11.1 "Architecture specification strings".

   Examples: i386, mipsel, musl-linux-amd64, linux-any, any-amd64, any */
	std::string literal;
	std::string complete;
	bool isPattern;
public:
	/** \brief matching against architecture specification strings
	 *
	 * @param pattern is the architecture specification string
	 * @param isPattern defines if the given \b pattern is a
	 *        architecture specification pattern to match others against
	 *        or if it is the fixed string and matched against patterns
	 */
	PackageArchitectureMatchesSpecification(std::string const &pattern, bool const isPattern = true);
	bool operator() (char const * const &arch);
	using PackageMatcher::operator();
	bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	~PackageArchitectureMatchesSpecification() APT_OVERRIDE;
};
									/*}}}*/
class APT_PUBLIC PackageIsNewInstall : public PackageMatcher {			/*{{{*/
	pkgCacheFile * const Cache;
public:
	explicit PackageIsNewInstall(pkgCacheFile * const Cache);
	using PackageMatcher::operator();
	bool operator() (pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	~PackageIsNewInstall() APT_OVERRIDE;
};
									/*}}}*/

/// \brief Parse a pattern, return nullptr or pattern
APT_PUBLIC std::unique_ptr<APT::CacheFilter::Matcher> ParsePattern(APT::StringView pattern, pkgCacheFile *file);
}
}
#endif

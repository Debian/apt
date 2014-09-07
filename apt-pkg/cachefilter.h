// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
   Collection of functor classes */
									/*}}}*/
#ifndef APT_CACHEFILTER_H
#define APT_CACHEFILTER_H
// Include Files							/*{{{*/
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <string>
#include <vector>

#include <regex.h>

class pkgCacheFile;
									/*}}}*/
namespace APT {
namespace CacheFilter {

class Matcher {
public:
   virtual bool operator() (pkgCache::PkgIterator const &/*Pkg*/) = 0;
   virtual bool operator() (pkgCache::GrpIterator const &/*Grp*/) = 0;
   virtual bool operator() (pkgCache::VerIterator const &/*Ver*/) = 0;
   virtual ~Matcher();
};

class PackageMatcher : public Matcher {
public:
   virtual bool operator() (pkgCache::PkgIterator const &Pkg) = 0;
   virtual bool operator() (pkgCache::VerIterator const &Ver) { return (*this)(Ver.ParentPkg()); }
   virtual bool operator() (pkgCache::GrpIterator const &/*Grp*/) { return false; }
   virtual ~PackageMatcher();
};

// Generica like True, False, NOT, AND, OR				/*{{{*/
class TrueMatcher : public Matcher {
public:
   virtual bool operator() (pkgCache::PkgIterator const &Pkg);
   virtual bool operator() (pkgCache::GrpIterator const &Grp);
   virtual bool operator() (pkgCache::VerIterator const &Ver);
};

class FalseMatcher : public Matcher {
public:
   virtual bool operator() (pkgCache::PkgIterator const &Pkg);
   virtual bool operator() (pkgCache::GrpIterator const &Grp);
   virtual bool operator() (pkgCache::VerIterator const &Ver);
};

class NOTMatcher : public Matcher {
   Matcher * const matcher;
public:
   NOTMatcher(Matcher * const matcher);
   virtual bool operator() (pkgCache::PkgIterator const &Pkg);
   virtual bool operator() (pkgCache::GrpIterator const &Grp);
   virtual bool operator() (pkgCache::VerIterator const &Ver);
   virtual ~NOTMatcher();
};

class ANDMatcher : public Matcher {
   std::vector<Matcher *> matchers;
public:
   // 5 ought to be enough for everybody… c++11 variadic templates would be nice
   ANDMatcher();
   ANDMatcher(Matcher * const matcher1);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4);
   ANDMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5);
   ANDMatcher& AND(Matcher * const matcher);
   virtual bool operator() (pkgCache::PkgIterator const &Pkg);
   virtual bool operator() (pkgCache::GrpIterator const &Grp);
   virtual bool operator() (pkgCache::VerIterator const &Ver);
   virtual ~ANDMatcher();
};
class ORMatcher : public Matcher {
   std::vector<Matcher *> matchers;
public:
   // 5 ought to be enough for everybody… c++11 variadic templates would be nice
   ORMatcher();
   ORMatcher(Matcher * const matcher1);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4);
   ORMatcher(Matcher * const matcher1, Matcher * const matcher2, Matcher * const matcher3, Matcher * const matcher4, Matcher * const matcher5);
   ORMatcher& OR(Matcher * const matcher);
   virtual bool operator() (pkgCache::PkgIterator const &Pkg);
   virtual bool operator() (pkgCache::GrpIterator const &Grp);
   virtual bool operator() (pkgCache::VerIterator const &Ver);
   virtual ~ORMatcher();
};
									/*}}}*/
class PackageNameMatchesRegEx : public PackageMatcher {			/*{{{*/
	regex_t* pattern;
public:
	PackageNameMatchesRegEx(std::string const &Pattern);
	virtual bool operator() (pkgCache::PkgIterator const &Pkg);
	virtual bool operator() (pkgCache::GrpIterator const &Grp);
	virtual ~PackageNameMatchesRegEx();
};
									/*}}}*/
class PackageNameMatchesFnmatch : public PackageMatcher {		/*{{{*/
	const std::string Pattern;
public:
	PackageNameMatchesFnmatch(std::string const &Pattern);
	virtual bool operator() (pkgCache::PkgIterator const &Pkg);
	virtual bool operator() (pkgCache::GrpIterator const &Grp);
	virtual ~PackageNameMatchesFnmatch() {};
};
									/*}}}*/
class PackageArchitectureMatchesSpecification : public PackageMatcher {	/*{{{*/
/** \class PackageArchitectureMatchesSpecification
   \brief matching against architecture specification strings

   The strings are of the format <kernel>-<cpu> where either component,
   or the whole string, can be the wildcard "any" as defined in
   debian-policy §11.1 "Architecture specification strings".

   Examples: i386, mipsel, linux-any, any-amd64, any */
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
	virtual bool operator() (pkgCache::PkgIterator const &Pkg);
	virtual ~PackageArchitectureMatchesSpecification();
};
									/*}}}*/
class PackageIsNewInstall : public PackageMatcher {			/*{{{*/
	pkgCacheFile * const Cache;
public:
	PackageIsNewInstall(pkgCacheFile * const Cache);
	virtual bool operator() (pkgCache::PkgIterator const &Pkg);
	virtual ~PackageIsNewInstall();
};
									/*}}}*/

}
}
#endif

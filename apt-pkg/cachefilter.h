// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cachefilter.h
   Collection of functor classes */
									/*}}}*/
#ifndef APT_CACHEFILTER_H
#define APT_CACHEFILTER_H
// Include Files							/*{{{*/
#include <apt-pkg/pkgcache.h>

#include <string>

#include <regex.h>
									/*}}}*/
namespace APT {
namespace CacheFilter {
// PackageNameMatchesRegEx						/*{{{*/
class PackageNameMatchesRegEx {
         /** \brief dpointer placeholder (for later in case we need it) */
         void *d;
	regex_t* pattern;
public:
	PackageNameMatchesRegEx(std::string const &Pattern);
	bool operator() (pkgCache::PkgIterator const &Pkg);
	bool operator() (pkgCache::GrpIterator const &Grp);
	~PackageNameMatchesRegEx();
};
									/*}}}*/
// PackageArchitectureMatchesSpecification				/*{{{*/
/** \class PackageArchitectureMatchesSpecification
   \brief matching against architecture specification strings

   The strings are of the format <kernel>-<cpu> where either component,
   or the whole string, can be the wildcard "any" as defined in
   debian-policy §11.1 "Architecture specification strings".

   Examples: i386, mipsel, linux-any, any-amd64, any */
class PackageArchitectureMatchesSpecification {
	std::string literal;
	std::string complete;
	bool isPattern;
	/** \brief dpointer placeholder (for later in case we need it) */
	void *d;
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
	bool operator() (pkgCache::PkgIterator const &Pkg);
	bool operator() (pkgCache::VerIterator const &Ver);
	~PackageArchitectureMatchesSpecification();
};
									/*}}}*/
}
}
#endif

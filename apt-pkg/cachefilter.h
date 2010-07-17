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
	regex_t* pattern;
public:
	PackageNameMatchesRegEx(std::string const &Pattern);
	bool operator() (pkgCache::PkgIterator const &Pkg);
	bool operator() (pkgCache::GrpIterator const &Grp);
	~PackageNameMatchesRegEx();
};
									/*}}}*/
}
}
#endif

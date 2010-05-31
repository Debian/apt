// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \class APT::PackageSet

    Simple wrapper around a std::set to provide a similar interface to
    a set of packages as to the complete set of all packages in the
    pkgCache.
*/
									/*}}}*/
#ifndef APT_PACKAGESET_H
#define APT_PACKAGESET_H
// Include Files							/*{{{*/
#include <iostream>
#include <fstream>
#include <set>
#include <string>

#include <apt-pkg/pkgcache.h>
									/*}}}*/
namespace APT {
class PackageSet : public std::set<pkgCache::PkgIterator> {		/*{{{*/
public:									/*{{{*/
	/** \brief smell like a pkgCache::PkgIterator */
	class const_iterator : public std::set<pkgCache::PkgIterator>::const_iterator {
	public:
		const_iterator(std::set<pkgCache::PkgIterator>::const_iterator x) :
			 std::set<pkgCache::PkgIterator>::const_iterator(x) {}

		operator pkgCache::PkgIterator(void) { return **this; }

		inline const char *Name() const {return (**this).Name(); }
		inline std::string FullName(bool const &Pretty) const { return (**this).FullName(Pretty); }
		inline std::string FullName() const { return (**this).FullName(); }
		inline const char *Section() const {return (**this).Section(); }
		inline bool Purge() const {return (**this).Purge(); }
		inline const char *Arch() const {return (**this).Arch(); }
		inline pkgCache::GrpIterator Group() const { return (**this).Group(); }
		inline pkgCache::VerIterator VersionList() const { return (**this).VersionList(); }
		inline pkgCache::VerIterator CurrentVer() const { return (**this).CurrentVer(); }
		inline pkgCache::DepIterator RevDependsList() const { return (**this).RevDependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return (**this).ProvidesList(); }
		inline pkgCache::PkgIterator::OkState State() const { return (**this).State(); }
		inline const char *CandVersion() const { return (**this).CandVersion(); }
		inline const char *CurVersion() const { return (**this).CurVersion(); }
		inline pkgCache *Cache() const { return (**this).Cache(); };
		inline unsigned long Index() const {return (**this).Index();};
		// we have only valid iterators here
		inline bool end() const { return false; };

		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, (*i)); }

		inline pkgCache::Package const * operator->() const {
			return &***this;
		};
	};
	// 103. set::iterator is required to be modifiable, but this allows modification of keys
	typedef typename APT::PackageSet::const_iterator iterator;

	/** \brief returns all packages in the cache whose name matchs a given pattern

	    A simple helper responsible for executing a regular expression on all
	    package names in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given package.
	    \param Cache the packages are in
	    \param pattern regular expression for package names
	    \param out stream to print the notice to */
	static APT::PackageSet FromRegEx(pkgCache &Cache, std::string pattern, std::ostream &out);
	static APT::PackageSet FromRegEx(pkgCache &Cache, std::string const &pattern) {
		std::ostream out (std::ofstream("/dev/null").rdbuf());
		return APT::PackageSet::FromRegEx(Cache, pattern, out);
	}

	/** \brief returns all packages specified on the commandline

	    Get all package names from the commandline and executes regex's if needed.
	    No special package command is supported, just plain names.
	    \param Cache the packages are in
	    \param cmdline Command line the package names should be extracted from
	    \param out stream to print various notices to */
	static APT::PackageSet FromCommandLine(pkgCache &Cache, const char **cmdline, std::ostream &out);
	static APT::PackageSet FromCommandLine(pkgCache &Cache, const char **cmdline) {
		std::ostream out (std::ofstream("/dev/null").rdbuf());
		return APT::PackageSet::FromCommandLine(Cache, cmdline, out);
	}



									/*}}}*/
};
									/*}}}*/
}
#endif

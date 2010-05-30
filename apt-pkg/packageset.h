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

		inline const char *Name() const {return (*this)->Name(); }
		inline std::string FullName(bool const &Pretty) const { return (*this)->FullName(Pretty); }
		inline std::string FullName() const { return (*this)->FullName(); }
		inline const char *Section() const {return (*this)->Section(); }
		inline bool Purge() const {return (*this)->Purge(); }
		inline const char *Arch() const {return (*this)->Arch(); }
		inline pkgCache::GrpIterator Group() const { return (*this)->Group(); }
		inline pkgCache::VerIterator VersionList() const { return (*this)->VersionList(); }
		inline pkgCache::VerIterator CurrentVer() const { return (*this)->CurrentVer(); }
		inline pkgCache::DepIterator RevDependsList() const { return (*this)->RevDependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return (*this)->ProvidesList(); }
		inline pkgCache::PkgIterator::OkState State() const { return (*this)->State(); }
		inline const char *CandVersion() const { return (*this)->CandVersion(); }
		inline const char *CurVersion() const { return (*this)->CurVersion(); }
		inline pkgCache *Cache() {return (*this)->Cache();};

		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, (*i)); }

		inline pkgCache::PkgIterator const * operator->() const {
			return &**this;
		};
	};
	// 103. set::iterator is required to be modifiable, but this allows modification of keys
	typedef typename APT::PackageSet::const_iterator iterator;
									/*}}}*/
};
									/*}}}*/
}
#endif

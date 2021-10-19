#ifndef APT_PRIVATE_CACHESET_H
#define APT_PRIVATE_CACHESET_H

#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>

#include <apt-private/private-output.h>

#include <list>
#include <set>
#include <string>
#include <vector>

class OpProgress;

class VerIteratorWithCaching
{
   const pkgCache::VerIterator iter;
   const pkgCache::DescFile * descFile;
public:

   // cppcheck-suppress noExplicitConstructor
   VerIteratorWithCaching(const pkgCache::VerIterator& iter) :
      iter(iter),
      descFile(iter->DescriptionList != 0
	 ? (const pkgCache::DescFile *) iter.TranslatedDescription().FileList()
	 : nullptr)
   {}
   const pkgCache::DescFile * CachedDescFile() const { return descFile; }
   operator pkgCache::VerIterator() const { return iter; }
   map_id_t ID() const { return iter->ID; }
};

struct VersionSortDescriptionLocality					/*{{{*/
{
   bool operator () (const VerIteratorWithCaching &v_lhs,
	 const VerIteratorWithCaching &v_rhs) const
   {
      pkgCache::DescFile const *A = v_lhs.CachedDescFile();
      pkgCache::DescFile const *B = v_rhs.CachedDescFile();

      if (A == nullptr)
      {
	 if (B == nullptr)
	    return v_lhs.ID() < v_rhs.ID();
	 return true;
      }
      else if (B == nullptr)
	 return false;

      if (A->File == B->File)
      {
	 if (A->Offset == B->Offset)
	    return v_lhs.ID() < v_rhs.ID();
	 return A->Offset < B->Offset;
      }

      return A->File < B->File;
   }
};
									/*}}}*/
// sorted by locality which makes iterating much faster
typedef APT::VersionContainer<
   std::set<VerIteratorWithCaching,
            VersionSortDescriptionLocality> > LocalitySortedVersionSet;

class Matcher {
public:
    virtual bool operator () (const pkgCache::PkgIterator &/*P*/) {
        return true;}
};

// FIXME: add default argument for OpProgress (or overloaded function)
bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,
                                    APT::VersionContainerInterface * const vci,
                                    Matcher &matcher,
                                    OpProgress * const progress);
bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,
                                    APT::VersionContainerInterface * const vci,
                                    OpProgress * const progress);


// CacheSetHelper saving virtual packages				/*{{{*/
class CacheSetHelperVirtuals: public APT::CacheSetHelper {
public:
   APT::PackageSet virtualPkgs;

   virtual pkgCache::VerIterator canNotGetVersion(enum CacheSetHelper::VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   virtual void canNotFindVersion(enum CacheSetHelper::VerSelector const select, APT::VersionContainerInterface * vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str) APT_OVERRIDE;

   CacheSetHelperVirtuals(bool const ShowErrors = true, GlobalError::MsgType const &ErrorType = GlobalError::NOTICE);
};
									/*}}}*/

// CacheSetHelperAPTGet - responsible for message telling from the CacheSets/*{{{*/
class CacheSetHelperAPTGet : public APT::CacheSetHelper {
	/** \brief stream message should be printed to */
	std::ostream &out;
	/** \brief were things like Task or RegEx used to select packages? */
	bool explicitlyNamed;

	APT::PackageSet virtualPkgs;
public:
	std::list<std::pair<pkgCache::VerIterator, std::string> > selectedByRelease;
	std::set<std::string> notFound;

	explicit CacheSetHelperAPTGet(std::ostream &out);

	virtual void showPackageSelection(pkgCache::PkgIterator const &Pkg, enum PkgSelector const select, std::string const &pattern) APT_OVERRIDE;
	void showTaskSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern);
	void showFnmatchSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern);
	void showRegExSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern);
	void showVersionSelection(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const &Ver, enum VerSelector const select, std::string const &pattern) APT_OVERRIDE;
	bool showVirtualPackageErrors(pkgCacheFile &Cache);

	pkgCache::VerIterator canNotGetVersion(enum VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	void canNotFindVersion(enum VerSelector const select, APT::VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotFindVersionNumber(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg, std::string const &verstr);
	pkgCache::VerIterator canNotFindVersionRelease(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg, std::string const &verstr);
	virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str) APT_OVERRIDE;

	APT::VersionSet tryVirtualPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg,
						CacheSetHelper::VerSelector const select);

	inline bool allPkgNamedExplicitly() const { return explicitlyNamed; }
};
									/*}}}*/

#endif

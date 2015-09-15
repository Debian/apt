#ifndef APT_PRIVATE_CACHESET_H
#define APT_PRIVATE_CACHESET_H

#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>

#include <apt-private/private-output.h>

#include <vector>
#include <list>
#include <set>
#include <string>

#include <apti18n.h>

class OpProgress;

struct APT_PUBLIC VersionSortDescriptionLocality			/*{{{*/
{
   bool operator () (const pkgCache::VerIterator &v_lhs,
	 const pkgCache::VerIterator &v_rhs)
   {
      pkgCache::DescFile const *A = nullptr;
      pkgCache::DescFile const *B = nullptr;
      if (v_lhs->DescriptionList != 0)
	 A = v_lhs.TranslatedDescription().FileList();
      if (v_rhs->DescriptionList != 0)
	 B = v_rhs.TranslatedDescription().FileList();

      if (A == nullptr && B == nullptr)
	 return false;

      if (A == nullptr)
	 return true;

      if (B == nullptr)
	 return false;

      if (A->File == B->File)
	 return A->Offset < B->Offset;

      return A->File < B->File;
   }
};
									/*}}}*/
// sorted by locality which makes iterating much faster
typedef APT::VersionContainer<
   std::set<pkgCache::VerIterator,
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
class APT_PUBLIC CacheSetHelperVirtuals: public APT::CacheSetHelper {
public:
   APT::PackageSet virtualPkgs;

   virtual pkgCache::VerIterator canNotGetVersion(enum CacheSetHelper::VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   virtual void canNotFindVersion(enum CacheSetHelper::VerSelector const select, APT::VersionContainerInterface * vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
   virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str) APT_OVERRIDE;

   CacheSetHelperVirtuals(bool const ShowErrors = true, GlobalError::MsgType const &ErrorType = GlobalError::NOTICE);
};
									/*}}}*/

// CacheSetHelperAPTGet - responsible for message telling from the CacheSets/*{{{*/
class APT_PUBLIC CacheSetHelperAPTGet : public APT::CacheSetHelper {
	/** \brief stream message should be printed to */
	std::ostream &out;
	/** \brief were things like Task or RegEx used to select packages? */
	bool explicitlyNamed;

	APT::PackageSet virtualPkgs;

public:
	std::list<std::pair<pkgCache::VerIterator, std::string> > selectedByRelease;

	CacheSetHelperAPTGet(std::ostream &out);

	virtual void showTaskSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) APT_OVERRIDE;
        virtual void showFnmatchSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) APT_OVERRIDE;
	virtual void showRegExSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) APT_OVERRIDE;
	virtual void showSelectedVersion(pkgCache::PkgIterator const &/*Pkg*/, pkgCache::VerIterator const Ver,
				 std::string const &ver, bool const /*verIsRel*/) APT_OVERRIDE;
	bool showVirtualPackageErrors(pkgCacheFile &Cache);

	virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) APT_OVERRIDE;
	virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str) APT_OVERRIDE;

	APT::VersionSet tryVirtualPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg,
						CacheSetHelper::VerSelector const select);

	inline bool allPkgNamedExplicitly() const { return explicitlyNamed; }
};
									/*}}}*/

#endif

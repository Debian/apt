#ifndef APT_PRIVATE_CACHEFILE_H
#define APT_PRIVATE_CACHEFILE_H

#include <apt-pkg/cachefile.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cacheset.h>

#include <apti18n.h>

// FIXME: we need to find a way to export this
class APT_PUBLIC SourceList : public pkgSourceList
{
 public:
   // Add custom metaIndex (e.g. local files)
   void AddMetaIndex(metaIndex *mi) {
      SrcList.push_back(mi);
   }

};

// class CacheFile - Cover class for some dependency cache functions	/*{{{*/
class APT_PUBLIC CacheFile : public pkgCacheFile
{
   public:
   std::vector<map_pointer_t> UniverseList;

   bool CheckDeps(bool AllowBroken = false);
   bool BuildCaches(bool WithLock = true)
   {
      OpTextProgress Prog(*_config);
      if (pkgCacheFile::BuildCaches(&Prog,WithLock) == false)
	 return false;
      return true;
   }
   // FIXME: this can go once the "libapt-pkg" pkgSourceList has a way
   //        to add custom metaIndexes (or custom local files or so)
   bool BuildSourceList(OpProgress */*Progress*/ = NULL) {
      if (SrcList != NULL)
         return true;
      SrcList = new SourceList();
      if (SrcList->ReadMainList() == false)
         return _error->Error(_("The list of sources could not be read."));
      return true;
   }
   bool Open(bool WithLock = true)
   {
      OpTextProgress Prog(*_config);
      return pkgCacheFile::Open(&Prog,WithLock);
   };
   bool OpenForInstall()
   {
      if (_config->FindB("APT::Get::Print-URIs") == true)
	 return Open(false);
      else
	 return Open(true);
   }
};
									/*}}}*/

class SortedPackageUniverse : public APT::PackageUniverse
{
   std::vector<map_pointer_t> &List;
   void LazyInit() const;

public:
   SortedPackageUniverse(CacheFile &Cache);

   class const_iterator : public APT::Container_iterator_base<APT::PackageContainerInterface, SortedPackageUniverse, SortedPackageUniverse::const_iterator, std::vector<map_pointer_t>::const_iterator, pkgCache::PkgIterator>
   {
      pkgCache * const Cache;
      public:
	 inline pkgCache::PkgIterator getType(void) const
	 {
	    if (*_iter == 0) return pkgCache::PkgIterator(*Cache);
	    return pkgCache::PkgIterator(*Cache, Cache->PkgP + *_iter);
	 }
	 explicit const_iterator(pkgCache * const Owner, std::vector<map_pointer_t>::const_iterator i):
	    Container_iterator_base<APT::PackageContainerInterface, SortedPackageUniverse, SortedPackageUniverse::const_iterator, std::vector<map_pointer_t>::const_iterator, pkgCache::PkgIterator>(i), Cache(Owner) {}

   };
   typedef const_iterator iterator;

   const_iterator begin() const { LazyInit(); return const_iterator(data(), List.begin()); }
   const_iterator end() const { LazyInit(); return const_iterator(data(), List.end()); }
   const_iterator cbegin() const { LazyInit(); return const_iterator(data(), List.begin()); }
   const_iterator cend() const { LazyInit(); return const_iterator(data(), List.end()); }
   iterator begin() { LazyInit(); return iterator(data(), List.begin()); }
   iterator end() { LazyInit(); return iterator(data(), List.end()); }
};

#endif

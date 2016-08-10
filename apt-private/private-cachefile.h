#ifndef APT_PRIVATE_CACHEFILE_H
#define APT_PRIVATE_CACHEFILE_H

#include <apt-pkg/cachefile.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cacheset.h>

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
   explicit SortedPackageUniverse(CacheFile &Cache);

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

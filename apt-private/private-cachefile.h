#ifndef APT_PRIVATE_CACHEFILE_H
#define APT_PRIVATE_CACHEFILE_H

#include <apt-pkg/cachefile.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/sourcelist.h>
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
// ---------------------------------------------------------------------
/* */
class APT_PUBLIC CacheFile : public pkgCacheFile
{
   static pkgCache *SortCache;
   APT_HIDDEN static int NameComp(const void *a,const void *b) APT_PURE;

   public:
   pkgCache::Package **List;
   
   void Sort();
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
      if (pkgCacheFile::Open(&Prog,WithLock) == false)
	 return false;
      Sort();
      
      return true;
   };
   bool OpenForInstall()
   {
      if (_config->FindB("APT::Get::Print-URIs") == true)
	 return Open(false);
      else
	 return Open(true);
   }
   CacheFile() : List(0) {};
   ~CacheFile() {
      delete[] List;
   }
};
									/*}}}*/

#endif

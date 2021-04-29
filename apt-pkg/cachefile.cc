// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   CacheFile - Simple wrapper class for opening, generating and whatnot
   
   This class implements a simple 2 line mechanism to open various sorts
   of caches. It can operate as root, as not root, show progress and so on,
   it transparently handles everything necessary.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

struct pkgCacheFile::Private
{
   bool WithLock = false;
   bool InhibitActionGroups = false;
};

// CacheFile::CacheFile - Constructor					/*{{{*/
pkgCacheFile::pkgCacheFile() : d(new Private()), ExternOwner(false), Map(NULL), Cache(NULL),
				DCache(NULL), SrcList(NULL), Policy(NULL)
{
}
pkgCacheFile::pkgCacheFile(pkgDepCache * const Owner) : d(new Private()), ExternOwner(true),
   Map(&Owner->GetCache().GetMap()), Cache(&Owner->GetCache()),
   DCache(Owner), SrcList(NULL), Policy(NULL)
{
}
									/*}}}*/
// CacheFile::~CacheFile - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::~pkgCacheFile()
{
   if (ExternOwner == false)
   {
      delete DCache;
      delete Cache;
      delete Map;
   }
   delete Policy;
   delete SrcList;
   if (d->WithLock == true)
      _system->UnLock(true);

   delete d;
}
									/*}}}*/
// CacheFile::BuildCaches - Open and build the cache files		/*{{{*/
class APT_HIDDEN ScopedErrorMerge {
public:
   ScopedErrorMerge() { _error->PushToStack(); }
   ~ScopedErrorMerge() { _error->MergeWithStack(); }
};

bool pkgCacheFile::BuildCaches(OpProgress *Progress, bool WithLock)
{
   std::unique_ptr<pkgCache> Cache;
   std::unique_ptr<MMap> Map;

   if (this->Cache != NULL)
      return true;

   ScopedErrorMerge sem;
   if (_config->FindB("pkgCacheFile::Generate", true) == false)
   {
      FileFd file(_config->FindFile("Dir::Cache::pkgcache"), FileFd::ReadOnly);
      if (file.IsOpen() == false || file.Failed())
	 return false;
      Map.reset(new MMap(file, MMap::Public|MMap::ReadOnly));
      if (unlikely(Map->validData() == false))
	 return false;
      Cache.reset(new pkgCache(Map.get()));
      if (_error->PendingError() == true)
	 return false;

      this->Cache = Cache.release();
      this->Map = Map.release();
      return true;
   }

   if (WithLock == true)
   {
      if (_system->Lock(Progress) == false)
	 return false;
      d->WithLock = true;
   }

   if (_error->PendingError() == true)
      return false;

   if (BuildSourceList(Progress) == false)
      return false;

   // Read the caches
   MMap *TmpMap = nullptr;
   pkgCache *TmpCache = nullptr;
   bool Res = pkgCacheGenerator::MakeStatusCache(*SrcList,Progress,&TmpMap, &TmpCache, true);
   Map.reset(TmpMap);
   Cache.reset(TmpCache);
   if (Progress != NULL)
      Progress->Done();
   if (Res == false)
      return _error->Error(_("The package lists or status file could not be parsed or opened."));

   /* This sux, remove it someday */
   if (_error->PendingError() == true)
      _error->Warning(_("You may want to run apt-get update to correct these problems"));

   if (Cache == nullptr)
      Cache.reset(new pkgCache(Map.get()));
   if (_error->PendingError() == true)
      return false;
   this->Map = Map.release();
   this->Cache = Cache.release();

   return true;
}
									/*}}}*/
// CacheFile::BuildSourceList - Open and build all relevant sources.list/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildSourceList(OpProgress * /*Progress*/)
{
   std::unique_ptr<pkgSourceList> SrcList;
   if (this->SrcList != NULL)
      return true;

   SrcList.reset(new pkgSourceList());
   if (SrcList->ReadMainList() == false)
      return _error->Error(_("The list of sources could not be read."));
   this->SrcList = SrcList.release();
   return true;
}
									/*}}}*/
// CacheFile::BuildPolicy - Open and build all relevant preferences	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildPolicy(OpProgress * /*Progress*/)
{
   std::unique_ptr<pkgPolicy> Policy;
   if (this->Policy != NULL)
      return true;

   Policy.reset(new pkgPolicy(Cache));
   if (_error->PendingError() == true)
      return false;

   ReadPinFile(*Policy);
   ReadPinDir(*Policy);

   this->Policy = Policy.release();
   return _error->PendingError() == false;
}
									/*}}}*/
// CacheFile::BuildDepCache - Open and build the dependency cache	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildDepCache(OpProgress *Progress)
{
   if (BuildCaches(Progress, false) == false)
      return false;

   std::unique_ptr<pkgDepCache> DCache;
   if (this->DCache != NULL)
      return true;

   if (BuildPolicy(Progress) == false)
      return false;

   DCache.reset(new pkgDepCache(Cache,Policy));
   if (_error->PendingError() == true)
      return false;
   if (d->InhibitActionGroups)
      DCache->IncreaseActionGroupLevel();
   if (DCache->Init(Progress) == false)
      return false;

   this->DCache = DCache.release();
   return true;
}
									/*}}}*/
// CacheFile::Open - Open the cache files, creating if necessary	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::Open(OpProgress *Progress, bool WithLock)
{
   if (BuildCaches(Progress,WithLock) == false)
      return false;

   if (BuildPolicy(Progress) == false)
      return false;

   if (BuildDepCache(Progress) == false)
      return false;

   if (Progress != NULL)
      Progress->Done();
   if (_error->PendingError() == true)
      return false;
   
   return true;
}
									/*}}}*/
bool pkgCacheFile::AddIndexFile(pkgIndexFile * const File)		/*{{{*/
{
   if (SrcList == NULL)
      if (BuildSourceList() == false)
	 return false;
   SrcList->AddVolatileFile(File);

   if (Cache == nullptr || File->HasPackages() == false || File->Exists() == false)
      return true;

   if (File->FindInCache(*Cache).end() == false)
      return _error->Warning("Duplicate sources.list entry %s",
	    File->Describe().c_str());

   if (ExternOwner == false)
   {
      delete DCache;
      delete Cache;
   }
   delete Policy;
   DCache = NULL;
   Policy = NULL;
   Cache = NULL;

   if (ExternOwner == false)
   {
      // a dynamic mmap means that we have build at least parts of the cache
      // in memory – which we might or might not have written to disk.
      // Throwing away would therefore be a very costly operation we want to avoid
      DynamicMMap * dynmmap = dynamic_cast<DynamicMMap*>(Map);
      if (dynmmap != nullptr)
      {
	 {
	    pkgCacheGenerator Gen(dynmmap, nullptr);
	    if (Gen.Start() == false || File->Merge(Gen, nullptr) == false)
	       return false;
	 }
	 Cache = new pkgCache(Map);
	 if (_error->PendingError() == true) {
	    delete Cache;
	    Cache = nullptr;
	    return false;
	 }
	 return true;
      }
      else
      {
	 delete Map;
	 Map = NULL;
      }
   }
   else
   {
      ExternOwner = false;
      Map = NULL;
   }
   _system->UnLock(true);
   return true;
}
									/*}}}*/
// CacheFile::RemoveCaches - remove all cache files from disk		/*{{{*/
// ---------------------------------------------------------------------
/* */
static void SetCacheStartBeforeRemovingCache(std::string const &cache)
{
   if (cache.empty())
      return;
   auto const CacheStart = _config->FindI("APT::Cache-Start", 0);
   constexpr auto CacheStartDefault = 24 * 1024 * 1024;
   struct stat Buf;
   if (stat(cache.c_str(), &Buf) == 0 && (Buf.st_mode & S_IFREG) != 0)
   {
      RemoveFile("RemoveCaches", cache);
      if (CacheStart == 0 && std::numeric_limits<decltype(CacheStart)>::max() >= Buf.st_size && Buf.st_size > CacheStartDefault)
	 _config->Set("APT::Cache-Start", Buf.st_size);
   }
}
void pkgCacheFile::RemoveCaches()
{
   std::string const pkgcache = _config->FindFile("Dir::cache::pkgcache");
   SetCacheStartBeforeRemovingCache(pkgcache);
   std::string const srcpkgcache = _config->FindFile("Dir::cache::srcpkgcache");
   SetCacheStartBeforeRemovingCache(srcpkgcache);

   if (pkgcache.empty() == false)
   {
      std::string cachedir = flNotFile(pkgcache);
      std::string cachefile = flNotDir(pkgcache);
      if (cachedir.empty() != true && cachefile.empty() != true && DirectoryExists(cachedir) == true)
      {
	 cachefile.append(".");
	 std::vector<std::string> caches = GetListOfFilesInDir(cachedir, false);
	 for (std::vector<std::string>::const_iterator file = caches.begin(); file != caches.end(); ++file)
	 {
	    std::string nuke = flNotDir(*file);
	    if (strncmp(cachefile.c_str(), nuke.c_str(), cachefile.length()) != 0)
	       continue;
	    RemoveFile("RemoveCaches", *file);
	 }
      }
   }

   if (srcpkgcache.empty() == true)
      return;

   std::string cachedir = flNotFile(srcpkgcache);
   std::string cachefile = flNotDir(srcpkgcache);
   if (cachedir.empty() == true || cachefile.empty() == true || DirectoryExists(cachedir) == false)
      return;
   cachefile.append(".");
   std::vector<std::string> caches = GetListOfFilesInDir(cachedir, false);
   for (std::vector<std::string>::const_iterator file = caches.begin(); file != caches.end(); ++file)
   {
      std::string nuke = flNotDir(*file);
      if (strncmp(cachefile.c_str(), nuke.c_str(), cachefile.length()) != 0)
	 continue;
      RemoveFile("RemoveCaches", *file);
   }
}
									/*}}}*/
// CacheFile::Close - close the cache files				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgCacheFile::Close()
{
   if (ExternOwner == false)
   {
      delete DCache;
      delete Cache;
      delete Map;
   }
   else
      ExternOwner = false;
   delete Policy;
   delete SrcList;
   if (d->WithLock == true)
   {
      _system->UnLock(true);
      d->WithLock = false;
   }

   Map = NULL;
   DCache = NULL;
   Policy = NULL;
   Cache = NULL;
   SrcList = NULL;
}
									/*}}}*/
void pkgCacheFile::InhibitActionGroups(bool const yes)
{
   d->InhibitActionGroups = yes;
}

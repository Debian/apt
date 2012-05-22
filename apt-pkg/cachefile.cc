// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cachefile.cc,v 1.8 2002/04/27 04:28:04 jgg Exp $
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
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/progress.h>

#include <apti18n.h>
									/*}}}*/
// CacheFile::CacheFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::pkgCacheFile() : d(NULL), Map(NULL), Cache(NULL), DCache(NULL),
				SrcList(NULL), Policy(NULL)
{
}
									/*}}}*/
// CacheFile::~CacheFile - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::~pkgCacheFile()
{
   delete DCache;
   delete Policy;
   delete SrcList;
   delete Cache;
   delete Map;
   _system->UnLock(true);
}
									/*}}}*/
// CacheFile::BuildCaches - Open and build the cache files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildCaches(OpProgress *Progress, bool WithLock)
{
   if (Cache != NULL)
      return true;

   if (_config->FindB("pkgCacheFile::Generate", true) == false)
   {
      Map = new MMap(*new FileFd(_config->FindFile("Dir::Cache::pkgcache"),
		     FileFd::ReadOnly),MMap::Public|MMap::ReadOnly);
      Cache = new pkgCache(Map);
      if (_error->PendingError() == true)
         return false;
      return true;
   }

   const bool ErrorWasEmpty = _error->empty();
   if (WithLock == true)
      if (_system->Lock() == false)
	 return false;
   
   if (_config->FindB("Debug::NoLocking",false) == true)
      WithLock = false;
      
   if (_error->PendingError() == true)
      return false;

   BuildSourceList(Progress);

   // Read the caches
   bool Res = pkgCacheGenerator::MakeStatusCache(*SrcList,Progress,&Map,!WithLock);
   if (Progress != NULL)
      Progress->Done();
   if (Res == false)
      return _error->Error(_("The package lists or status file could not be parsed or opened."));

   /* This sux, remove it someday */
   if (ErrorWasEmpty == true && _error->empty() == false)
      _error->Warning(_("You may want to run apt-get update to correct these problems"));

   Cache = new pkgCache(Map);
   if (_error->PendingError() == true)
      return false;
   return true;
}
									/*}}}*/
// CacheFile::BuildSourceList - Open and build all relevant sources.list/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildSourceList(OpProgress *Progress)
{
   if (SrcList != NULL)
      return true;

   SrcList = new pkgSourceList();
   if (SrcList->ReadMainList() == false)
      return _error->Error(_("The list of sources could not be read."));
   return true;
}
									/*}}}*/
// CacheFile::BuildPolicy - Open and build all relevant preferences	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildPolicy(OpProgress *Progress)
{
   if (Policy != NULL)
      return true;

   Policy = new pkgPolicy(Cache);
   if (_error->PendingError() == true)
      return false;

   if (ReadPinFile(*Policy) == false || ReadPinDir(*Policy) == false)
      return false;

   return true;
}
									/*}}}*/
// CacheFile::BuildDepCache - Open and build the dependency cache	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildDepCache(OpProgress *Progress)
{
   if (DCache != NULL)
      return true;

   DCache = new pkgDepCache(Cache,Policy);
   if (_error->PendingError() == true)
      return false;

   DCache->Init(Progress);
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
// CacheFile::RemoveCaches - remove all cache files from disk		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgCacheFile::RemoveCaches()
{
   std::string const pkgcache = _config->FindFile("Dir::cache::pkgcache");
   std::string const srcpkgcache = _config->FindFile("Dir::cache::srcpkgcache");

   if (pkgcache.empty() == false && RealFileExists(pkgcache) == true)
      unlink(pkgcache.c_str());
   if (srcpkgcache.empty() == false && RealFileExists(srcpkgcache) == true)
      unlink(srcpkgcache.c_str());
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
	    unlink(file->c_str());
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
      unlink(file->c_str());
   }
}
									/*}}}*/
// CacheFile::Close - close the cache files				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgCacheFile::Close()
{
   delete DCache;
   delete Policy;
   delete Cache;
   delete SrcList;
   delete Map;
   _system->UnLock(true);

   Map = NULL;
   DCache = NULL;
   Policy = NULL;
   Cache = NULL;
   SrcList = NULL;
}
									/*}}}*/

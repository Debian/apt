// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cachefile.cc,v 1.4 1999/06/24 04:06:30 jgg Exp $
/* ######################################################################
   
   CacheFile - Simple wrapper class for opening, generating and whatnot
   
   This class implements a simple 2 line mechanism to open various sorts
   of caches. It can operate as root, as not root, show progress and so on,
   it transparently handles everything necessary.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/cachefile.h"
#endif

#include <apt-pkg/cachefile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/configuration.h>
									/*}}}*/

// CacheFile::CacheFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::pkgCacheFile() : Map(0), Cache(0), Lock(0) 
{
}
									/*}}}*/
// CacheFile::~CacheFile - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::~pkgCacheFile()
{
   delete Cache;
   delete Map;
   delete Lock;
}   
									/*}}}*/
// CacheFile::Open - Open the cache files, creating if necessary	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::Open(OpProgress &Progress,bool WithLock)
{
   if (WithLock == true)
      Lock = new pkgDpkgLock;
   
   if (_error->PendingError() == true)
      return false;
   
   // Read the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return _error->Error("The list of sources could not be read.");
   
   /* Build all of the caches, using the cache files if we are locking 
      (ie as root) */
   if (WithLock == true)
   {
      pkgMakeStatusCache(List,Progress);
      Progress.Done();
      if (_error->PendingError() == true)
	 return _error->Error("The package lists or status file could not be parsed or opened.");
      if (_error->empty() == false)
	 _error->Warning("You may want to run apt-get update to correct these missing files");
      
      // Open the cache file
      FileFd File(_config->FindFile("Dir::Cache::pkgcache"),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return false;
      
      Map = new MMap(File,MMap::Public | MMap::ReadOnly);
      if (_error->PendingError() == true)
	 return false;
   }
   else
   {
      Map = pkgMakeStatusCacheMem(List,Progress);
      Progress.Done();
      if (Map == 0)
	 return false;
   }
   
   // Create the dependency cache
   Cache = new pkgDepCache(*Map,Progress);
   Progress.Done();
   if (_error->PendingError() == true)
      return false;
   
   return true;
}
									/*}}}*/

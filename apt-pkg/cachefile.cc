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
#include <apt-pkg/cachefile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/fileutl.h>
    
#include <apti18n.h>
									/*}}}*/

// CacheFile::CacheFile - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCacheFile::pkgCacheFile() : Map(0), Cache(0), DCache(0), Policy(0)
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
   delete Cache;
   delete Map;
   _system->UnLock(true);
}   
									/*}}}*/
// CacheFile::BuildCaches - Open and build the cache files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::BuildCaches(OpProgress &Progress,bool WithLock)
{
   if (WithLock == true)
      if (_system->Lock() == false)
	 return false;
   
   if (_config->FindB("Debug::NoLocking",false) == true)
      WithLock = false;
      
   if (_error->PendingError() == true)
      return false;
   
   // Read the source list
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return _error->Error(_("The list of sources could not be read."));

   // Read the caches
   bool Res = pkgMakeStatusCache(List,Progress,&Map,!WithLock);
   Progress.Done();
   if (Res == false)
      return _error->Error(_("The package lists or status file could not be parsed or opened."));

   /* This sux, remove it someday */
   if (_error->empty() == false)
      _error->Warning(_("You may want to run apt-get update to correct these problems"));

   Cache = new pkgCache(Map);
   if (_error->PendingError() == true)
      return false;
   return true;
}
									/*}}}*/
// CacheFile::Open - Open the cache files, creating if necessary	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheFile::Open(OpProgress &Progress,bool WithLock)
{
   if (BuildCaches(Progress,WithLock) == false)
      return false;
   
   // The policy engine
   Policy = new pkgPolicy(Cache);
   if (_error->PendingError() == true)
      return false;
   if (ReadPinFile(*Policy) == false)
      return false;
   
   // Create the dependency cache
   DCache = new pkgDepCache(Cache,Policy);
   if (_error->PendingError() == true)
      return false;
   
   DCache->Init(&Progress);
   Progress.Done();
   if (_error->PendingError() == true)
      return false;
   
   return true;
}
									/*}}}*/

// CacheFile::ListUpdate - update the cache files                    	/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple wrapper to update the cache. it will fetch stuff
 * from the network (or any other sources defined in sources.list)
 */
bool pkgCacheFile::ListUpdate(pkgAcquireStatus &Stat, 
			      pkgSourceList &List, 
			      int PulseInterval)
{
   pkgAcquire::RunResult res;
   pkgAcquire Fetcher(&Stat);

   // Populate it with the source selection
   if (List.GetIndexes(&Fetcher) == false)
	 return false;

   // Run scripts
   RunScripts("APT::Update::Pre-Invoke");
   
   // check arguments
   if(PulseInterval>0)
      res = Fetcher.Run(PulseInterval);
   else
      res = Fetcher.Run();

   if (res == pkgAcquire::Failed)
      return false;

   bool Failed = false;
   bool TransientNetworkFailure = false;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); 
	I != Fetcher.ItemsEnd(); I++)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 continue;

      (*I)->Finished();

      fprintf(stderr,_("Failed to fetch %s  %s\n"),(*I)->DescURI().c_str(),
	      (*I)->ErrorText.c_str());

      if ((*I)->Status == pkgAcquire::Item::StatTransientNetworkError) 
      {
	 TransientNetworkFailure = true;
	 continue;
      }

      Failed = true;
   }
   
   // Clean out any old list files
   // Keep "APT::Get::List-Cleanup" name for compatibility, but
   // this is really a global option for the APT library now
   if (!TransientNetworkFailure && !Failed &&
       (_config->FindB("APT::Get::List-Cleanup",true) == true ||
	_config->FindB("APT::List-Cleanup",true) == true))
   {
      if (Fetcher.Clean(_config->FindDir("Dir::State::lists")) == false ||
	  Fetcher.Clean(_config->FindDir("Dir::State::lists") + "partial/") == false)
	 // something went wrong with the clean
	 return false;
   }
   
   if (TransientNetworkFailure == true)
      _error->Warning(_("Some index files failed to download, they have been ignored, or old ones used instead."));
   else if (Failed == true)
      return _error->Error(_("Some index files failed to download, they have been ignored, or old ones used instead."));


   // Run the scripts if all was fine
   RunScripts("APT::Update::Post-Invoke");
   return true;
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
   delete Map;
   _system->UnLock(true);

   Map = 0;
   DCache = 0;
   Policy = 0;
   Cache = 0;
}
									/*}}}*/

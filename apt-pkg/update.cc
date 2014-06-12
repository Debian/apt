// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/update.h>

#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// ListUpdate - construct Fetcher and update the cache files		/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple wrapper to update the cache. it will fetch stuff
 * from the network (or any other sources defined in sources.list)
 */
bool ListUpdate(pkgAcquireStatus &Stat, 
		pkgSourceList &List, 
		int PulseInterval)
{
   pkgAcquire Fetcher;
   if (Fetcher.Setup(&Stat, _config->FindDir("Dir::State::Lists")) == false)
      return false;

   // Populate it with the source selection
   if (List.GetIndexes(&Fetcher) == false)
	 return false;

   return AcquireUpdate(Fetcher, PulseInterval, true);
}
									/*}}}*/
// AcquireUpdate - take Fetcher and update the cache files		/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple wrapper to update the cache with a provided acquire
 * If you only need control over Status and the used SourcesList use
 * ListUpdate method instead.
 */
bool AcquireUpdate(pkgAcquire &Fetcher, int const PulseInterval,
		   bool const RunUpdateScripts, bool const ListCleanup)
{
   // Run scripts
   if (RunUpdateScripts == true)
      RunScripts("APT::Update::Pre-Invoke");

   pkgAcquire::RunResult res;
   if(PulseInterval > 0)
      res = Fetcher.Run(PulseInterval);
   else
      res = Fetcher.Run();

   if (res == pkgAcquire::Failed)
      return false;

   bool Failed = false;
   bool TransientNetworkFailure = false;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); 
	I != Fetcher.ItemsEnd(); ++I)
   {
      if ((*I)->Status == pkgAcquire::Item::StatDone)
	 continue;

      (*I)->Finished();

      ::URI uri((*I)->DescURI());
      uri.User.clear();
      uri.Password.clear();
      string descUri = string(uri);
      _error->Warning(_("Failed to fetch %s  %s\n"), descUri.c_str(),
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
   if (!TransientNetworkFailure && !Failed && ListCleanup == true &&
       (_config->FindB("APT::Get::List-Cleanup",true) == true &&
	_config->FindB("APT::List-Cleanup",true) == true))
   {
      if (Fetcher.Clean(_config->FindDir("Dir::State::lists")) == false ||
	  Fetcher.Clean(_config->FindDir("Dir::State::lists") + "partial/") == false)
	 // something went wrong with the clean
	 return false;
   }
   
   if (TransientNetworkFailure == true)
      _error->Warning(_("Some index files failed to download. They have been ignored, or old ones used instead."));
   else if (Failed == true)
      return _error->Error(_("Some index files failed to download. They have been ignored, or old ones used instead."));


   // Run the success scripts if all was fine
   if (RunUpdateScripts == true)
   {
      if(!TransientNetworkFailure && !Failed)
	 RunScripts("APT::Update::Post-Invoke-Success");

      // Run the other scripts
      RunScripts("APT::Update::Post-Invoke");
   }
   return true;
}
									/*}}}*/

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
   pkgAcquire Fetcher(&Stat);
   if (Fetcher.GetLock(_config->FindDir("Dir::State::Lists")) == false)
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

   bool const errorsWereReported = (res == pkgAcquire::Failed);
   bool Failed = errorsWereReported;
   bool TransientNetworkFailure = false;
   bool AllFailed = true;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); 
	I != Fetcher.ItemsEnd(); ++I)
   {
      switch ((*I)->Status)
      {
	 case pkgAcquire::Item::StatDone:
	    AllFailed = false;
	    continue;
	 case pkgAcquire::Item::StatTransientNetworkError:
	    TransientNetworkFailure = true;
	    break;
	 case pkgAcquire::Item::StatIdle:
	 case pkgAcquire::Item::StatFetching:
	 case pkgAcquire::Item::StatError:
	 case pkgAcquire::Item::StatAuthError:
	    Failed = true;
	    break;
      }

      (*I)->Finished();

      if (errorsWereReported)
	 continue;

      ::URI uri((*I)->DescURI());
      uri.User.clear();
      uri.Password.clear();
      std::string const descUri = std::string(uri);
      // Show an error for non-transient failures, otherwise only warn
      if ((*I)->Status == pkgAcquire::Item::StatTransientNetworkError)
	 _error->Warning(_("Failed to fetch %s  %s"), descUri.c_str(),
			(*I)->ErrorText.c_str());
      else
	 _error->Error(_("Failed to fetch %s  %s"), descUri.c_str(),
	       (*I)->ErrorText.c_str());
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

   bool Res = true;

   if (errorsWereReported == true)
      Res = false;
   else if (TransientNetworkFailure == true)
      Res = _error->Warning(_("Some index files failed to download. They have been ignored, or old ones used instead."));
   else if (Failed == true)
      Res = _error->Error(_("Some index files failed to download. They have been ignored, or old ones used instead."));

   // Run the success scripts if all was fine
   if (RunUpdateScripts == true)
   {
      if(AllFailed == false)
	 RunScripts("APT::Update::Post-Invoke-Success");

      // Run the other scripts
      RunScripts("APT::Update::Post-Invoke");
   }
   return Res;
}
									/*}}}*/

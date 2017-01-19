// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/update.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/configuration.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-download.h>
#include <apt-private/private-output.h>
#include <apt-private/private-update.h>

#include <ostream>
#include <string>

#include <apti18n.h>
									/*}}}*/

// DoUpdate - Update the package lists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoUpdate(CommandLine &CmdL)
{
   if (CmdL.FileSize() != 1)
      return _error->Error(_("The update command takes no arguments"));

   CacheFile Cache;

   // Get the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();

   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      // force a hashsum for compatibility reasons
      _config->CndSet("Acquire::ForceHash", "md5sum");

      // Populate it with the source selection and get all Indexes
      // (GetAll=true)
      aptAcquireWithTextStatus Fetcher;
      if (List->GetIndexes(&Fetcher,true) == false)
	 return false;

      std::string compExt = APT::Configuration::getCompressionTypes()[0];
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
      {
         std::string FileName = flNotDir(I->Owner->DestFile);
         if(compExt.empty() == false && 
            APT::String::Endswith(FileName, compExt))
            FileName = FileName.substr(0, FileName.size() - compExt.size() - 1);
	 c1out << '\'' << I->URI << "' " << FileName << ' ' <<
	    std::to_string(I->Owner->FileSize) << ' ' << I->Owner->HashSum() << std::endl;
      }
      return true;
   }

   // do the work
   if (_config->FindB("APT::Get::Download",true) == true)
   {
      AcqTextStatus Stat(std::cout, ScreenWidth,_config->FindI("quiet",0));
      ListUpdate(Stat, *List);
   }

   if (_config->FindB("pkgCacheFile::Generate", true) == false)
      return true;

   // Rebuild the cache.
   pkgCacheFile::RemoveCaches();
   if (Cache.BuildCaches(false) == false)
      return false;

   // show basic stats (if the user whishes)
   if (_config->FindB("APT::Cmd::Show-Update-Stats", false) == true)
   {
      int upgradable = 0;
      if (Cache.Open(false) == false)
         return false;
      for (pkgCache::PkgIterator I = Cache->PkgBegin(); I.end() != true; ++I)
      {
         pkgDepCache::StateCache &state = Cache[I];
         if (I->CurrentVer != 0 && state.Upgradable() && state.CandidateVer != NULL)
            upgradable++;
      }
      const char *msg = P_(
         "%i package can be upgraded. Run 'apt list --upgradable' to see it.\n",
         "%i packages can be upgraded. Run 'apt list --upgradable' to see them.\n",
         upgradable);
      if (upgradable == 0)
         c1out << _("All packages are up to date.") << std::endl;
      else
         ioprintf(c1out, msg, upgradable);
   }

   return true;
}
									/*}}}*/

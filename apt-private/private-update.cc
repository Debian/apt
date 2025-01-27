// Include files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/update.h>

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
bool DoUpdate(CommandLine &CmdL)
{
   if (CmdL.FileSize() != 1)
      return _error->Error(_("The update command takes no arguments"));
   return DoUpdate();
}

bool DoUpdate()
{
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

   bool const SLWarnings = _config->FindB("APT::Get::Update::SourceListWarnings", true);
   if (SLWarnings)
      List = Cache.GetSourceList();

   if (_config->FindB("APT::Get::Update::SourceListWarnings::APTAuth", SLWarnings))
   {
      constexpr std::string_view const affected_method[] = {"http", "https", "tor+http", "tor+https", "ftp"};
      for (auto *S : *List)
      {
	 URI uri(S->GetURI());
	 if (uri.User.empty() && uri.Password.empty())
	    continue;
	 // we can't really predict if a +http method supports everything http does,
	 // so we play it safe and use an allowlist here.
	 if (std::find(std::begin(affected_method), std::end(affected_method), uri.Access) != std::end(affected_method))
	    // TRANSLATOR: the first two are manpage references, the last the URI from a sources.list
	    _error->Notice(_("Usage of %s should be preferred over embedding login information directly in the %s entry for '%s'"),
			   "apt_auth.conf(5)", "sources.list(5)", URI::ArchiveOnly(uri).c_str());
      }
   }

   if (_config->FindB("APT::Get::Update::SourceListWarnings::SignedBy", SLWarnings))
   {
      bool modernize = false;
      for (auto *S : *List)
      {
	 URI uri(S->GetURI());

	 if (not S->HasFlag(metaIndex::Flag::DEB822))
	 {
	    // TRANSLATOR: the first is manpage reference, the last the URI from a sources.list
	    _error->Audit(_("The %s entry for '%s' should be upgraded to deb822 .sources"),
			  "sources.list(5)", URI::ArchiveOnly(uri).c_str());
	 }
	 if (S->GetSignedBy().empty())
	 {
	    if (S->HasFlag(metaIndex::Flag::DEB822))
	    {
	       // TRANSLATOR: the first is manpage reference, the last the URI from a sources.list
	       _error->Notice(_("Missing Signed-By in the %s entry for '%s'"),
			      "sources.list(5)", URI::ArchiveOnly(uri).c_str());
	    }
	    else
	    {
	       // TRANSLATOR: the first is manpage reference, the last the URI from a sources.list
	       _error->Audit(_("Missing Signed-By in the %s entry for '%s'"),
			     "sources.list(5)", URI::ArchiveOnly(uri).c_str());
	       modernize = true;
	    }
	 }
      }
      if (modernize)
      {
         _error->Audit(_("Consider migrating all sources.list(5) entries to the deb822 .sources format"));
         _error->Audit(_("The deb822 .sources format supports both embedded as well as external OpenPGP keys"));
	 _error->Audit(_("See apt-secure(8) for best practices in configuring repository signing."));
	 _error->Notice(_("Some sources can be modernized. Run 'apt modernize-sources' to do so."));
      }
   }

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
      {
	 c1out << _config->Find("APT::Color::Bold");
	 ioprintf(c1out, msg, upgradable);
	 c1out << _config->Find("APT::Color::Neutral");
      }

      RunScripts("APT::Update::Post-Invoke-Stats");
   }

   return true;
}
									/*}}}*/

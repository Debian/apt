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
#include <tuple>

#include <apti18n.h>
									/*}}}*/

// DoUpdate - Update the package lists					/*{{{*/
static bool isDebianBookwormRelease(pkgCache::RlsFileIterator const &RlsFile)
{
   std::tuple<std::string_view, std::string_view, std::string_view> const affected[] = {
      {"Debian", "Debian", "bookworm"},
      {"Debian", "Debian", "sid"},
   };
   if (RlsFile.end() || RlsFile->Origin == nullptr || RlsFile->Label == nullptr || RlsFile->Codename == nullptr)
      return false;
   std::tuple<std::string_view, std::string_view, std::string_view> const release{RlsFile.Origin(), RlsFile.Label(), RlsFile.Codename()};
   return std::find(std::begin(affected), std::end(affected), release) != std::end(affected);
}
static void suggestDebianNonFreeFirmware(char const *const repo, char const *const val,
					 char const *const from, char const *const to)
{
   // Both messages are reused from the ReleaseInfoChange feature in acquire-item.cc
   _error->Notice(_("Repository '%s' changed its '%s' value from '%s' to '%s'"), repo, val, from, to);
   std::string notes;
   strprintf(notes, "https://www.debian.org/releases/bookworm/%s/release-notes/ch-information.html#non-free-split", _config->Find("APT::Architecture").c_str());
   _error->Notice(_("More information about this can be found online in the Release notes at: %s"), notes.c_str());
}
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

   if (_config->FindB("APT::Get::Update::SourceListWarnings::NonFreeFirmware", SLWarnings))
   {
      // If a Debian source has a non-free component, suggest adding non-free-firmware
      bool found_affected_release = false;
      bool found_non_free = false;
      bool found_non_free_firmware = false;
      for (auto *S : *List)
      {
	 if (not isDebianBookwormRelease(S->FindInCache(Cache, false)))
	    continue;

	 for (auto PkgFile = Cache.GetPkgCache()->FileBegin(); not PkgFile.end(); ++PkgFile)
	 {
	    if (PkgFile.Flagged(pkgCache::Flag::NoPackages))
	       continue;
	    found_affected_release = true;
	    const auto * const comp = PkgFile.Component();
	    if (comp == nullptr)
	      continue;
	    if (strcmp(comp, "non-free") == 0)
	       found_non_free = true;
	    else if (strcmp(comp, "non-free-firmware") == 0)
	    {
	       found_non_free_firmware = true;
	       break;
	    }
	 }
	 if (found_non_free_firmware)
	    break;
      }
      if (not found_non_free_firmware && found_non_free && found_affected_release)
      {
	 /* See if a well-known firmware package is installable from this codename
	    if so, we likely operate with new apt on an old snapshot not supporting non-free-firmware */
	 bool suggest_non_free_firmware = true;
	 if (auto const Grp = Cache.GetPkgCache()->FindGrp("firmware-linux-nonfree"); not Grp.end())
	 {
	    for (auto Pkg = Grp.PackageList(); not Pkg.end() && suggest_non_free_firmware; Pkg = Grp.NextPkg(Pkg))
	    {
	       for (auto Ver = Pkg.VersionList(); not Ver.end(); ++Ver)
	       {
		  if (not Ver.Downloadable())
		     continue;
		  for (auto VerFile = Ver.FileList(); not VerFile.end(); ++VerFile)
		  {
		     auto const PkgFile = VerFile.File();
		     if (PkgFile.end())
			continue;
		     if (not isDebianBookwormRelease(PkgFile.ReleaseFile()))
			continue;
		     suggest_non_free_firmware = false;
		     break;
		  }
		  if (not suggest_non_free_firmware)
		     break;
	       }
	    }
	 }
	 if (suggest_non_free_firmware)
	    suggestDebianNonFreeFirmware("Debian bookworm", "non-free component", "non-free", "non-free non-free-firmware");
      }

      if (not found_non_free_firmware && not found_non_free && found_affected_release)
      {
	 /* Try to notify users who have installed firmware packages at some point, but
	    have not enabled non-free currently – they might want to opt into updates now */
	 APT::StringView const affected_pkgs[] = {
	    "amd64-microcode", "atmel-firmware", "bluez-firmware", "dahdi-firmware-nonfree",
	    "firmware-amd-graphics", "firmware-ast", "firmware-atheros", "firmware-bnx2",
	    "firmware-bnx2x", "firmware-brcm80211", "firmware-cavium", "firmware-intel-sound",
	    "firmware-intelwimax", "firmware-ipw2x00", "firmware-ivtv", "firmware-iwlwifi",
	    "firmware-libertas", "firmware-linux", "firmware-linux-nonfree", "firmware-misc-nonfree",
	    "firmware-myricom", "firmware-netronome", "firmware-netxen", "firmware-qcom-media",
	    "firmware-qcom-soc", "firmware-qlogic", "firmware-realtek", "firmware-realtek-rtl8723cs-bt",
	    "firmware-samsung", "firmware-siano", "firmware-sof-signed", "firmware-ti-connectivity",
	    "firmware-zd1211", "intel-microcode", "midisport-firmware", "raspi-firmware",
	 };
	 bool suggest_non_free_firmware = false;
	 for (auto pkgname : affected_pkgs)
	 {
	    auto const Grp = Cache.GetPkgCache()->FindGrp(pkgname);
	    if (Grp.end())
	       continue;
	    for (auto Pkg = Grp.PackageList(); not Pkg.end(); Pkg = Grp.NextPkg(Pkg))
	    {
	       auto const Ver = Pkg.CurrentVer();
	       if (Ver.end() || Ver.Downloadable())
		  continue;
	       bool another = false;
	       for (auto V = Pkg.VersionList(); not V.end(); ++V)
		  if (V.Downloadable())
		  {
		     another = true;
		     break;
		  }
	       if (another)
		  continue;
	       suggest_non_free_firmware = true;
	       break;
	    }
	    if (suggest_non_free_firmware)
	       break;
	 }
	 if (suggest_non_free_firmware)
	    suggestDebianNonFreeFirmware("Debian bookworm", "firmware component", "non-free", "non-free-firmware");
      }
   }

   if (_config->FindB("APT::Get::Update::SourceListWarnings::SignedBy", SLWarnings))
   {
      for (auto *S : *List)
      {
	 if (not S->HasFlag(metaIndex::Flag::DEB822) || not S->GetSignedBy().empty())
	    continue;

	 URI uri(S->GetURI());
	 // TRANSLATOR: the first is manpage reference, the last the URI from a sources.list
	 _error->Notice(_("Missing Signed-By in the %s entry for '%s'"),
			"sources.list(5)", URI::ArchiveOnly(uri).c_str());
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

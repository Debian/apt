// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/install-progress.h>
#include <apt-pkg/prettyprinters.h>

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <vector>
#include <map>

#include <apt-private/acqprogress.h>
#include <apt-private/private-install.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-download.h>
#include <apt-private/private-output.h>

#include <apti18n.h>
									/*}}}*/
class pkgSourceList;

bool CheckNothingBroken(CacheFile &Cache)				/*{{{*/
{
   // Now we check the state of the packages,
   if (Cache->BrokenCount() == 0)
      return true;

   // FIXME: if an external solver showed an error, we shouldn't show one here
   if (_error->PendingError() && _config->Find("APT::Solver") == "dump")
      return false;

   c1out <<
      _("Some packages could not be installed. This may mean that you have\n"
	    "requested an impossible situation or if you are using the unstable\n"
	    "distribution that some required packages have not yet been created\n"
	    "or been moved out of Incoming.") << std::endl;
   /*
   if (Packages == 1)
   {
      c1out << std::endl;
      c1out <<
	 _("Since you only requested a single operation it is extremely likely that\n"
	       "the package is simply not installable and a bug report against\n"
	       "that package should be filed.") << std::endl;
   }
   */

   c1out << _("The following information may help to resolve the situation:") << std::endl;
   c1out << std::endl;
   ShowBroken(c1out,Cache,false);
   if (_error->PendingError() == true)
      return false;
   else
      return _error->Error(_("Broken packages"));
}
									/*}}}*/
// InstallPackages - Actually download and install the packages		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the informative messages describing what is going to 
   happen and then calls the download routines */
static void RemoveDownloadNeedingItemsFromFetcher(pkgAcquire &Fetcher, bool &Transient)
{
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd();)
   {
      if ((*I)->Local == true)
      {
	 ++I;
	 continue;
      }

      // Close the item and check if it was found in cache
      (*I)->Finished();
      if ((*I)->Complete == false)
	 Transient = true;

      // Clear it out of the fetch list
      delete *I;
      I = Fetcher.ItemsBegin();
   }
}
bool InstallPackages(CacheFile &Cache,bool ShwKept,bool Ask, bool Safety)
{
   if (_config->FindB("APT::Get::Purge", false) == true)
      for (pkgCache::PkgIterator I = Cache->PkgBegin(); I.end() == false; ++I)
	 if (Cache[I].Delete() == true && Cache[I].Purge() == false)
	    Cache->MarkDelete(I,true);

   // Create the download object
   aptAcquireWithTextStatus Fetcher;
   if (_config->FindB("APT::Get::Print-URIs", false) == true)
   {
      // force a hashsum for compatibility reasons
      _config->CndSet("Acquire::ForceHash", "md5sum");
   }
   else if (_config->FindB("APT::Get::Simulate") == true)
      ;
   else if (Fetcher.GetLock(_config->FindDir("Dir::Cache::Archives")) == false)
      return false;

   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList * const List = Cache.GetSourceList();

   // Create the text record parser
   pkgRecords Recs(Cache);
   if (_error->PendingError() == true)
      return false;

   // Create the package manager and prepare to download
   std::unique_ptr<pkgPackageManager> PM(_system->CreatePM(Cache));
   if (PM->GetArchives(&Fetcher,List,&Recs) == false || 
       _error->PendingError() == true)
      return false;

   if (_config->FindB("APT::Get::Download",true) == false)
   {
      bool Missing = false;
      RemoveDownloadNeedingItemsFromFetcher(Fetcher, Missing);
      if (Missing)
      {
	 if (_config->FindB("APT::Get::Fix-Missing",false))
	    PM->FixMissing();
	 else
	    return _error->Error(_("Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"));
      }
      Fetcher.Shutdown();
      if (_error->PendingError() == true)
	 return false;
   }

   // Show all the various warning indicators
   ShowDel(c1out,Cache);
   ShowNew(c1out,Cache);
   if (ShwKept == true)
      ShowKept(c1out,Cache);
   bool const Hold = !ShowHold(c1out,Cache);
   if (_config->FindB("APT::Get::Show-Upgraded",true) == true)
      ShowUpgraded(c1out,Cache);
   bool const Downgrade = !ShowDowngraded(c1out,Cache);

   bool Essential = false;
   if (_config->FindB("APT::Get::Download-Only",false) == false)
        Essential = !ShowEssential(c1out,Cache);

   Stats(c1out,Cache);

   // Sanity check
   if (Cache->BrokenCount() != 0)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, InstallPackages was called with broken packages!"));
   }

   if (Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
       Cache->BadCount() == 0)
      return true;

   // No remove flag
   if (Cache->DelCount() != 0 && _config->FindB("APT::Get::Remove",true) == false)
      return _error->Error(_("Packages need to be removed but remove is disabled."));

   // Fail safe check
   bool const Fail = (Essential || Downgrade || Hold);
   if (_config->FindI("quiet",0) >= 2 ||
       _config->FindB("APT::Get::Assume-Yes",false) == true)
   {
      if (_config->FindB("APT::Get::Force-Yes",false) == true) {
	 _error->Warning(_("--force-yes is deprecated, use one of the options starting with --allow instead."));
      }

      if (Fail == true && _config->FindB("APT::Get::Force-Yes",false) == false) {
	 if (Essential == true && _config->FindB("APT::Get::allow-remove-essential", false) == false)
	    return _error->Error(_("Essential packages were removed and -y was used without --allow-remove-essential."));
	 if (Downgrade == true && _config->FindB("APT::Get::allow-downgrades", false) == false)
	    return _error->Error(_("Packages were downgraded and -y was used without --allow-downgrades."));
	 if (Hold == true && _config->FindB("APT::Get::allow-change-held-packages", false) == false)
	    return _error->Error(_("Held packages were changed and -y was used without --allow-change-held-packages."));
      }
   }

   // Run the simulator ..
   if (_config->FindB("APT::Get::Simulate") == true)
   {
      pkgSimulate PM(Cache);

      APT::Progress::PackageManager *progress = APT::Progress::PackageManagerProgressFactory();
      pkgPackageManager::OrderResult Res = PM.DoInstall(progress);
      delete progress;

      if (Res == pkgPackageManager::Failed)
	 return false;
      if (Res != pkgPackageManager::Completed)
	 return _error->Error(_("Internal error, Ordering didn't finish"));
      return true;
   }

   // Display statistics
   auto const FetchBytes = Fetcher.FetchNeeded();
   auto const FetchPBytes = Fetcher.PartialPresent();
   auto const DebBytes = Fetcher.TotalNeeded();
   if (DebBytes != Cache->DebSize())
   {
      c0out << DebBytes << ',' << Cache->DebSize() << std::endl;
      c0out << _("How odd... The sizes didn't match, email apt@packages.debian.org") << std::endl;
   }
   
   // Number of bytes
   if (DebBytes != FetchBytes)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement strings, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB/%sB of archives.\n"),
	       SizeToStr(FetchBytes).c_str(),SizeToStr(DebBytes).c_str());
   else if (DebBytes != 0)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB of archives.\n"),
	       SizeToStr(DebBytes).c_str());

   // Size delta
   if (Cache->UsrSize() >= 0)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("After this operation, %sB of additional disk space will be used.\n"),
	       SizeToStr(Cache->UsrSize()).c_str());
   else
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("After this operation, %sB disk space will be freed.\n"),
	       SizeToStr(-1*Cache->UsrSize()).c_str());

   if (CheckFreeSpaceBeforeDownload(_config->FindDir("Dir::Cache::Archives"), (FetchBytes - FetchPBytes)) == false)
      return false;

   if (_error->PendingError() == true)
      return false;

   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 std::cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' <<
	       std::to_string(I->Owner->FileSize) << ' ' << I->Owner->HashSum() << std::endl;
      return true;
   }

   if (Essential == true && Safety == true && _config->FindB("APT::Get::allow-remove-essential", false) == false)
   {
      if (_config->FindB("APT::Get::Trivial-Only",false) == true)
	 return _error->Error(_("Trivial Only specified but this is not a trivial operation."));

      // TRANSLATOR: This string needs to be typed by the user as a confirmation, so be
      //             careful with hard to type or special characters (like non-breaking spaces)
      const char *Prompt = _("Yes, do as I say!");
      std::string question;
      strprintf(question,
	       _("You are about to do something potentially harmful.\n"
		 "To continue type in the phrase '%s'\n"
		 " ?] "),Prompt);
      if (AnalPrompt(question, Prompt) == false)
      {
	 c2out << _("Abort.") << std::endl;
	 exit(1);
      }
   }
   else
   {
      // Prompt to continue
      if (Ask == true || Fail == true)
      {
	 if (_config->FindB("APT::Get::Trivial-Only",false) == true)
	    return _error->Error(_("Trivial Only specified but this is not a trivial operation."));

	 if (_config->FindI("quiet",0) < 2 &&
	     _config->FindB("APT::Get::Assume-Yes",false) == false)
	 {
	    if (YnPrompt(_("Do you want to continue?")) == false)
	    {
	       c2out << _("Abort.") << std::endl;
	       exit(1);
	    }
	 }
      }
   }

   if (!CheckAuth(Fetcher, true))
      return false;

   /* Unlock the dpkg lock if we are not going to be doing an install
      after. */
   if (_config->FindB("APT::Get::Download-Only",false) == true)
      _system->UnLock();

   // Run it
   bool Failed = false;
   while (1)
   {
      bool Transient = false;
      if (AcquireRun(Fetcher, 0, &Failed, &Transient) == false)
	 return false;

      if (_config->FindB("APT::Get::Download-Only",false) == true)
      {
	 if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
	    return _error->Error(_("Some files failed to download"));
	 c1out << _("Download complete and in download only mode") << std::endl;
	 return true;
      }

      if (Failed == true && _config->FindB("APT::Get::Fix-Missing",false) == false)
	 return _error->Error(_("Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"));

      if (Transient == true && Failed == true)
	 return _error->Error(_("--fix-missing and media swapping is not currently supported"));

      // Try to deal with missing package files
      if (Failed == true && PM->FixMissing() == false)
      {
	 c2out << _("Unable to correct missing packages.") << std::endl;
	 return _error->Error(_("Aborting install."));
      }

      auto const progress = APT::Progress::PackageManagerProgressFactory();
      _system->UnLock();
      pkgPackageManager::OrderResult const Res = PM->DoInstall(progress);
      delete progress;

      if (Res == pkgPackageManager::Failed || _error->PendingError() == true)
	 return false;
      if (Res == pkgPackageManager::Completed)
	 break;

      _system->Lock();

      // Reload the fetcher object and loop again for media swapping
      Fetcher.Shutdown();
      if (PM->GetArchives(&Fetcher,List,&Recs) == false)
	 return false;

      Failed = false;
      if (_config->FindB("APT::Get::Download",true) == false)
	 RemoveDownloadNeedingItemsFromFetcher(Fetcher, Failed);
   }

   std::set<std::string> const disappearedPkgs = PM->GetDisappearedPackages();
   if (disappearedPkgs.empty() == false)
   {
      ShowList(c1out, P_("The following package disappeared from your system as\n"
	       "all files have been overwritten by other packages:",
	       "The following packages disappeared from your system as\n"
	       "all files have been overwritten by other packages:", disappearedPkgs.size()), disappearedPkgs,
	    [](std::string const &Pkg) { return Pkg.empty() == false; },
	    [](std::string const &Pkg) { return Pkg; },
	    [](std::string const &) { return std::string(); });
      c0out << _("Note: This is done automatically and on purpose by dpkg.") << std::endl;
   }

   // cleanup downloaded debs
   if (_config->FindB("APT::Keep-Downloaded-Packages", true) == false)
   {
      std::string const archivedir = _config->FindDir("Dir::Cache::archives");
      for (auto I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); ++I)
      {
	 if (flNotFile((*I)->DestFile) != archivedir || (*I)->Local)
	    continue;
         RemoveFile("Keep-Downloaded-Packages=false", (*I)->DestFile);
      }
   }

   return true;
}
									/*}}}*/
// DoAutomaticRemove - Remove all automatic unused packages		/*{{{*/
// ---------------------------------------------------------------------
/* Remove unused automatic packages */
bool DoAutomaticRemove(CacheFile &Cache)
{
   bool Debug = _config->FindB("Debug::pkgAutoRemove",false);
   bool doAutoRemove = _config->FindB("APT::Get::AutomaticRemove", false);
   bool hideAutoRemove = _config->FindB("APT::Get::HideAutoRemove");

   pkgDepCache::ActionGroup group(*Cache);
   if(Debug)
      std::cout << "DoAutomaticRemove()" << std::endl;

   if (doAutoRemove == true &&
	_config->FindB("APT::Get::Remove",true) == false)
   {
      c1out << _("We are not supposed to delete stuff, can't start "
		 "AutoRemover") << std::endl;
      return false;
   }

   bool purgePkgs = _config->FindB("APT::Get::Purge", false);
   bool smallList = (hideAutoRemove == false &&
		strcasecmp(_config->Find("APT::Get::HideAutoRemove","").c_str(),"small") == 0);

   unsigned long autoRemoveCount = 0;
   APT::PackageSet tooMuch;
   SortedPackageUniverse Universe(Cache);
   // look over the cache to see what can be removed
   for (auto const &Pkg: Universe)
   {
      if (Cache[Pkg].Garbage)
      {
	 if(Pkg.CurrentVer() != 0 || Cache[Pkg].Install())
	    if(Debug)
	       std::cout << "We could delete " <<  APT::PrettyPkg(Cache, Pkg) << std::endl;

	 if (doAutoRemove)
	 {
	    if(Pkg.CurrentVer() != 0 &&
	       Pkg->CurrentState != pkgCache::State::ConfigFiles)
	       Cache->MarkDelete(Pkg, purgePkgs, 0, false);
	    else
	       Cache->MarkKeep(Pkg, false, false);
	 }
	 else
	 {
	    // if the package is a new install and already garbage we don't need to
	    // install it in the first place, so nuke it instead of show it
	    if (Cache[Pkg].Install() == true && Pkg.CurrentVer() == 0)
	    {
	       tooMuch.insert(Pkg);
	       Cache->MarkDelete(Pkg, false, 0, false);
	    }
	    // only show stuff in the list that is not yet marked for removal
	    else if(hideAutoRemove == false && Cache[Pkg].Delete() == false)
	       ++autoRemoveCount;
	 }
      }
   }

   // we could have removed a new dependency of a garbage package,
   // so check if a reverse depends is broken and if so install it again.
   if (tooMuch.empty() == false && (Cache->BrokenCount() != 0 || Cache->PolicyBrokenCount() != 0))
   {
      bool Changed;
      do {
	 Changed = false;
	 for (APT::PackageSet::iterator Pkg = tooMuch.begin();
	      Pkg != tooMuch.end(); ++Pkg)
	 {
	    APT::PackageSet too;
	    too.insert(*Pkg);
	    for (pkgCache::PrvIterator Prv = Cache[Pkg].CandidateVerIter(Cache).ProvidesList();
		 Prv.end() == false; ++Prv)
	       too.insert(Prv.ParentPkg());
	    for (APT::PackageSet::const_iterator P = too.begin(); P != too.end(); ++P)
	    {
	       for (pkgCache::DepIterator R = P.RevDependsList();
		    R.end() == false; ++R)
	       {
		  if (R.IsNegative() == true ||
		      Cache->IsImportantDep(R) == false)
		     continue;
		 auto const RV = R.ParentVer();
		 if (unlikely(RV.end() == true))
		    continue;
		 auto const RP = RV.ParentPkg();
		 // check if that dependency comes from an interesting version
		 if (RP.CurrentVer() == RV)
		 {
		    if ((*Cache)[RP].Keep() == false)
		       continue;
		 }
		 else if (Cache[RP].CandidateVerIter(Cache) == RV)
		 {
		    if ((*Cache)[RP].NewInstall() == false && (*Cache)[RP].Upgrade() == false)
		       continue;
		 }
		 else // ignore dependency from a non-candidate version
		    continue;
		 if (Debug == true)
		    std::clog << "Save " << APT::PrettyPkg(Cache, Pkg) << " as another installed package depends on it: " << APT::PrettyPkg(Cache, RP) << std::endl;
		 Cache->MarkInstall(Pkg, false, 0, false);
		 if (hideAutoRemove == false)
		    ++autoRemoveCount;
		 tooMuch.erase(Pkg);
		 Changed = true;
		 break;
	       }
	       if (Changed == true)
		  break;
	    }
	    if (Changed == true)
	       break;
	 }
      } while (Changed == true);
   }
   // trigger marking now so that the package list below is correct
   group.release();

   // Now see if we had destroyed anything (if we had done anything)
   if (Cache->BrokenCount() != 0)
   {
      c1out << _("Hmm, seems like the AutoRemover destroyed something which really\n"
	         "shouldn't happen. Please file a bug report against apt.") << std::endl;
      c1out << std::endl;
      c1out << _("The following information may help to resolve the situation:") << std::endl;
      c1out << std::endl;
      ShowBroken(c1out,Cache,false);

      return _error->Error(_("Internal Error, AutoRemover broke stuff"));
   }

   // if we don't remove them, we should show them!
   if (doAutoRemove == false && autoRemoveCount != 0)
   {
      if (smallList == false)
      {
	 SortedPackageUniverse Universe(Cache);
	 ShowList(c1out, P_("The following package was automatically installed and is no longer required:",
	          "The following packages were automatically installed and are no longer required:",
	          autoRemoveCount), Universe,
	       [&Cache](pkgCache::PkgIterator const &Pkg) { return (*Cache)[Pkg].Garbage == true && (*Cache)[Pkg].Delete() == false; },
	       &PrettyFullName, CandidateVersion(&Cache));
      }
      else
	 ioprintf(c1out, P_("%lu package was automatically installed and is no longer required.\n",
	          "%lu packages were automatically installed and are no longer required.\n", autoRemoveCount), autoRemoveCount);
      std::string autocmd = "apt autoremove";
      if (getenv("SUDO_USER") != nullptr)
      {
	 auto const envsudocmd = getenv("SUDO_COMMAND");
	 auto const envshell = getenv("SHELL");
	 if (envsudocmd == nullptr || envshell == nullptr || strcmp(envsudocmd, envshell) != 0)
	    autocmd = "sudo " + autocmd;
      }
      ioprintf(c1out, P_("Use '%s' to remove it.", "Use '%s' to remove them.", autoRemoveCount), autocmd.c_str());
      c1out << std::endl;
   }
   return true;
}
									/*}}}*/
// DoCacheManipulationFromCommandLine					/*{{{*/
static const unsigned short MOD_REMOVE = 1;
static const unsigned short MOD_INSTALL = 2;

bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, CacheFile &Cache, int UpgradeMode)
{
   std::vector<std::string> VolatileCmdL;
   return DoCacheManipulationFromCommandLine(CmdL, VolatileCmdL, Cache, UpgradeMode);
}
bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, std::vector<std::string> &VolatileCmdL, CacheFile &Cache, int UpgradeMode)
{
   std::map<unsigned short, APT::VersionSet> verset;
   return DoCacheManipulationFromCommandLine(CmdL, VolatileCmdL, Cache, verset, UpgradeMode);
}
bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, std::vector<std::string> &VolatileCmdL, CacheFile &Cache,
                                        std::map<unsigned short, APT::VersionSet> &verset, int UpgradeMode)
{
   // Enter the special broken fixing mode if the user specified arguments
   bool BrokenFix = false;
   if (Cache->BrokenCount() != 0)
      BrokenFix = true;

   std::unique_ptr<pkgProblemResolver> Fix(nullptr);
   if (_config->FindB("APT::Get::CallResolver", true) == true)
      Fix.reset(new pkgProblemResolver(Cache));

   unsigned short fallback = MOD_INSTALL;
   if (strcasecmp(CmdL.FileList[0],"remove") == 0)
      fallback = MOD_REMOVE;
   else if (strcasecmp(CmdL.FileList[0], "purge") == 0)
   {
      _config->Set("APT::Get::Purge", true);
      fallback = MOD_REMOVE;
   }
   else if (strcasecmp(CmdL.FileList[0], "autoremove") == 0 ||
	    strcasecmp(CmdL.FileList[0], "auto-remove") == 0)
   {
      _config->Set("APT::Get::AutomaticRemove", "true");
      fallback = MOD_REMOVE;
   }

   std::list<APT::VersionSet::Modifier> mods;
   mods.push_back(APT::VersionSet::Modifier(MOD_INSTALL, "+",
		APT::VersionSet::Modifier::POSTFIX, APT::CacheSetHelper::CANDIDATE));
   mods.push_back(APT::VersionSet::Modifier(MOD_REMOVE, "-",
		APT::VersionSet::Modifier::POSTFIX, APT::CacheSetHelper::NEWEST));
   CacheSetHelperAPTGet helper(c0out);
   verset = APT::VersionSet::GroupedFromCommandLine(Cache,
		CmdL.FileList + 1, mods, fallback, helper);

   for (auto const &I: VolatileCmdL)
   {
      pkgCache::PkgIterator const P = Cache->FindPkg(I);
      if (P.end())
	 continue;

      // Set any version providing the .deb as the candidate.
      for (auto Prv = P.ProvidesList(); Prv.end() == false; Prv++)
	 Cache.GetDepCache()->SetCandidateVersion(Prv.OwnerVer());

      // via cacheset to have our usual virtual handling
      APT::VersionContainerInterface::FromPackage(&(verset[MOD_INSTALL]), Cache, P, APT::CacheSetHelper::CANDIDATE, helper);
   }

   if (_error->PendingError() == true)
   {
      helper.showVirtualPackageErrors(Cache);
      return false;
   }


  TryToInstall InstallAction(Cache, Fix.get(), BrokenFix);
  TryToRemove RemoveAction(Cache, Fix.get());

   // new scope for the ActionGroup
   {
      pkgDepCache::ActionGroup group(Cache);
      unsigned short const order[] = { MOD_REMOVE, MOD_INSTALL, 0 };

      for (unsigned short i = 0; order[i] != 0; ++i)
      {
	 if (order[i] == MOD_INSTALL)
	    InstallAction = std::for_each(verset[MOD_INSTALL].begin(), verset[MOD_INSTALL].end(), InstallAction);
	 else if (order[i] == MOD_REMOVE)
	    RemoveAction = std::for_each(verset[MOD_REMOVE].begin(), verset[MOD_REMOVE].end(), RemoveAction);
      }

      if (Fix != NULL && _config->FindB("APT::Get::AutoSolving", true) == true)
      {
	 InstallAction.propergateReleaseCandiateSwitching(helper.selectedByRelease, c0out);
	 InstallAction.doAutoInstall();
      }

      if (_error->PendingError() == true)
      {
	 return false;
      }

      /* If we are in the Broken fixing mode we do not attempt to fix the
	 problems. This is if the user invoked install without -f and gave
	 packages */
      if (BrokenFix == true && Cache->BrokenCount() != 0)
      {
	 c1out << _("You might want to run 'apt --fix-broken install' to correct these.") << std::endl;
	 ShowBroken(c1out,Cache,false);
	 return _error->Error(_("Unmet dependencies. Try 'apt --fix-broken install' with no packages (or specify a solution)."));
      }

      if (Fix != NULL)
      {
	 // Call the scored problem resolver
	 OpTextProgress Progress(*_config);
	 bool const distUpgradeMode = strcmp(CmdL.FileList[0], "dist-upgrade") == 0 || strcmp(CmdL.FileList[0], "full-upgrade") == 0;

	 bool resolver_fail = false;
	 if (distUpgradeMode == true || UpgradeMode != APT::Upgrade::ALLOW_EVERYTHING)
	    resolver_fail = APT::Upgrade::Upgrade(Cache, UpgradeMode, &Progress);
	 else
	    resolver_fail = Fix->Resolve(true, &Progress);

	 if (resolver_fail == false && Cache->BrokenCount() == 0)
	    return false;
      }

      if (CheckNothingBroken(Cache) == false)
	 return false;
   }
   if (!DoAutomaticRemove(Cache)) 
      return false;

   // if nothing changed in the cache, but only the automark information
   // we write the StateFile here, otherwise it will be written in 
   // cache.commit()
   if (InstallAction.AutoMarkChanged > 0 &&
       Cache->DelCount() == 0 && Cache->InstCount() == 0 &&
       Cache->BadCount() == 0 &&
       _config->FindB("APT::Get::Simulate",false) == false)
      Cache->writeStateFile(NULL);

   return true;
}
									/*}}}*/
// DoInstall - Install packages from the command line			/*{{{*/
// ---------------------------------------------------------------------
/* Install named packages */
struct PkgIsExtraInstalled {
   pkgCacheFile * const Cache;
   APT::VersionSet const * const verset;
   PkgIsExtraInstalled(pkgCacheFile * const Cache, APT::VersionSet const * const Container) : Cache(Cache), verset(Container) {}
   bool operator() (pkgCache::PkgIterator const &Pkg)
   {
        if ((*Cache)[Pkg].Install() == false)
           return false;
        pkgCache::VerIterator const Cand = (*Cache)[Pkg].CandidateVerIter(*Cache);
        return verset->find(Cand) == verset->end();
   }
};
bool DoInstall(CommandLine &CmdL)
{
   CacheFile Cache;
   std::vector<std::string> VolatileCmdL;
   Cache.GetSourceList()->AddVolatileFiles(CmdL, &VolatileCmdL);

   // then open the cache
   if (Cache.OpenForInstall() == false || 
       Cache.CheckDeps(CmdL.FileSize() != 1) == false)
      return false;

   std::map<unsigned short, APT::VersionSet> verset;
   if(!DoCacheManipulationFromCommandLine(CmdL, VolatileCmdL, Cache, verset, 0))
      return false;

   /* Print out a list of packages that are going to be installed extra
      to what the user asked */
   SortedPackageUniverse Universe(Cache);
   if (Cache->InstCount() != verset[MOD_INSTALL].size())
      ShowList(c1out, _("The following additional packages will be installed:"), Universe,
	    PkgIsExtraInstalled(&Cache, &verset[MOD_INSTALL]),
	    &PrettyFullName, CandidateVersion(&Cache));

   /* Print out a list of suggested and recommended packages */
   {
      std::list<std::string> Recommends, Suggests, SingleRecommends, SingleSuggests;
      for (auto const &Pkg: Universe)
      {
	 /* Just look at the ones we want to install */
	 if ((*Cache)[Pkg].Install() == false)
	   continue;

	 // get the recommends/suggests for the candidate ver
	 pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);
	 for (pkgCache::DepIterator D = CV.DependsList(); D.end() == false; )
	 {
	    pkgCache::DepIterator Start;
	    pkgCache::DepIterator End;
	    D.GlobOr(Start,End); // advances D
	    if (Start->Type != pkgCache::Dep::Recommends && Start->Type != pkgCache::Dep::Suggests)
	       continue;

	    {
	       // Skip if we already saw this
	       std::string target;
	       for (pkgCache::DepIterator I = Start; I != D; ++I)
	       {
		  if (target.empty() == false)
		     target.append(" | ");
		  target.append(I.TargetPkg().FullName(true));
	       }
	       std::list<std::string> &Type = Start->Type == pkgCache::Dep::Recommends ? SingleRecommends : SingleSuggests;
	       if (std::find(Type.begin(), Type.end(), target) != Type.end())
		  continue;
	       Type.push_back(target);
	    }

	    std::list<std::string> OrList;
	    bool foundInstalledInOrGroup = false;
	    for (pkgCache::DepIterator I = Start; I != D; ++I)
	    {
	       {
		  // satisfying package is installed and not marked for deletion
		  APT::VersionList installed = APT::VersionList::FromDependency(Cache, I, APT::CacheSetHelper::INSTALLED);
		  if (std::find_if(installed.begin(), installed.end(),
			   [&Cache](pkgCache::VerIterator const &Ver) { return Cache[Ver.ParentPkg()].Delete() == false; }) != installed.end())
		  {
		     foundInstalledInOrGroup = true;
		     break;
		  }
	       }

	       {
		  // satisfying package is upgraded to/new install
		  APT::VersionList upgrades = APT::VersionList::FromDependency(Cache, I, APT::CacheSetHelper::CANDIDATE);
		  if (std::find_if(upgrades.begin(), upgrades.end(),
			   [&Cache](pkgCache::VerIterator const &Ver) { return Cache[Ver.ParentPkg()].Upgrade(); }) != upgrades.end())
		  {
		     foundInstalledInOrGroup = true;
		     break;
		  }
	       }

	       if (OrList.empty())
		  OrList.push_back(I.TargetPkg().FullName(true));
	       else
		  OrList.push_back("| " + I.TargetPkg().FullName(true));
	    }

	    if(foundInstalledInOrGroup == false)
	    {
	       std::list<std::string> &Type = Start->Type == pkgCache::Dep::Recommends ? Recommends : Suggests;
	       std::move(OrList.begin(), OrList.end(), std::back_inserter(Type));
	    }
	 }
      }
      auto always_true = [](std::string const&) { return true; };
      auto string_ident = [](std::string const&str) { return str; };
      auto verbose_show_candidate =
	 [&Cache](std::string str)
	 {
	    if (APT::String::Startswith(str, "| "))
	       str.erase(0, 2);
	    pkgCache::PkgIterator const Pkg = Cache->FindPkg(str);
	    if (Pkg.end() == true)
	       return "";
	    return (*Cache)[Pkg].CandVersion;
	 };
      ShowList(c1out,_("Suggested packages:"), Suggests,
	    always_true, string_ident, verbose_show_candidate);
      ShowList(c1out,_("Recommended packages:"), Recommends,
	    always_true, string_ident, verbose_show_candidate);
   }

   // See if we need to prompt
   // FIXME: check if really the packages in the set are going to be installed
   if (Cache->InstCount() == verset[MOD_INSTALL].size() && Cache->DelCount() == 0)
      return InstallPackages(Cache,false,false);

   return InstallPackages(Cache,false);
}
									/*}}}*/

// TryToInstall - Mark a package for installation			/*{{{*/
void TryToInstall::operator() (pkgCache::VerIterator const &Ver) {
   if (unlikely(Ver.end()))
   {
      _error->Fatal("The given version to TryToInstall is invalid!");
      return;
   }
   pkgCache::PkgIterator Pkg = Ver.ParentPkg();
   if (unlikely(Pkg.end()))
   {
      _error->Fatal("The given version to TryToInstall has an invalid parent package!");
      return;
   }

   Cache->GetDepCache()->SetCandidateVersion(Ver);
   pkgDepCache::StateCache &State = (*Cache)[Pkg];

   // Handle the no-upgrade case
   if (_config->FindB("APT::Get::upgrade",true) == false && Pkg->CurrentVer != 0)
      ioprintf(c1out,_("Skipping %s, it is already installed and upgrade is not set.\n"),
	    Pkg.FullName(true).c_str());
   // Ignore request for install if package would be new
   else if (_config->FindB("APT::Get::Only-Upgrade", false) == true && Pkg->CurrentVer == 0)
      ioprintf(c1out,_("Skipping %s, it is not installed and only upgrades are requested.\n"),
	    Pkg.FullName(true).c_str());
   else {
      if (Fix != NULL) {
	 Fix->Clear(Pkg);
	 Fix->Protect(Pkg);
      }
      Cache->GetDepCache()->MarkInstall(Pkg,false);

      if (State.Install() == false) {
	 if (_config->FindB("APT::Get::ReInstall",false) == true) {
	    if (Pkg->CurrentVer == 0 || Pkg.CurrentVer().Downloadable() == false)
	       ioprintf(c1out,_("Reinstallation of %s is not possible, it cannot be downloaded.\n"),
		     Pkg.FullName(true).c_str());
	    else
	       Cache->GetDepCache()->SetReInstall(Pkg, true);
	 } else
	    // TRANSLATORS: First string is package name, second is version
	    ioprintf(c1out,_("%s is already the newest version (%s).\n"),
		  Pkg.FullName(true).c_str(), Pkg.CurrentVer().VerStr());
      }

      // Install it with autoinstalling enabled (if we not respect the minial
      // required deps or the policy)
      if (FixBroken == false)
	 doAutoInstallLater.insert(Pkg);
   }

   // see if we need to fix the auto-mark flag
   // e.g. apt-get install foo
   // where foo is marked automatic
   if (State.Install() == false &&
	 (State.Flags & pkgCache::Flag::Auto) &&
	 _config->FindB("APT::Get::ReInstall",false) == false &&
	 _config->FindB("APT::Get::Only-Upgrade",false) == false &&
	 _config->FindB("APT::Get::Download-Only",false) == false)
   {
      ioprintf(c1out,_("%s set to manually installed.\n"),
	    Pkg.FullName(true).c_str());
      Cache->GetDepCache()->MarkAuto(Pkg,false);
      AutoMarkChanged++;
   }
}
									/*}}}*/
bool TryToInstall::propergateReleaseCandiateSwitching(std::list<std::pair<pkgCache::VerIterator, std::string> > const &start, std::ostream &out)/*{{{*/
{
   for (std::list<std::pair<pkgCache::VerIterator, std::string> >::const_iterator s = start.begin();
	 s != start.end(); ++s)
      Cache->GetDepCache()->SetCandidateVersion(s->first);

   bool Success = true;
   // the Changed list contains:
   //   first: "new version"
   //   second: "what-caused the change"
   std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > Changed;
   for (std::list<std::pair<pkgCache::VerIterator, std::string> >::const_iterator s = start.begin();
	 s != start.end(); ++s)
   {
      Changed.push_back(std::make_pair(s->first, pkgCache::VerIterator(*Cache)));
      // We continue here even if it failed to enhance the ShowBroken output
      Success &= Cache->GetDepCache()->SetCandidateRelease(s->first, s->second, Changed);
   }
   for (std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> >::const_iterator c = Changed.begin();
	 c != Changed.end(); ++c)
   {
      if (c->second.end() == true)
	 ioprintf(out, _("Selected version '%s' (%s) for '%s'\n"),
	       c->first.VerStr(), c->first.RelStr().c_str(), c->first.ParentPkg().FullName(true).c_str());
      else if (c->first.ParentPkg()->Group != c->second.ParentPkg()->Group)
      {
	 pkgCache::VerIterator V = (*Cache)[c->first.ParentPkg()].CandidateVerIter(*Cache);
	 ioprintf(out, _("Selected version '%s' (%s) for '%s' because of '%s'\n"), V.VerStr(),
	       V.RelStr().c_str(), V.ParentPkg().FullName(true).c_str(), c->second.ParentPkg().FullName(true).c_str());
      }
   }
   return Success;
}
									/*}}}*/
void TryToInstall::doAutoInstall() {					/*{{{*/
   for (APT::PackageSet::const_iterator P = doAutoInstallLater.begin();
	 P != doAutoInstallLater.end(); ++P) {
      pkgDepCache::StateCache &State = (*Cache)[P];
      if (State.InstBroken() == false && State.InstPolicyBroken() == false)
	 continue;
      Cache->GetDepCache()->MarkInstall(P, true);
   }
   doAutoInstallLater.clear();
}
									/*}}}*/
// TryToRemove - Mark a package for removal				/*{{{*/
void TryToRemove::operator() (pkgCache::VerIterator const &Ver)
{
   pkgCache::PkgIterator Pkg = Ver.ParentPkg();

   if (Fix != NULL)
   {
      Fix->Clear(Pkg);
      Fix->Protect(Pkg);
      Fix->Remove(Pkg);
   }

   if ((Pkg->CurrentVer == 0 && PurgePkgs == false) ||
	 (PurgePkgs == true && Pkg->CurrentState == pkgCache::State::NotInstalled))
   {
      pkgCache::GrpIterator Grp = Pkg.Group();
      pkgCache::PkgIterator P = Grp.PackageList();
      for (; P.end() != true; P = Grp.NextPkg(P))
      {
	 if (P == Pkg)
	    continue;
	 if (P->CurrentVer != 0 || (PurgePkgs == true && P->CurrentState != pkgCache::State::NotInstalled))
	 {
	    // TRANSLATORS: Note, this is not an interactive question
	    ioprintf(c1out,_("Package '%s' is not installed, so not removed. Did you mean '%s'?\n"),
		  Pkg.FullName(true).c_str(), P.FullName(true).c_str());
	    break;
	 }
      }
      if (P.end() == true)
	 ioprintf(c1out,_("Package '%s' is not installed, so not removed\n"),Pkg.FullName(true).c_str());

      // MarkInstall refuses to install packages on hold
      Pkg->SelectedState = pkgCache::State::Hold;
   }
   else
      Cache->GetDepCache()->MarkDelete(Pkg, PurgePkgs);
}
									/*}}}*/

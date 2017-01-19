// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <string>

#include <apti18n.h>
									/*}}}*/

// DistUpgrade - Distribution upgrade					/*{{{*/
// ---------------------------------------------------------------------
/* This autoinstalls every package and then force installs every 
   pre-existing package. This creates the initial set of conditions which 
   most likely contain problems because too many things were installed.
   
   The problem resolver is used to resolve the problems.
 */
static bool pkgDistUpgrade(pkgDepCache &Cache, OpProgress * const Progress)
{
   std::string const solver = _config->Find("APT::Solver", "internal");
   auto const ret = EDSP::ResolveExternal(solver.c_str(), Cache, EDSP::Request::UPGRADE_ALL, Progress);
   if (solver != "internal")
      return ret;

   if (Progress != NULL)
      Progress->OverallProgress(0, 100, 1, _("Calculating upgrade"));

   pkgDepCache::ActionGroup group(Cache);

   /* Upgrade all installed packages first without autoinst to help the resolver
      in versioned or-groups to upgrade the old solver instead of installing
      a new one (if the old solver is not the first one [anymore]) */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (I->CurrentVer != 0)
	 Cache.MarkInstall(I, false, 0, false);

   if (Progress != NULL)
      Progress->Progress(10);

   /* Auto upgrade all installed packages, this provides the basis 
      for the installation */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (I->CurrentVer != 0)
	 Cache.MarkInstall(I, true, 0, false);

   if (Progress != NULL)
      Progress->Progress(50);

   /* Now, install each essential package which is not installed
      (and not provided by another package in the same name group) */
   std::string essential = _config->Find("pkgCacheGen::Essential", "all");
   if (essential == "all")
   {
      for (pkgCache::GrpIterator G = Cache.GrpBegin(); G.end() == false; ++G)
      {
	 bool isEssential = false;
	 bool instEssential = false;
	 for (pkgCache::PkgIterator P = G.PackageList(); P.end() == false; P = G.NextPkg(P))
	 {
	    if ((P->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential)
	       continue;
	    isEssential = true;
	    if (Cache[P].Install() == true)
	    {
	       instEssential = true;
	       break;
	    }
	 }
	 if (isEssential == false || instEssential == true)
	    continue;
	 pkgCache::PkgIterator P = G.FindPreferredPkg();
	 Cache.MarkInstall(P, true, 0, false);
      }
   }
   else if (essential != "none")
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
	 if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    Cache.MarkInstall(I, true, 0, false);

   if (Progress != NULL)
      Progress->Progress(55);

   /* We do it again over all previously installed packages to force 
      conflict resolution on them all. */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (I->CurrentVer != 0)
	 Cache.MarkInstall(I, false, 0, false);

   if (Progress != NULL)
      Progress->Progress(65);

   pkgProblemResolver Fix(&Cache);

   if (Progress != NULL)
      Progress->Progress(95);

   // Hold back held packages.
   if (_config->FindB("APT::Ignore-Hold",false) == false)
   {
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      {
	 if (I->SelectedState == pkgCache::State::Hold)
	 {
	    Fix.Protect(I);
	    Cache.MarkKeep(I, false, false);
	 }
      }
   }

   bool const success = Fix.ResolveInternal(false);
   if (Progress != NULL)
      Progress->Done();
   return success;
}
bool pkgDistUpgrade(pkgDepCache &Cache)
{
   return pkgDistUpgrade(Cache, NULL);
}
									/*}}}*/
// AllUpgradeNoNewPackages - Upgrade but no removals or new pkgs        /*{{{*/
static bool pkgAllUpgradeNoNewPackages(pkgDepCache &Cache, OpProgress * const Progress)
{
   std::string const solver = _config->Find("APT::Solver", "internal");
   constexpr auto flags = EDSP::Request::UPGRADE_ALL | EDSP::Request::FORBID_NEW_INSTALL | EDSP::Request::FORBID_REMOVE;
   auto const ret = EDSP::ResolveExternal(solver.c_str(), Cache, flags, Progress);
   if (solver != "internal")
      return ret;

   if (Progress != NULL)
      Progress->OverallProgress(0, 100, 1, _("Calculating upgrade"));

   pkgDepCache::ActionGroup group(Cache);
   pkgProblemResolver Fix(&Cache);

   // Upgrade all installed packages
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (Cache[I].Install() == true)
	 Fix.Protect(I);
	  
      if (_config->FindB("APT::Ignore-Hold",false) == false)
	 if (I->SelectedState == pkgCache::State::Hold)
	    continue;
      
      if (I->CurrentVer != 0 && Cache[I].InstallVer != 0)
	 Cache.MarkInstall(I, false, 0, false);
   }

   if (Progress != NULL)
      Progress->Progress(50);

   // resolve remaining issues via keep
   bool const success = Fix.ResolveByKeepInternal();
   if (Progress != NULL)
      Progress->Done();
   return success;
}
									/*}}}*/
// AllUpgradeWithNewInstalls - Upgrade + install new packages as needed /*{{{*/
// ---------------------------------------------------------------------
/* Right now the system must be consistent before this can be called.
 * Upgrade as much as possible without deleting anything (useful for
 * stable systems)
 */
static bool pkgAllUpgradeWithNewPackages(pkgDepCache &Cache, OpProgress * const Progress)
{
   std::string const solver = _config->Find("APT::Solver", "internal");
   constexpr auto flags = EDSP::Request::UPGRADE_ALL | EDSP::Request::FORBID_REMOVE;
   auto const ret = EDSP::ResolveExternal(solver.c_str(), Cache, flags, Progress);
   if (solver != "internal")
      return ret;

   if (Progress != NULL)
      Progress->OverallProgress(0, 100, 1, _("Calculating upgrade"));

   pkgDepCache::ActionGroup group(Cache);
   pkgProblemResolver Fix(&Cache);

   // provide the initial set of stuff we want to upgrade by marking
   // all upgradable packages for upgrade
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (I->CurrentVer != 0 && Cache[I].InstallVer != 0)
      {
         if (_config->FindB("APT::Ignore-Hold",false) == false)
            if (I->SelectedState == pkgCache::State::Hold)
               continue;

	 Cache.MarkInstall(I, false, 0, false);
      }
   }

   if (Progress != NULL)
      Progress->Progress(10);

   // then let auto-install loose
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (Cache[I].Install())
	 Cache.MarkInstall(I, true, 0, false);

   if (Progress != NULL)
      Progress->Progress(50);

   // ... but it may remove stuff, we need to clean up afterwards again
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (Cache[I].Delete() == true)
	 Cache.MarkKeep(I, false, false);

   if (Progress != NULL)
      Progress->Progress(60);

   // resolve remaining issues via keep
   bool const success = Fix.ResolveByKeepInternal();
   if (Progress != NULL)
      Progress->Done();
   return success;
}
									/*}}}*/
// AllUpgrade - Upgrade as many packages as possible			/*{{{*/
// ---------------------------------------------------------------------
/* Right now the system must be consistent before this can be called.
   It also will not change packages marked for install, it only tries
   to install packages not marked for install */
static bool pkgAllUpgrade(pkgDepCache &Cache, OpProgress * const Progress)
{
   return pkgAllUpgradeNoNewPackages(Cache, Progress);
}
bool pkgAllUpgrade(pkgDepCache &Cache)
{
   return pkgAllUpgrade(Cache, NULL);
}
									/*}}}*/
// MinimizeUpgrade - Minimizes the set of packages to be upgraded	/*{{{*/
// ---------------------------------------------------------------------
/* This simply goes over the entire set of packages and tries to keep 
   each package marked for upgrade. If a conflict is generated then 
   the package is restored. */
bool pkgMinimizeUpgrade(pkgDepCache &Cache)
{   
   pkgDepCache::ActionGroup group(Cache);

   if (Cache.BrokenCount() != 0)
      return false;
   
   // We loop for 10 tries to get the minimal set size.
   bool Change = false;
   unsigned int Count = 0;
   do
   {
      Change = false;
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      {
	 // Not interesting
	 if (Cache[I].Upgrade() == false || Cache[I].NewInstall() == true)
	    continue;

	 // Keep it and see if that is OK
	 Cache.MarkKeep(I, false, false);
	 if (Cache.BrokenCount() != 0)
	    Cache.MarkInstall(I, false, 0, false);
	 else
	 {
	    // If keep didn't actually do anything then there was no change..
	    if (Cache[I].Upgrade() == false)
	       Change = true;
	 }	 
      }      
      ++Count;
   }
   while (Change == true && Count < 10);

   if (Cache.BrokenCount() != 0)
      return _error->Error("Internal Error in pkgMinimizeUpgrade");
   
   return true;
}
									/*}}}*/
// APT::Upgrade::Upgrade - Upgrade using a specific strategy		/*{{{*/
bool APT::Upgrade::Upgrade(pkgDepCache &Cache, int mode, OpProgress * const Progress)
{
APT_IGNORE_DEPRECATED_PUSH
   if (mode == ALLOW_EVERYTHING)
      return pkgDistUpgrade(Cache, Progress);
   else if ((mode & ~FORBID_REMOVE_PACKAGES) == 0)
      return pkgAllUpgradeWithNewPackages(Cache, Progress);
   else if ((mode & ~(FORBID_REMOVE_PACKAGES|FORBID_INSTALL_NEW_PACKAGES)) == 0)
      return pkgAllUpgradeNoNewPackages(Cache, Progress);
   else
      _error->Error("pkgAllUpgrade called with unsupported mode %i", mode);
APT_IGNORE_DEPRECATED_POP
   return false;
}
									/*}}}*/

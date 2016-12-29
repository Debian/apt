// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheset.h>

#include <apt-private/private-output.h>
#include <apt-private/private-cachefile.h>

#include <string.h>
#include <ostream>
#include <cstdlib>

#include <apti18n.h>
									/*}}}*/

using namespace std;

static bool SortPackagesByName(pkgCache * const Owner,
      map_pointer_t const A, map_pointer_t const B)
{
   if (A == 0)
      return false;
   if (B == 0 || A == B)
      return true;
   pkgCache::Group const * const GA = Owner->GrpP + A;
   pkgCache::Group const * const GB = Owner->GrpP + B;
   return strcmp(Owner->StrP + GA->Name, Owner->StrP + GB->Name) <= 0;
}
SortedPackageUniverse::SortedPackageUniverse(CacheFile &Cache) :
      PackageUniverse{Cache}, List(Cache.UniverseList)
{
}
void SortedPackageUniverse::LazyInit() const
{
   if (List.empty() == false)
      return;
   pkgCache * const Owner = data();
   // In Multi-Arch systems Grps are easier to sort than Pkgs
   std::vector<map_pointer_t> GrpList;
   List.reserve(Owner->Head().GroupCount);
   for (pkgCache::GrpIterator I{Owner->GrpBegin()}; I.end() != true; ++I)
      GrpList.emplace_back(I - Owner->GrpP);
   std::stable_sort(GrpList.begin(), GrpList.end(), std::bind( &SortPackagesByName, Owner, std::placeholders::_1, std::placeholders::_2 ));
   List.reserve(Owner->Head().PackageCount);
   for (auto G : GrpList)
   {
      pkgCache::GrpIterator const Grp(*Owner, Owner->GrpP + G);
      for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() != true; P = Grp.NextPkg(P))
	 List.emplace_back(P - Owner->PkgP);
   }
}
// CacheFile::CheckDeps - Open the cache file				/*{{{*/
// ---------------------------------------------------------------------
/* This routine generates the caches and then opens the dependency cache
   and verifies that the system is OK. */
bool CacheFile::CheckDeps(bool AllowBroken)
{
   bool FixBroken = _config->FindB("APT::Get::Fix-Broken",false);

   if (_error->PendingError() == true)
      return false;

   // Check that the system is OK
   if (DCache->DelCount() != 0 || DCache->InstCount() != 0)
      return _error->Error("Internal error, non-zero counts");
   
   // Apply corrections for half-installed packages
   if (pkgApplyStatus(*DCache) == false)
      return false;
   
   if (_config->FindB("APT::Get::Fix-Policy-Broken",false) == true)
   {
      FixBroken = true;
      if ((DCache->PolicyBrokenCount() > 0))
      {
	 // upgrade all policy-broken packages with ForceImportantDeps=True
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); !I.end(); ++I)
	    if ((*DCache)[I].NowPolicyBroken() == true) 
	       DCache->MarkInstall(I,true,0, false, true);
      }
   }

   // Nothing is broken
   if (DCache->BrokenCount() == 0 || AllowBroken == true)
      return true;

   // Attempt to fix broken things
   if (FixBroken == true)
   {
      c1out << _("Correcting dependencies...") << flush;
      if (pkgFixBroken(*DCache) == false || DCache->BrokenCount() != 0)
      {
	 c1out << _(" failed.") << endl;
	 ShowBroken(c1out,*this,true);

	 return _error->Error(_("Unable to correct dependencies"));
      }
      if (pkgMinimizeUpgrade(*DCache) == false)
	 return _error->Error(_("Unable to minimize the upgrade set"));
      
      c1out << _(" Done") << endl;
   }
   else
   {
      c1out << _("You might want to run 'apt --fix-broken install' to correct these.") << endl;
      ShowBroken(c1out,*this,true);
      return _error->Error(_("Unmet dependencies. Try 'apt --fix-broken install' with no packages (or specify a solution)."));
   }
      
   return true;
}
									/*}}}*/

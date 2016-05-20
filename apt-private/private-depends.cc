// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-depends.h>

#include <iostream>
#include <string>
#include <vector>

#include <stddef.h>

#include <apti18n.h>
									/*}}}*/

// ShowDepends - Helper for printing out a dependency tree		/*{{{*/
static bool ShowDepends(CommandLine &CmdL, bool const RevDepends)
{
   pkgCacheFile CacheFile;
   pkgCache * const Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == nullptr || CacheFile.GetDepCache() == nullptr))
      return false;

   CacheSetHelperVirtuals helper(false);
   APT::VersionList verset = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1, APT::CacheSetHelper::CANDIDATE, helper);
   if (verset.empty() == true && helper.virtualPkgs.empty() == true)
      return _error->Error(_("No packages found"));
   std::vector<bool> Shown(Cache->Head().PackageCount);

   bool const Recurse = _config->FindB("APT::Cache::RecurseDepends", false);
   bool const Installed = _config->FindB("APT::Cache::Installed", false);
   bool const Important = _config->FindB("APT::Cache::Important", false);
   bool const ShowDepType = _config->FindB("APT::Cache::ShowDependencyType", RevDepends == false);
   bool const ShowVersion = _config->FindB("APT::Cache::ShowVersion", false);
   bool const ShowPreDepends = _config->FindB("APT::Cache::ShowPre-Depends", true);
   bool const ShowDepends = _config->FindB("APT::Cache::ShowDepends", true);
   bool const ShowRecommends = _config->FindB("APT::Cache::ShowRecommends", Important == false);
   bool const ShowSuggests = _config->FindB("APT::Cache::ShowSuggests", Important == false);
   bool const ShowReplaces = _config->FindB("APT::Cache::ShowReplaces", Important == false);
   bool const ShowConflicts = _config->FindB("APT::Cache::ShowConflicts", Important == false);
   bool const ShowBreaks = _config->FindB("APT::Cache::ShowBreaks", Important == false);
   bool const ShowEnhances = _config->FindB("APT::Cache::ShowEnhances", Important == false);
   bool const ShowOnlyFirstOr = _config->FindB("APT::Cache::ShowOnlyFirstOr", false);
   bool const ShowImplicit = _config->FindB("APT::Cache::ShowImplicit", false);

   while (verset.empty() != true)
   {
      pkgCache::VerIterator Ver = *verset.begin();
      verset.erase(verset.begin());
      pkgCache::PkgIterator Pkg = Ver.ParentPkg();
      Shown[Pkg->ID] = true;

      std::cout << Pkg.FullName(true) << std::endl;

      if (RevDepends == true)
	 std::cout << "Reverse Depends:" << std::endl;
      for (pkgCache::DepIterator D = RevDepends ? Pkg.RevDependsList() : Ver.DependsList();
	    D.end() == false; ++D)
      {
	 switch (D->Type) {
	    case pkgCache::Dep::PreDepends: if (!ShowPreDepends) continue; break;
	    case pkgCache::Dep::Depends: if (!ShowDepends) continue; break;
	    case pkgCache::Dep::Recommends: if (!ShowRecommends) continue; break;
	    case pkgCache::Dep::Suggests: if (!ShowSuggests) continue; break;
	    case pkgCache::Dep::Replaces: if (!ShowReplaces) continue; break;
	    case pkgCache::Dep::Conflicts: if (!ShowConflicts) continue; break;
	    case pkgCache::Dep::DpkgBreaks: if (!ShowBreaks) continue; break;
	    case pkgCache::Dep::Enhances: if (!ShowEnhances) continue; break;
	 }
	 if (ShowImplicit == false && D.IsImplicit())
	    continue;

	 pkgCache::PkgIterator Trg = RevDepends ? D.ParentPkg() : D.TargetPkg();

	 if((Installed && Trg->CurrentVer != 0) || !Installed)
	 {

	    if ((D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or && ShowOnlyFirstOr == false)
	       std::cout << " |";
	    else
	       std::cout << "  ";

	    // Show the package
	    if (ShowDepType == true)
	       std::cout << D.DepType() << ": ";
	    if (Trg->VersionList == 0)
	       std::cout << "<" << Trg.FullName(true) << ">";
	    else
	       std::cout << Trg.FullName(true);
	    if (ShowVersion == true && D->Version != 0)
	       std::cout << " (" << pkgCache::CompTypeDeb(D->CompareOp) << ' ' << D.TargetVer() << ')';
	    std::cout << std::endl;

	    if (Recurse == true && Shown[Trg->ID] == false)
	    {
	       Shown[Trg->ID] = true;
	       verset.insert(APT::VersionSet::FromPackage(CacheFile, Trg, APT::CacheSetHelper::CANDIDATE, helper));
	    }

	 }

	 // Display all solutions
	 std::unique_ptr<pkgCache::Version *[]> List(D.AllTargets());
	 pkgPrioSortList(*Cache,List.get());
	 for (pkgCache::Version **I = List.get(); *I != 0; I++)
	 {
	    pkgCache::VerIterator V(*Cache,*I);
	    if (V != Cache->VerP + V.ParentPkg()->VersionList ||
		  V->ParentPkg == D->Package)
	       continue;
	    std::cout << "    " << V.ParentPkg().FullName(true) << std::endl;

	    if (Recurse == true && Shown[V.ParentPkg()->ID] == false)
	    {
	       Shown[V.ParentPkg()->ID] = true;
	       verset.insert(APT::VersionSet::FromPackage(CacheFile, V.ParentPkg(), APT::CacheSetHelper::CANDIDATE, helper));
	    }
	 }

	 if (ShowOnlyFirstOr == true)
	    while ((D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or) ++D;
      }
   }

   for (APT::PackageSet::const_iterator Pkg = helper.virtualPkgs.begin();
	 Pkg != helper.virtualPkgs.end(); ++Pkg)
      std::cout << '<' << Pkg.FullName(true) << '>' << std::endl;

   return true;
}
									/*}}}*/
// Depends - Print out a dependency tree				/*{{{*/
bool Depends(CommandLine &CmdL)
{
   return ShowDepends(CmdL, false);
}
									/*}}}*/
// RDepends - Print out a reverse dependency tree			/*{{{*/
bool RDepends(CommandLine &CmdL)
{
   return ShowDepends(CmdL, true);
}
									/*}}}*/

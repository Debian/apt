#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/policy.h>

#include <apt-private/private-cacheset.h>

#include <stddef.h>

#include <apti18n.h>

bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,
                                 APT::VersionContainerInterface * const vci,
                                 OpProgress * const progress)
{
    Matcher null_matcher = Matcher();
    return GetLocalitySortedVersionSet(CacheFile, vci,
                                       null_matcher, progress);
}

bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile,
                                 APT::VersionContainerInterface * const vci,
                                 Matcher &matcher,
                                 OpProgress * const progress)
{
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   APT::CacheSetHelper helper(false);

   int Done=0;
   if (progress != NULL)
      progress->SubProgress(Cache->Head().PackageCount, _("Sorting"));

   bool const insertCurrentVer = _config->FindB("APT::Cmd::Installed", false);
   bool const insertUpgradable = _config->FindB("APT::Cmd::Upgradable", false);
   bool const insertManualInstalled = _config->FindB("APT::Cmd::Manual-Installed", false);

   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
   {
      if (progress != NULL)
      {
	 if (Done % 500 == 0)
	    progress->Progress(Done);
	 ++Done;
      }

      // exclude virtual pkgs
      if (P->VersionList == 0)
	 continue;

      if ((matcher)(P) == false)
	 continue;

      pkgDepCache::StateCache &state = (*DepCache)[P];
      if (insertCurrentVer == true)
      {
	 if (P->CurrentVer != 0)
	    vci->FromPackage(vci, CacheFile, P, APT::VersionContainerInterface::INSTALLED, helper);
      }
      else if (insertUpgradable == true)
      {
	 if(P.CurrentVer() && state.Upgradable())
	    vci->FromPackage(vci, CacheFile, P, APT::VersionContainerInterface::CANDIDATE, helper);
      }
      else if (insertManualInstalled == true)
      {
	 if (P.CurrentVer() &&
	       ((*DepCache)[P].Flags & pkgCache::Flag::Auto) == false)
	    vci->FromPackage(vci, CacheFile, P, APT::VersionContainerInterface::CANDIDATE, helper);
      }
      else
      {
         if (vci->FromPackage(vci, CacheFile, P, APT::VersionContainerInterface::CANDIDATE, helper) == false)
	 {
	    // no candidate, this may happen for packages in
	    // dpkg "deinstall ok config-file" state - we pick the first ver
	    // (which should be the only one)
	    vci->insert(P.VersionList());
	 }
      }
   }
   if (progress != NULL)
      progress->Done();
   return true;
}

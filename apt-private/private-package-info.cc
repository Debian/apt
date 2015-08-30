// Includes                      /*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/debversion.h>
#include <apt-private/private-output.h>
#include <apt-private/private-package-info.h>

#include <algorithm>
		  /*}}}*/


using namespace std;

PackageInfo::PackageInfo(pkgCacheFile &CacheFile, pkgRecords &records,
		       pkgCache::VerIterator const &V, std::string formated_output)
{
   _formated_output = formated_output;

   if(V)
   {
      pkgCache::PkgIterator const P = V.ParentPkg();
      _name = P.Name();
      _version = DeNull(V.VerStr());
      _description = GetShortDescription(CacheFile, records, P);
      _status = GetPackageStatus(CacheFile, V);
   }

}

// PackageInfo::GetPackageStatus - Populate the package status       /*{{{*/
// ---------------------------------------------------------------------
/* Returns the actual status of a package */
PackageInfo::PackageStatus 
PackageInfo::GetPackageStatus(pkgCacheFile &CacheFile,
		       pkgCache::VerIterator const &V)
{
   pkgCache::PkgIterator const P = V.ParentPkg();
   pkgDepCache * const DepCache = CacheFile.GetDepCache();
   pkgDepCache::StateCache const &state = (*DepCache)[P];

   PackageStatus Status = UNINSTALLED;
   if (P->CurrentVer != 0)
   {
      if (P.CurrentVer() == V)
      {
	 if (state.Upgradable() && state.CandidateVer != NULL)
	    Status = INSTALLED_UPGRADABLE;
	 else if (V.Downloadable() == false)
	    Status = INSTALLED_LOCAL;
	 else if(V.Automatic() == true && state.Garbage == true)
	    Status = INSTALLED_AUTO_REMOVABLE;
	 else if ((state.Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
	    Status = INSTALLED_AUTOMATIC;
	 else
	    Status = INSTALLED;
      }
      else if (state.CandidateVer == V && state.Upgradable())
      Status = UPGRADABLE;
   }
   else if (V.ParentPkg()->CurrentState == pkgCache::State::ConfigFiles)
      Status = RESIDUAL_CONFIG;

    return Status;
}
		  /*}}}*/

PackageInfo::SortBy 
PackageInfo::getOrderByOption ()
{
   std::string inString = _config->Find("APT::Cache::OrderBy","Alphabetic");
   std::transform(inString.begin(), inString.end(), inString.begin(), ::tolower);
   if (inString == "alphabetic") return PackageInfo::ALPHABETIC;
   else if (inString == "reverse") return PackageInfo::REVERSEALPHABETIC;
   else if (inString == "reverse alphabetic") return PackageInfo::REVERSEALPHABETIC;
   else if (inString == "version") return PackageInfo::VERSION;
   else if (inString == "status") return PackageInfo::STATUS;
   return PackageInfo::ALPHABETIC;
}

bool OrderByStatus (const PackageInfo &a, const PackageInfo &b)
{
   if(a.status() == b.status())
      return a.name() < b.name();
   else
      return a.status() < b.status();
}

bool OrderByAlphabetic (const PackageInfo &a, const PackageInfo &b)
{
   return a.name() < b.name();
}

bool OrderByVersion (const PackageInfo &a, const PackageInfo &b)
{
   int result = debVS.CmpVersion(a.version(),b.version());
   if(result > 0)
      return true;
   else
      return false;
}

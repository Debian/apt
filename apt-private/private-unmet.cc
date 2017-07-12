// -*- mode: cpp; mode: fold -*-
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-unmet.h>

#include <stddef.h>

#include <iostream>

#include <apti18n.h>
									/*}}}*/

// UnMet - Show unmet dependencies					/*{{{*/
static bool ShowUnMet(pkgCache::VerIterator const &V, bool const Important)
{
	 bool Header = false;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false;)
	 {
	    // Collect or groups
	    pkgCache::DepIterator Start;
	    pkgCache::DepIterator End;
	    D.GlobOr(Start,End);

	    // Important deps only
	    if (Important == true)
	       if (End->Type != pkgCache::Dep::PreDepends &&
	           End->Type != pkgCache::Dep::Depends)
		  continue;

	    // Skip conflicts and replaces
	    if (End.IsNegative() == true || End->Type == pkgCache::Dep::Replaces)
	       continue;

	    // Verify the or group
	    bool OK = false;
	    pkgCache::DepIterator RealStart = Start;
	    do
	    {
	       // See if this dep is Ok
	       pkgCache::Version **VList = Start.AllTargets();
	       if (*VList != 0)
	       {
		  OK = true;
		  delete [] VList;
		  break;
	       }
	       delete [] VList;

	       if (Start == End)
		  break;
	       ++Start;
	    }
	    while (1);

	    // The group is OK
	    if (OK == true)
	       continue;

	    // Oops, it failed..
	    if (Header == false)
	       ioprintf(std::cout,_("Package %s version %s has an unmet dep:\n"),
			V.ParentPkg().FullName(true).c_str(),V.VerStr());
	    Header = true;

	    // Print out the dep type
	    std::cout << " " << End.DepType() << ": ";

	    // Show the group
	    Start = RealStart;
	    do
	    {
	       std::cout << Start.TargetPkg().FullName(true);
	       if (Start.TargetVer() != 0)
		  std::cout << " (" << Start.CompType() << " " << Start.TargetVer() <<
		  ")";
	       if (Start == End)
		  break;
	       std::cout << " | ";
	       ++Start;
	    }
	    while (1);

	    std::cout << std::endl;
	 }
   return true;
}
bool UnMet(CommandLine &CmdL)
{
   bool const Important = _config->FindB("APT::Cache::Important",false);

   pkgCacheFile CacheFile;
   if (unlikely(CacheFile.GetPkgCache() == NULL))
      return false;

   if (CmdL.FileSize() <= 1)
   {
      for (pkgCache::PkgIterator P = CacheFile.GetPkgCache()->PkgBegin(); P.end() == false; ++P)
	 for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
	    if (ShowUnMet(V, Important) == false)
	       return false;
   }
   else
   {
      CacheSetHelperVirtuals helper(true, GlobalError::NOTICE);
      APT::VersionList verset = APT::VersionList::FromCommandLine(CacheFile, CmdL.FileList + 1,
				APT::CacheSetHelper::CANDIDATE, helper);
      for (APT::VersionList::iterator V = verset.begin(); V != verset.end(); ++V)
	 if (ShowUnMet(V, Important) == false)
	    return false;
   }
   return true;
}
									/*}}}*/

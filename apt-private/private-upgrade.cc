
#include <apt-pkg/algorithms.h>

#include "private-install.h"
#include "private-cachefile.h"
#include "private-upgrade.h"
#include "private-output.h"


// DoUpgradeNoNewPackages - Upgrade all packages        		/*{{{*/
// ---------------------------------------------------------------------
/* Upgrade all packages without installing new packages or erasing old
   packages */
bool DoUpgradeNoNewPackages(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   // Do the upgrade
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, AllUpgrade broke stuff"));
   }
   
   return InstallPackages(Cache,true);
}
									/*}}}*/

// DoSafeUpgrade - Upgrade all packages with install but not remove	/*{{{*/
bool DoUpgradeWithAllowNewPackages(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   // Do the upgrade
   if (pkgAllUpgradeNoDelete(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, AllUpgrade broke stuff"));
   }
   
   return InstallPackages(Cache,true);
}
									/*}}}*/

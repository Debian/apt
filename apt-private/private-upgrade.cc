// Includes								/*{{{*/
#include <apt-pkg/algorithms.h>
#include <apt-pkg/upgrade.h>
#include <iostream>
#include "private-install.h"
#include "private-cachefile.h"
#include "private-upgrade.h"
#include "private-output.h"
									/*}}}*/

// this is actually performing the various upgrade operations 
static bool UpgradeHelper(CommandLine &CmdL, int UpgradeFlags)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   c0out << _("Calculating upgrade... ") << std::flush;
   if (APT::Upgrade::Upgrade(Cache, UpgradeFlags) == false)
   {
      c0out << _("Failed") << std::endl;
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, Upgrade broke stuff"));
   }
   c0out << _("Done") << std::endl;

   // parse additional cmdline pkg manipulation switches
   if(!DoCacheManipulationFromCommandLine(CmdL, Cache))
      return false;
   
   return InstallPackages(Cache,true);
}

// DoDistUpgrade - Automatic smart upgrader				/*{{{*/
// ---------------------------------------------------------------------
/* Intelligent upgrader that will install and remove packages at will */
bool DoDistUpgrade(CommandLine &CmdL)
{
   return UpgradeHelper(CmdL, 0);
}
									/*}}}*/
// DoUpgradeNoNewPackages - Upgrade all packages			/*{{{*/
// ---------------------------------------------------------------------
/* Upgrade all packages without installing new packages or erasing old
   packages */
bool DoUpgradeNoNewPackages(CommandLine &CmdL)
{
   // Do the upgrade
   return UpgradeHelper(CmdL, 
                        APT::Upgrade::FORBID_REMOVE_PACKAGES|
                        APT::Upgrade::FORBID_INSTALL_NEW_PACKAGES);
}
									/*}}}*/
// DoSafeUpgrade - Upgrade all packages with install but not remove	/*{{{*/
bool DoUpgradeWithAllowNewPackages(CommandLine &CmdL)
{
   return UpgradeHelper(CmdL, APT::Upgrade::FORBID_REMOVE_PACKAGES);
}
									/*}}}*/

// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/upgrade.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

#include <apt-private/private-install.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-upgrade.h>
#include <apt-private/private-output.h>

#include <iostream>

#include <apti18n.h>
									/*}}}*/

// this is actually performing the various upgrade operations 
static bool UpgradeHelper(CommandLine &CmdL, int UpgradeFlags)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   c0out << _("Calculating upgrade... ") << std::flush;
   if(!DoCacheManipulationFromCommandLine(CmdL, Cache, UpgradeFlags))
      return false;
   c0out << _("Done") << std::endl;

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
bool DoUpgrade(CommandLine &CmdL)					/*{{{*/
{
   if (_config->FindB("APT::Get::Upgrade-Allow-New", false) == true)
      return DoUpgradeWithAllowNewPackages(CmdL);
   else
      return DoUpgradeNoNewPackages(CmdL);
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

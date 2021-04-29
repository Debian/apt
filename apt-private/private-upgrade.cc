// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/upgrade.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-install.h>
#include <apt-private/private-json-hooks.h>
#include <apt-private/private-output.h>
#include <apt-private/private-upgrade.h>

#include <iostream>

#include <apti18n.h>
									/*}}}*/

// this is actually performing the various upgrade operations 
static bool UpgradeHelper(CommandLine &CmdL, int UpgradeFlags)
{
   CacheFile Cache;
   auto VolatileCmdL = GetPseudoPackages(Cache.GetSourceList(), CmdL, AddVolatileBinaryFile, "");

   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;

   std::map<unsigned short, APT::VersionSet> verset;
   std::set<std::string> UnknownPackages;
   APT::PackageVector HeldBackPackages;
   if (not DoCacheManipulationFromCommandLine(CmdL, VolatileCmdL, Cache, verset, UpgradeFlags, UnknownPackages, HeldBackPackages))
   {
      RunJsonHook("AptCli::Hooks::Upgrade", "org.debian.apt.hooks.install.fail", CmdL.FileList, Cache, UnknownPackages);
      return false;
   }
   RunJsonHook("AptCli::Hooks::Upgrade", "org.debian.apt.hooks.install.pre-prompt", CmdL.FileList, Cache);
   if (InstallPackages(Cache, HeldBackPackages, true, true, true, "AptCli::Hooks::Upgrade", CmdL))
      return RunJsonHook("AptCli::Hooks::Upgrade", "org.debian.apt.hooks.install.post", CmdL.FileList, Cache);
   else
      return RunJsonHook("AptCli::Hooks::Upgrade", "org.debian.apt.hooks.install.fail", CmdL.FileList, Cache);
}

// DoDistUpgrade - Automatic smart upgrader				/*{{{*/
// ---------------------------------------------------------------------
/* Intelligent upgrader that will install and remove packages at will */
bool DoDistUpgrade(CommandLine &CmdL)
{
   return UpgradeHelper(CmdL, APT::Upgrade::ALLOW_EVERYTHING);
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   apt - CLI UI for apt
   
   Returns 100 on failure, 0 on success.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-depends.h>
#include <apt-private/private-download.h>
#include <apt-private/private-install.h>
#include <apt-private/private-list.h>
#include <apt-private/private-main.h>
#include <apt-private/private-moo.h>
#include <apt-private/private-output.h>
#include <apt-private/private-search.h>
#include <apt-private/private-show.h>
#include <apt-private/private-source.h>
#include <apt-private/private-sources.h>
#include <apt-private/private-update.h>
#include <apt-private/private-upgrade.h>

#include <iostream>
#include <vector>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
      _("Usage: apt [options] command\n"
	    "\n"
	    "apt is a commandline package manager and provides commands for\n"
	    "searching and managing as well as querying information about packages.\n"
	    "It provides the same functionality as the specialized APT tools,\n"
	    "like apt-get and apt-cache, but enables options more suitable for\n"
	    "interactive use by default.\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      // query
      {"list", &DoList, _("list packages based on package names")},
      {"search", &DoSearch, _("search in package descriptions")},
      {"show", &ShowPackage, _("show package details")},

      // package stuff
      {"install", &DoInstall, _("install packages")},
      {"reinstall", &DoInstall, _("reinstall packages")},
      {"remove", &DoInstall, _("remove packages")},
      {"autoremove", &DoInstall, _("Remove automatically all unused packages")},
      {"auto-remove", &DoInstall, nullptr},
      {"autopurge",&DoInstall, nullptr},
      {"purge", &DoInstall, nullptr},

      // system wide stuff
      {"update", &DoUpdate, _("update list of available packages")},
      {"upgrade", &DoUpgrade, _("upgrade the system by installing/upgrading packages")},
      {"full-upgrade", &DoDistUpgrade, _("upgrade the system by removing/installing/upgrading packages")},

      // misc
      {"edit-sources", &EditSources, _("edit the source information file")},
      {"moo", &DoMoo, nullptr},
      {"satisfy", &DoBuildDep, _("satisfy dependency strings")},

      // for compat with muscle memory
      {"dist-upgrade", &DoDistUpgrade, nullptr},
      {"showsrc",&ShowSrcPackage, nullptr},
      {"depends",&Depends, nullptr},
      {"rdepends",&RDepends, nullptr},
      {"policy",&Policy, nullptr},
      {"build-dep", &DoBuildDep,nullptr},
      {"clean", &DoClean, nullptr},
      {"autoclean", &DoAutoClean, nullptr},
      {"auto-clean", &DoAutoClean, nullptr},
      {"source", &DoSource, nullptr},
      {"download", &DoDownload, nullptr},
      {"changelog", &DoChangelog, nullptr},
      {"info", &ShowPackage, nullptr},

      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc, const char *argv[])					/*{{{*/
{
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);

   int const quiet = _config->FindI("quiet", 0);
   if (quiet == 2)
   {
      _config->CndSet("quiet::NoProgress", true);
      _config->Set("quiet", 1);
   }

   InitSignals();
   InitOutput();

   CheckIfCalledByScript(argc, argv);
   CheckIfSimulateMode(CmdL);

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/

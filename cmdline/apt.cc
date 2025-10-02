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
#include <apt-pkg/solver3.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cmndline.h>
#include <apt-private/private-depends.h>
#include <apt-private/private-download.h>
#include <apt-private/private-history.h>
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

static bool DoWhy(CommandLine &CmdL) /*{{{*/
{
   pkgCacheFile CacheFile;
   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1);
   bool const decision = strcmp(CmdL.FileList[0], "why") == 0;
   if (pkgset.size() != 1)
      return _error->PendingError() ? false : _error->Error("Only a single argument is supported at this time.");
   if (unlikely(not CacheFile.BuildDepCache()))
      return false;
   for (auto pkg : pkgset)
      std::cout << APT::Solver::InternalCliWhy(CacheFile, pkg, decision) << std::flush;
   return not _error->PendingError();
}
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   // advanced commands are left undocumented on purpose
   return {
      // query
      {"list", &DoList, _("list packages based on package names")},
      {"search", &DoSearch, _("search in package descriptions")},
      {"show", &ShowPackage, _("show package details")},

      // package stuff
      {"install", &DoInstall, _("install packages")},
      {"reinstall", &DoInstall, _("reinstall packages")},
      {"remove", &DoInstall, _("remove packages")},
      {"autoremove", &DoInstall, _("automatically remove all unused packages")},
      {"auto-remove", &DoInstall, nullptr},
      {"autopurge", &DoInstall, nullptr},
      {"purge", &DoInstall, nullptr},

      // system wide stuff
      {"update", &DoUpdate, _("update list of available packages")},
      {"upgrade", &DoUpgrade, _("upgrade the system by installing/upgrading packages")},
      {"full-upgrade", &DoDistUpgrade, _("upgrade the system by removing/installing/upgrading packages")},

      // history stuff
      {"history-list", &DoHistoryList, _("show list of history")},
      {"history-info", &DoHistoryInfo, _("show info on specific transactions")},
      {"history-redo", &DoHistoryRedo, _("redo transactions")},
      {"history-undo", &DoHistoryUndo, _("undo transactions")},
      {"history-rollback", &DoHistoryRollback, _("rollback transactions")},

      // misc
      {"edit-sources", &EditSources, _("edit the source information file")},
      {"modernize-sources", &ModernizeSources, _("modernize .list files to .sources files")},
      {"moo", &DoMoo, nullptr},
      {"satisfy", &DoBuildDep, _("satisfy dependency strings")},
      {"why", &DoWhy, _("produce a reason trace for the current state of the package")},
      {"why-not", &DoWhy, _("produce a reason trace for the current state of the package")},

      // for compat with muscle memory
      {"dist-upgrade", &DoDistUpgrade, nullptr},
      {"showsrc", &ShowSrcPackage, nullptr},
      {"depends", &Depends, nullptr},
      {"rdepends", &RDepends, nullptr},
      {"policy", &Policy, nullptr},
      {"build-dep", &DoBuildDep, nullptr},
      {"clean", &DoClean, nullptr},
      {"distclean", &DoDistClean, nullptr},
      {"dist-clean", &DoDistClean, nullptr},
      {"autoclean", &DoAutoClean, nullptr},
      {"auto-clean", &DoAutoClean, nullptr},
      {"source", &DoSource, nullptr},
      {"download", &DoDownload, nullptr},
      {"changelog", &DoChangelog, nullptr},
      {"info", &ShowPackage, nullptr},

      {nullptr, nullptr, nullptr}};
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

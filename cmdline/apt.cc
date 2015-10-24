// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   apt - CLI UI for apt
   
   Returns 100 on failure, 0 on success.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>

#include <apt-private/private-list.h>
#include <apt-private/private-search.h>
#include <apt-private/private-install.h>
#include <apt-private/private-output.h>
#include <apt-private/private-update.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-moo.h>
#include <apt-private/private-upgrade.h>
#include <apt-private/private-show.h>
#include <apt-private/private-main.h>
#include <apt-private/private-sources.h>

#include <unistd.h>
#include <iostream>
#include <vector>

#include <apti18n.h>
									/*}}}*/

static bool ShowHelp(CommandLine &, CommandLine::DispatchWithHelp const * Cmds)
{
   ioprintf(c1out, "%s %s (%s)\n", PACKAGE, PACKAGE_VERSION, COMMON_ARCH);

   // FIXME: generate from CommandLine
   c1out <<
    _("Usage: apt [options] command\n"
      "\n"
      "CLI for apt.\n")
    << std::endl
    << _("Commands:") << std::endl;
   for (; Cmds->Handler != nullptr; ++Cmds)
   {
      if (Cmds->Help == nullptr)
	 continue;
      std::cout << "  " << Cmds->Match << " - " << Cmds->Help << std::endl;
   }

   return true;
}

int main(int argc, const char *argv[])					/*{{{*/
{
   InitLocale();

   CommandLine::DispatchWithHelp Cmds[] = {
      // query
      {"list", &DoList, _("list packages based on package names")},
      {"search", &DoSearch, _("search in package descriptions")},
      {"show", &ShowPackage, _("show package details")},

      // package stuff
      {"install", &DoInstall, _("install packages")},
      {"remove", &DoInstall, _("remove packages")},
      {"autoremove", &DoInstall, _("Remove automatically all unused packages")},
      {"auto-remove", &DoInstall, nullptr},
      {"purge", &DoInstall, nullptr},

      // system wide stuff
      {"update", &DoUpdate, _("update list of available packages")},
      {"upgrade", &DoUpgrade, _("upgrade the system by installing/upgrading packages")},
      {"full-upgrade", &DoDistUpgrade, _("upgrade the system by removing/installing/upgrading packages")},
      {"dist-upgrade", &DoDistUpgrade, nullptr}, // for compat with muscle memory

      // misc
      {"edit-sources", &EditSources, _("edit the source information file")},
      {"moo", &DoMoo, nullptr},
      {nullptr, nullptr, nullptr}
   };

   // FIXME: Those ignore commandline configuration like -q
   InitSignals();
   InitOutput();

   CommandLine CmdL;
   ParseCommandLine(CmdL, Cmds, "apt", &_config, &_system, argc, argv, ShowHelp);

   CheckIfCalledByScript(argc, argv);
   CheckIfSimulateMode(CmdL);

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/

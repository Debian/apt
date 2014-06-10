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

static bool ShowHelp(CommandLine &)
{
   ioprintf(c1out,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);

   // FIXME: generate from CommandLine
   c1out << 
    _("Usage: apt [options] command\n"
      "\n"
      "CLI for apt.\n"
      "Basic commands: \n"
      " list - list packages based on package names\n"
      " search - search in package descriptions\n"
      " show - show package details\n"
      "\n"
      " update - update list of available packages\n"
      "\n"
      " install - install packages\n"
      " remove  - remove packages\n"
      "\n"
      " upgrade - upgrade the system by installing/upgrading packages\n"
      " full-upgrade - upgrade the system by removing/installing/upgrading packages\n"
      "\n"
      " edit-sources - edit the source information file\n"
       );
   
   return true;
}

int main(int argc, const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {
                                   // query
                                   {"list",&DoList},
                                   {"search", &FullTextSearch},
                                   {"show", &APT::Cmd::ShowPackage},

                                   // package stuff
                                   {"install",&DoInstall},
                                   {"remove", &DoInstall},
                                   {"purge", &DoInstall},

                                   // system wide stuff
                                   {"update",&DoUpdate},
                                   {"upgrade",&DoUpgrade},
                                   {"full-upgrade",&DoDistUpgrade},
                                   // for compat with muscle memory
                                   {"dist-upgrade",&DoDistUpgrade},

                                   // misc
                                   {"edit-sources",&EditSources},

                                   // helper
                                   {"moo",&DoMoo},
                                   {"help",&ShowHelp},
                                   {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs("apt", CommandLine::GetCommand(Cmds, argc, argv));

   // Init the signals
   InitSignals();

   // Init the output
   InitOutput();

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

    if(pkgInitConfig(*_config) == false) 
    {
        _error->DumpErrors();
        return 100;
    }

    // some different defaults
   _config->CndSet("DPkg::Progress-Fancy", "1");
   _config->CndSet("Apt::Color", "1");
   _config->CndSet("APT::Get::Upgrade-Allow-New", true);
   _config->CndSet("APT::Cmd::Show-Update-Stats", true);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(), _config);
   if (CmdL.Parse(argc, argv) == false ||
       pkgInitSystem(*_config, _system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   if(!isatty(STDOUT_FILENO) && 
      _config->FindB("Apt::Cmd::Disable-Script-Warning", false) == false)
   {
      std::cerr << std::endl
                << "WARNING: " << argv[0] << " "
                << "does not have a stable CLI interface yet. "
                << "Use with caution in scripts."
                << std::endl
                << std::endl;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
   {
      ShowHelp(CmdL);
      return 0;
   }

   // see if we are in simulate mode
   CheckSimulateMode(CmdL);

   // parse args
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   return Errors == true ? 100 : 0;
}
									/*}}}*/

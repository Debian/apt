// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   apt - CLI UI for apt
   
   Returns 100 on failure, 0 on success.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <cassert>
#include <locale.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <iomanip>
#include <algorithm>


#include <apt-pkg/error.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/init.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/hashes.h>

#include <apti18n.h>

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
#include <apt-private/private-utils.h>
#include <apt-private/private-sources.h>
									/*}}}*/



bool ShowHelp(CommandLine &CmdL)
{
   ioprintf(c1out,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);

   // FIXME: generate from CommandLine
   c1out << 
    _("Usage: apt [options] command\n"
      "\n"
      "CLI for apt.\n"
      "Commands: \n"
      " list - list packages based on package names\n"
      " search - search in package descriptions\n"
      " show - show package details\n"
      "\n"
      " update - update list of available packages\n"
      " install - install packages\n"
      " upgrade - upgrade the systems packages\n"
      "\n"
      " edit-sources - edit the source information file\n"
       );
   
   return true;
}

// figure out what kind of upgrade the user wants
bool DoAptUpgrade(CommandLine &CmdL)
{
   if (_config->FindB("Apt::Cmd::Dist-Upgrade"))
      return DoDistUpgrade(CmdL);
   else
      return DoUpgradeWithAllowNewPackages(CmdL);
}

int main(int argc, const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {{"list",&List},
                                   {"search", &FullTextSearch},
                                   {"show", &APT::Cmd::ShowPackage},
                                   // needs root
                                   {"install",&DoInstall},
                                   {"remove", &DoInstall},
                                   {"update",&DoUpdate},
                                   {"upgrade",&DoAptUpgrade},
                                   // misc
                                   {"edit-sources",&EditSources},
                                   // helper
                                   {"moo",&DoMoo},
                                   {"help",&ShowHelp},
                                   {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs("apt", CommandLine::GetCommand(Cmds, argc, argv));

   if(!isatty(1)) 
   {
      std::cerr << std::endl
                << "WARNING WARNING "
                << argv[0]
                << " is *NOT* intended for scripts "
                << "use at your own peril^Wrisk"
                << std::endl
                << std::endl;
   }

   InitOutput();

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

    if(pkgInitConfig(*_config) == false) 
    {
        _error->DumpErrors();
        return 100;
    }

   // FIXME: move into a new libprivate/private-install.cc:Install()
   _config->Set("DPkgPM::Progress", "1");
   _config->Set("Apt::Color", "1");

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(), _config);
   if (CmdL.Parse(argc, argv) == false ||
       pkgInitSystem(*_config, _system) == false)
   {
      _error->DumpErrors();
      return 100;
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################
   apt-helper - cmdline helpers
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-output.h>
#include <apt-private/private-download.h>
#include <apt-private/private-cmndline.h>

#include <iostream>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

static bool DoDownloadFile(CommandLine &CmdL)
{
   if (CmdL.FileSize() <= 2)
      return _error->Error(_("Must specify at least one pair url/filename"));


   pkgAcquire Fetcher;
   AcqTextStatus Stat(ScreenWidth, _config->FindI("quiet",0));
   Fetcher.Setup(&Stat);
   std::string download_uri = CmdL.FileList[1];
   std::string targetfile = CmdL.FileList[2];
   std::string hash;
   if (CmdL.FileSize() > 3)
      hash = CmdL.FileList[3];
   // we use download_uri as descr and targetfile as short-descr
   new pkgAcqFile(&Fetcher, download_uri, hash, 0, download_uri, targetfile, 
                  "dest-dir-ignored", targetfile);
   Fetcher.Run();
   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true ||
	 FileExists(targetfile) == false)
      return _error->Error(_("Download Failed"));
   return true;
}

static bool ShowHelp(CommandLine &)
{
   ioprintf(std::cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);

   if (_config->FindB("version") == true)
     return true;

   std::cout <<
    _("Usage: apt-helper [options] command\n"
      "       apt-helper [options] download-file uri target-path\n"
      "\n"
      "apt-helper is a internal helper for apt\n"
      "\n"
      "Commands:\n"
      "   download-file - download the given uri to the target-path\n"
      "\n"
      "                       This APT helper has Super Meep Powers.\n");
   return true;
}


int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {{"help",&ShowHelp},
				   {"download-file", &DoDownloadFile},
                                   {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs(
      "apt-download", CommandLine::GetCommand(Cmds, argc, argv));

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(),_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      if (_config->FindB("version") == true)
	 ShowHelp(CmdL);
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

   InitOutput();

   // Match the operation
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

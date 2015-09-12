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
#include <apt-pkg/proxy.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-output.h>
#include <apt-private/private-download.h>
#include <apt-private/private-cmndline.h>
#include <apt-pkg/srvrec.h>

#include <iostream>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

static bool DoAutoDetectProxy(CommandLine &CmdL)
{
   if (CmdL.FileSize() != 2)
      return _error->Error(_("Need one URL as argument"));
   URI ServerURL(CmdL.FileList[1]);
   AutoDetectProxy(ServerURL);
   std::string SpecificProxy = _config->Find("Acquire::"+ServerURL.Access+"::Proxy::" + ServerURL.Host);
   ioprintf(std::cout, "Using proxy '%s' for URL '%s'\n",
            SpecificProxy.c_str(), std::string(ServerURL).c_str());

   return true;
}

static bool DoDownloadFile(CommandLine &CmdL)
{
   if (CmdL.FileSize() <= 2)
      return _error->Error(_("Must specify at least one pair url/filename"));

   AcqTextStatus Stat(std::cout, ScreenWidth,_config->FindI("quiet",0));
   pkgAcquire Fetcher(&Stat);

   size_t fileind = 0;
   std::vector<std::string> targetfiles;
   while (fileind + 2 <= CmdL.FileSize())
   {
      std::string download_uri = CmdL.FileList[fileind + 1];
      std::string targetfile = CmdL.FileList[fileind + 2];
      std::string hash;
      if (CmdL.FileSize() > fileind + 3)
	 hash = CmdL.FileList[fileind + 3];
      // we use download_uri as descr and targetfile as short-descr
      new pkgAcqFile(&Fetcher, download_uri, hash, 0, download_uri, targetfile,
	    "dest-dir-ignored", targetfile);
      targetfiles.push_back(targetfile);
      fileind += 3;
   }

   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true)
      return _error->Error(_("Download Failed"));
   if (targetfiles.empty() == false)
      for (std::vector<std::string>::const_iterator f = targetfiles.begin(); f != targetfiles.end(); ++f)
	 if (FileExists(*f) == false)
	    return _error->Error(_("Download Failed"));

   return true;
}

static bool DoSrvLookup(CommandLine &CmdL)
{
   if (CmdL.FileSize() <= 1)
      return _error->Error("Must specify at least one SRV record");

   for(size_t i = 1; CmdL.FileList[i] != NULL; ++i)
   {
      std::vector<SrvRec> srv_records;
      std::string const name = CmdL.FileList[i];
      c0out << "# Target\tPriority\tWeight\tPort # for " << name << std::endl;
      size_t const found = name.find(":");
      if (found != std::string::npos)
      {
	 std::string const host = name.substr(0, found);
	 size_t const port = atoi(name.c_str() + found + 1);
	 if(GetSrvRecords(host, port, srv_records) == false)
	    _error->Error(_("GetSrvRec failed for %s"), name.c_str());
      }
      else if(GetSrvRecords(name, srv_records) == false)
	 _error->Error(_("GetSrvRec failed for %s"), name.c_str());

      for (SrvRec const &I : srv_records)
	 c1out << I.target << "\t" << I.priority << "\t" << I.weight << "\t" << I.port << std::endl;
   }
   return true;
}

static bool ShowHelp(CommandLine &)
{
   ioprintf(std::cout, "%s %s (%s)\n", PACKAGE, PACKAGE_VERSION, COMMON_ARCH);

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
      "   srv-lookup - lookup a SRV record (e.g. _http._tcp.ftp.debian.org)\n"
      "   auto-detect-proxy - detect proxy using apt.conf\n"
      "\n"
      "                       This APT helper has Super Meep Powers.\n");
   return true;
}


int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {{"help",&ShowHelp},
				   {"download-file", &DoDownloadFile},
				   {"srv-lookup", &DoSrvLookup},
				   {"auto-detect-proxy", &DoAutoDetectProxy},
                                   {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs(
      "apt-download", CommandLine::GetCommand(Cmds, argc, argv));

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL;
   ParseCommandLine(CmdL, Cmds, Args.data(), &_config, &_system, argc, argv, ShowHelp);

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

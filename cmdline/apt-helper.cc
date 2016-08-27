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
#include <apt-private/private-main.h>
#include <apt-pkg/srvrec.h>

#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>
#include <stdlib.h>

#include <apti18n.h>
									/*}}}*/

static bool DoAutoDetectProxy(CommandLine &CmdL)			/*{{{*/
{
   if (CmdL.FileSize() != 2)
      return _error->Error(_("Need one URL as argument"));
   URI ServerURL(CmdL.FileList[1]);
   if (AutoDetectProxy(ServerURL) == false)
      return false;
   std::string SpecificProxy = _config->Find("Acquire::"+ServerURL.Access+"::Proxy::" + ServerURL.Host);
   ioprintf(std::cout, "Using proxy '%s' for URL '%s'\n",
            SpecificProxy.c_str(), std::string(ServerURL).c_str());

   return true;
}
									/*}}}*/
static bool DoDownloadFile(CommandLine &CmdL)				/*{{{*/
{
   if (CmdL.FileSize() <= 2)
      return _error->Error(_("Must specify at least one pair url/filename"));

   aptAcquireWithTextStatus Fetcher;
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
									/*}}}*/
static bool DoSrvLookup(CommandLine &CmdL)				/*{{{*/
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
	 ioprintf(c1out, "%s\t%d\t%d\t%d\n", I.target.c_str(), I.priority, I.weight, I.port);
   }
   return true;
}
									/*}}}*/
static const APT::Configuration::Compressor *FindCompressor(std::vector<APT::Configuration::Compressor> const & compressors, std::string name)				/*{{{*/
{
   APT::Configuration::Compressor const * compressor = NULL;
   for (auto const & c : compressors)
   {
      if (compressor != NULL && c.Cost >= compressor->Cost)
         continue;
      if (c.Name == name || c.Extension == name || (!c.Extension.empty() && c.Extension.substr(1) == name))
         compressor = &c;
   }

   return compressor;
}
									/*}}}*/
static bool DoCatFile(CommandLine &CmdL)				/*{{{*/
{
   FileFd fd;
   FileFd out;
   std::string const compressorName = _config->Find("Apt-Helper::Cat-File::Compress", "");

   if (compressorName.empty() == false)
   {

      auto const compressors = APT::Configuration::getCompressors();
      auto const compressor = FindCompressor(compressors, compressorName);

      if (compressor == NULL)
         return _error->Error("Could not find compressor: %s", compressorName.c_str());

      if (out.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly, *compressor) == false)
         return false;
   } else
   {
      if (out.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly) == false)
         return false;
   }

   if (CmdL.FileSize() <= 1)
   {
      if (fd.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false)
	 return false;
      if (CopyFile(fd, out) == false)
         return false;
      return true;
   }

   for(size_t i = 1; CmdL.FileList[i] != NULL; ++i)
   {
      std::string const name = CmdL.FileList[i];

      if (name != "-")
      {
	 if (fd.Open(name, FileFd::ReadOnly, FileFd::Extension) == false)
	    return false;
      }
      else
      {
	 if (fd.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false)
	    return false;
      }

      if (CopyFile(fd, out) == false)
         return false;
   }
   return true;
}
									/*}}}*/
static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
      _("Usage: apt-helper [options] command\n"
	    "       apt-helper [options] cat-file file ...\n"
	    "       apt-helper [options] download-file uri target-path\n"
	    "\n"
	    "apt-helper bundles a variety of commands for shell scripts to use\n"
	    "e.g. the same proxy configuration or acquire system as APT would.\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {"download-file", &DoDownloadFile, _("download the given uri to the target-path")},
      {"srv-lookup", &DoSrvLookup, _("lookup a SRV record (e.g. _http._tcp.ftp.debian.org)")},
      {"cat-file", &DoCatFile, _("concatenate files, with automatic decompression")},
      {"auto-detect-proxy", &DoAutoDetectProxy, _("detect proxy using apt.conf")},
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_HELPER, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);

   InitOutput();

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/

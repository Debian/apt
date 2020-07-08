// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* #####################################################################
   apt-helper - cmdline helpers
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/cachefilter-patterns.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/proxy.h>
#include <apt-pkg/strutl.h>

#include <apt-pkg/srvrec.h>
#include <apt-private/acqprogress.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-download.h>
#include <apt-private/private-main.h>
#include <apt-private/private-output.h>

#include <iostream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
      HashStringList hashes;

      fileind += 2;

      // An empty string counts as a hash for compatibility reasons
      if (CmdL.FileSize() > fileind + 1 && *CmdL.FileList[fileind + 1] == '\0')
	 fileind++;

      /* Let's start looking for hashes */
      for (auto i = fileind + 1; CmdL.FileSize() > i; i++)
      {
	 bool isAHash = false;

	 for (auto HashP = HashString::SupportedHashes(); *HashP != nullptr; HashP++)
	 {
	    if (APT::String::Startswith(CmdL.FileList[i], *HashP))
	       isAHash = true;
	 }

	 if (!isAHash)
	    break;

	 hashes.push_back(HashString(CmdL.FileList[i]));
	 fileind++;
      }

      // we use download_uri as descr and targetfile as short-descr
      new pkgAcqFile(&Fetcher, download_uri, hashes, 0, download_uri, targetfile,
		     "dest-dir-ignored", targetfile);
      targetfiles.push_back(targetfile);
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
static const APT::Configuration::Compressor *FindCompressor(std::vector<APT::Configuration::Compressor> const &compressors, std::string const &name) /*{{{*/
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

static pid_t ExecuteProcess(const char *Args[])				/*{{{*/
{
   pid_t pid = ExecFork();
   if (pid == 0)
   {
      execvp(Args[0], (char **)Args);
      _exit(100);
   }
   return pid;
}

static bool ServiceIsActive(const char *service)
{
   const char *argv[] = {"systemctl", "is-active", "-q", service, nullptr};
   pid_t pid = ExecuteProcess(argv);
   return ExecWait(pid, "systemctl is-active", true);
}

static bool DoWaitOnline(CommandLine &)
{
   // Also add services to After= in .service
   static const char *WaitingTasks[][6] = {
       {"systemd-networkd.service", "/lib/systemd/systemd-networkd-wait-online", "-q", "--timeout=30", nullptr},
       {"NetworkManager.service", "nm-online", "-q", "--timeout", "30", nullptr},
       {"connman.service", "connmand-wait-online", "--timeout=30", nullptr},
   };

   for (const char **task : WaitingTasks)
   {
      if (ServiceIsActive(task[0]))
      {
	 pid_t pid = ExecuteProcess(task + 1);

	 ExecWait(pid, task[1]);
      }
   }

   return _error->PendingError() == false;
}
									/*}}}*/
static bool DropPrivsAndRun(CommandLine &CmdL)				/*{{{*/
{
   if (CmdL.FileSize() < 2)
      return _error->Error("No command given to run without privileges");
   if (DropPrivileges() == false)
      return _error->Error("Dropping Privileges failed, not executing '%s'", CmdL.FileList[1]);

   std::vector<char const *> Args;
   Args.reserve(CmdL.FileSize() + 1);
   for (auto a = CmdL.FileList + 1; *a != nullptr; ++a)
      Args.push_back(*a);
   Args.push_back(nullptr);
   auto const pid = ExecuteProcess(Args.data());
   return ExecWait(pid, CmdL.FileList[1]);
}
									/*}}}*/
static bool AnalyzePattern(CommandLine &CmdL)				/*{{{*/
{
   if (CmdL.FileSize() != 2)
      return _error->Error("Expect one argument, a pattern");

   try
   {
      auto top = APT::Internal::PatternTreeParser(CmdL.FileList[1]).parseTop();
      top->render(std::cout) << "\n";
   }
   catch (APT::Internal::PatternTreeParser::Error &e)
   {
      std::stringstream ss;
      ss << "input:" << e.location.start << "-" << e.location.end << ": error: " << e.message << "\n";
      ss << CmdL.FileList[1] << "\n";
      for (size_t i = 0; i < e.location.start; i++)
	 ss << " ";
      for (size_t i = e.location.start; i < e.location.end; i++)
	 ss << "^";

      ss << "\n";

      _error->Error("%s", ss.str().c_str());
      return false;
   }

   return true;
}
									/*}}}*/
static bool DoQuoteString(CommandLine &CmdL)				/*{{{*/
{
   if (CmdL.FileSize() != 3)
      return _error->Error("Expect two arguments, a string to quote and a string of additional characters to quote");
   std::cout << QuoteString(CmdL.FileList[1], CmdL.FileList[2]) << '\n';
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
       {"wait-online", &DoWaitOnline, _("wait for system to be online")},
       {"drop-privs", &DropPrivsAndRun, _("drop privileges before running given command")},
       {"analyze-pattern", &AnalyzePattern, _("analyse a pattern")},
       {"analyse-pattern", &AnalyzePattern, nullptr},
       {"quote-string", &DoQuoteString, nullptr},
       {nullptr, nullptr, nullptr}};
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

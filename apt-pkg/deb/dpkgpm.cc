// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   DPKG Package Manager - Provide an interface to dpkg

   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/debsystem.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/install-progress.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/statechanges.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <apti18n.h>
									/*}}}*/

extern char **environ;

using namespace std;

APT_PURE static string AptHistoryRequestingUser()			/*{{{*/
{
   const char* EnvKeys[]{"SUDO_UID", "PKEXEC_UID", "PACKAGEKIT_CALLER_UID"};

   for (const auto &Key: EnvKeys)
   {
      if (getenv(Key) != nullptr)
      {
         int uid = atoi(getenv(Key));
         if (uid > 0) {
            struct passwd pwd;
            struct passwd *result;
            char buf[255];
            if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &result) == 0 && result != NULL) {
               std::string res;
               strprintf(res, "%s (%d)", pwd.pw_name, uid);
               return res;
            }
         }
      }
   }
   return "";
}
									/*}}}*/
APT_PURE static unsigned int EnvironmentSize()				/*{{{*/
{
  unsigned int size = 0;
  char **envp = environ;

  while (*envp != NULL)
    size += strlen (*envp++) + 1;

  return size;
}
									/*}}}*/
class pkgDPkgPMPrivate							/*{{{*/
{
public:
   pkgDPkgPMPrivate() : stdin_is_dev_null(false), status_fd_reached_end_of_file(false),
			dpkgbuf_pos(0), term_out(NULL), history_out(NULL),
			progress(NULL), tt_is_valid(false), master(-1),
			slave(NULL), protect_slave_from_dying(-1),
			direct_stdin(false)
   {
      dpkgbuf[0] = '\0';
   }
   ~pkgDPkgPMPrivate()
   {
   }
   bool stdin_is_dev_null;
   bool status_fd_reached_end_of_file;
   // the buffer we use for the dpkg status-fd reading
   char dpkgbuf[1024];
   size_t dpkgbuf_pos;
   FILE *term_out;
   FILE *history_out;
   string dpkg_error;
   APT::Progress::PackageManager *progress;

   // pty stuff
   struct termios tt;
   bool tt_is_valid;
   int master;
   char * slave;
   int protect_slave_from_dying;

   // signals
   sigset_t sigmask;
   sigset_t original_sigmask;

   bool direct_stdin;
};
									/*}}}*/
namespace
{
  // Maps the dpkg "processing" info to human readable names.  Entry 0
  // of each array is the key, entry 1 is the value.
  const std::pair<const char *, const char *> PackageProcessingOps[] = {
    std::make_pair("install",   N_("Preparing %s")),
    // we don't care for the difference
    std::make_pair("upgrade",   N_("Preparing %s")),
    std::make_pair("configure", N_("Preparing to configure %s")),
    std::make_pair("remove",    N_("Preparing for removal of %s")),
    std::make_pair("purge",     N_("Preparing to completely remove %s")),
    std::make_pair("disappear", N_("Noting disappearance of %s")),
    std::make_pair("trigproc",  N_("Running post-installation trigger %s"))
  };

  const std::pair<const char *, const char *> * const PackageProcessingOpsBegin = PackageProcessingOps;
  const std::pair<const char *, const char *> * const PackageProcessingOpsEnd   = PackageProcessingOps + sizeof(PackageProcessingOps) / sizeof(PackageProcessingOps[0]);

  // Predicate to test whether an entry in the PackageProcessingOps
  // array matches a string.
  class MatchProcessingOp
  {
    const char *target;

  public:
    explicit MatchProcessingOp(const char *the_target)
      : target(the_target)
    {
    }

    bool operator()(const std::pair<const char *, const char *> &pair) const
    {
      return strcmp(pair.first, target) == 0;
    }
  };
}

// ionice - helper function to ionice the given PID			/*{{{*/
/* there is no C header for ionice yet - just the syscall interface
   so we use the binary from util-linux */
static bool ionice(int PID)
{
   if (!FileExists("/usr/bin/ionice"))
      return false;
   pid_t Process = ExecFork();
   if (Process == 0)
   {
      char buf[32];
      snprintf(buf, sizeof(buf), "-p%d", PID);
      const char *Args[4];
      Args[0] = "/usr/bin/ionice";
      Args[1] = "-c3";
      Args[2] = buf;
      Args[3] = 0;
      execv(Args[0], (char **)Args);
   }
   return ExecWait(Process, "ionice");
}
									/*}}}*/
// FindNowVersion - Helper to find a Version in "now" state	/*{{{*/
// ---------------------------------------------------------------------
/* This is helpful when a package is no longer installed but has residual
 * config files
 */
static
pkgCache::VerIterator FindNowVersion(const pkgCache::PkgIterator &Pkg)
{
   pkgCache::VerIterator Ver;
   for (Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
      for (pkgCache::VerFileIterator Vf = Ver.FileList(); Vf.end() == false; ++Vf)
	 for (pkgCache::PkgFileIterator F = Vf.File(); F.end() == false; ++F)
	 {
	    if (F.Archive() != 0 && strcmp(F.Archive(), "now") == 0)
	       return Ver;
	 }
   return Ver;
}
									/*}}}*/
static pkgCache::VerIterator FindToBeRemovedVersion(pkgCache::PkgIterator const &Pkg)/*{{{*/
{
   auto const PV = Pkg.CurrentVer();
   if (PV.end() == false)
      return PV;
   return FindNowVersion(Pkg);
}
									/*}}}*/

// DPkgPM::pkgDPkgPM - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::pkgDPkgPM(pkgDepCache *Cache)
   : pkgPackageManager(Cache),d(new pkgDPkgPMPrivate()), pkgFailures(0), PackagesDone(0), PackagesTotal(0)
{
}
									/*}}}*/
// DPkgPM::pkgDPkgPM - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDPkgPM::~pkgDPkgPM()
{
   delete d;
}
									/*}}}*/
// DPkgPM::Install - Install a package					/*{{{*/
// ---------------------------------------------------------------------
/* Add an install operation to the sequence list */
bool pkgDPkgPM::Install(PkgIterator Pkg,string File)
{
   if (File.empty() == true || Pkg.end() == true)
      return _error->Error("Internal Error, No file name for %s",Pkg.FullName().c_str());

   List.emplace_back(Item::Install, Pkg, debSystem::StripDpkgChrootDirectory(File));
   return true;
}
									/*}}}*/
// DPkgPM::Configure - Configure a package				/*{{{*/
// ---------------------------------------------------------------------
/* Add a configure operation to the sequence list */
bool pkgDPkgPM::Configure(PkgIterator Pkg)
{
   if (Pkg.end() == true)
      return false;

   List.push_back(Item(Item::Configure, Pkg));

   // Use triggers for config calls if we configure "smart"
   // as otherwise Pre-Depends will not be satisfied, see #526774
   if (_config->FindB("DPkg::TriggersPending", false) == true)
      List.push_back(Item(Item::TriggersPending, PkgIterator()));

   return true;
}
									/*}}}*/
// DPkgPM::Remove - Remove a package					/*{{{*/
// ---------------------------------------------------------------------
/* Add a remove operation to the sequence list */
bool pkgDPkgPM::Remove(PkgIterator Pkg,bool Purge)
{
   if (Pkg.end() == true)
      return false;

   if (Purge == true)
      List.push_back(Item(Item::Purge,Pkg));
   else
      List.push_back(Item(Item::Remove,Pkg));
   return true;
}
									/*}}}*/
// DPkgPM::SendPkgInfo - Send info for install-pkgs hook		/*{{{*/
// ---------------------------------------------------------------------
/* This is part of the helper script communication interface, it sends
   very complete information down to the other end of the pipe.*/
bool pkgDPkgPM::SendPkgsInfo(FILE * const F, unsigned int const &Version)
{
   // This version of APT supports only v3, so don't sent higher versions
   if (Version <= 3)
      fprintf(F,"VERSION %u\n", Version);
   else
      fprintf(F,"VERSION 3\n");

   /* Write out all of the configuration directives by walking the
      configuration tree */
   const Configuration::Item *Top = _config->Tree(0);
   for (; Top != 0;)
   {
      if (Top->Value.empty() == false)
      {
	 fprintf(F,"%s=%s\n",
		 QuoteString(Top->FullTag(),"=\"\n").c_str(),
		 QuoteString(Top->Value,"\n").c_str());
      }

      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }

      while (Top != 0 && Top->Next == 0)
	 Top = Top->Parent;
      if (Top != 0)
	 Top = Top->Next;
   }
   fprintf(F,"\n");

   // Write out the package actions in order.
   for (vector<Item>::iterator I = List.begin(); I != List.end(); ++I)
   {
      if(I->Pkg.end() == true)
	 continue;

      pkgDepCache::StateCache &S = Cache[I->Pkg];

      fprintf(F,"%s ",I->Pkg.Name());

      // Current version which we are going to replace
      pkgCache::VerIterator CurVer = I->Pkg.CurrentVer();
      if (CurVer.end() == true && (I->Op == Item::Remove || I->Op == Item::Purge))
	 CurVer = FindNowVersion(I->Pkg);

      if (CurVer.end() == true)
      {
	 if (Version <= 2)
	    fprintf(F, "- ");
	 else
	    fprintf(F, "- - none ");
      }
      else
      {
	 fprintf(F, "%s ", CurVer.VerStr());
	 if (Version >= 3)
	    fprintf(F, "%s %s ", CurVer.Arch(), CurVer.MultiArchType());
      }

      // Show the compare operator between current and install version
      if (S.InstallVer != 0)
      {
	 pkgCache::VerIterator const InstVer = S.InstVerIter(Cache);
	 int Comp = 2;
	 if (CurVer.end() == false)
	    Comp = InstVer.CompareVer(CurVer);
	 if (Comp < 0)
	    fprintf(F,"> ");
	 else if (Comp == 0)
	    fprintf(F,"= ");
	 else if (Comp > 0)
	    fprintf(F,"< ");
	 fprintf(F, "%s ", InstVer.VerStr());
	 if (Version >= 3)
	    fprintf(F, "%s %s ", InstVer.Arch(), InstVer.MultiArchType());
      }
      else
      {
	 if (Version <= 2)
	    fprintf(F, "> - ");
	 else
	    fprintf(F, "> - - none ");
      }

      // Show the filename/operation
      if (I->Op == Item::Install)
      {
	 // No errors here..
	 if (I->File[0] != '/')
	    fprintf(F,"**ERROR**\n");
	 else
	    fprintf(F,"%s\n",I->File.c_str());
      }
      else if (I->Op == Item::Configure)
	 fprintf(F,"**CONFIGURE**\n");
      else if (I->Op == Item::Remove ||
	  I->Op == Item::Purge)
	 fprintf(F,"**REMOVE**\n");

      if (ferror(F) != 0)
	 return false;
   }
   return true;
}
									/*}}}*/
// DPkgPM::RunScriptsWithPkgs - Run scripts with package names on stdin /*{{{*/
// ---------------------------------------------------------------------
/* This looks for a list of scripts to run from the configuration file
   each one is run and is fed on standard input a list of all .deb files
   that are due to be installed. */
bool pkgDPkgPM::RunScriptsWithPkgs(const char *Cnf)
{
   bool result = true;

   Configuration::Item const *Opts = _config->Tree(Cnf);
   if (Opts == 0 || Opts->Child == 0)
      return true;
   Opts = Opts->Child;

   sighandler_t old_sigpipe = signal(SIGPIPE, SIG_IGN);
   sighandler_t old_sigint = signal(SIGINT, SIG_IGN);
   sighandler_t old_sigquit = signal(SIGQUIT, SIG_IGN);

   unsigned int Count = 1;
   for (; Opts != 0; Opts = Opts->Next, Count++)
   {
      if (Opts->Value.empty() == true)
         continue;

      if(_config->FindB("Debug::RunScripts", false) == true)
         std::clog << "Running external script with list of all .deb file: '"
                   << Opts->Value << "'" << std::endl;

      // Determine the protocol version
      string OptSec = Opts->Value;
      string::size_type Pos;
      if ((Pos = OptSec.find(' ')) == string::npos || Pos == 0)
	 Pos = OptSec.length();
      OptSec = "DPkg::Tools::Options::" + string(Opts->Value.c_str(),Pos);

      unsigned int Version = _config->FindI(OptSec+"::Version",1);
      unsigned int InfoFD = _config->FindI(OptSec + "::InfoFD", STDIN_FILENO);

      // Create the pipes
      std::set<int> KeepFDs;
      MergeKeepFdsFromConfiguration(KeepFDs);
      int Pipes[2];
      if (pipe(Pipes) != 0) {
         result = _error->Errno("pipe","Failed to create IPC pipe to subprocess");
         break;
      }
      if (InfoFD != (unsigned)Pipes[0])
	 SetCloseExec(Pipes[0],true);
      else
         KeepFDs.insert(Pipes[0]);


      SetCloseExec(Pipes[1],true);

      // Purified Fork for running the script
      pid_t Process = ExecFork(KeepFDs);
      if (Process == 0)
      {
	 // Setup the FDs
	 dup2(Pipes[0], InfoFD);
	 SetCloseExec(STDOUT_FILENO,false);
	 SetCloseExec(STDIN_FILENO,false);
	 SetCloseExec(STDERR_FILENO,false);

	 string hookfd;
	 strprintf(hookfd, "%d", InfoFD);
	 setenv("APT_HOOK_INFO_FD", hookfd.c_str(), 1);

	 if (_system != nullptr && _system->IsLocked() == true && stringcasecmp(Cnf, "DPkg::Pre-Install-Pkgs") == 0)
	    setenv("DPKG_FRONTEND_LOCKED", "true", 1);

	 debSystem::DpkgChrootDirectory();
	 const char *Args[4];
	 Args[0] = "/bin/sh";
	 Args[1] = "-c";
	 Args[2] = Opts->Value.c_str();
	 Args[3] = 0;
	 execv(Args[0],(char **)Args);
	 _exit(100);
      }
      close(Pipes[0]);
      FILE *F = fdopen(Pipes[1],"w");
      if (F == 0) {
         result = _error->Errno("fdopen","Failed to open new FD");
         break;
      }

      // Feed it the filenames.
      if (Version <= 1)
      {
	 for (vector<Item>::iterator I = List.begin(); I != List.end(); ++I)
	 {
	    // Only deal with packages to be installed from .deb
	    if (I->Op != Item::Install)
	       continue;

	    // No errors here..
	    if (I->File[0] != '/')
	       continue;

	    /* Feed the filename of each package that is pending install
	       into the pipe. */
	    fprintf(F,"%s\n",I->File.c_str());
	    if (ferror(F) != 0)
	       break;
	 }
      }
      else
	 SendPkgsInfo(F, Version);

      fclose(F);

      // Clean up the sub process
      if (ExecWait(Process,Opts->Value.c_str()) == false) {
	 result = _error->Error("Failure running script %s",Opts->Value.c_str());
         break;
      }
   }
   signal(SIGINT, old_sigint);
   signal(SIGPIPE, old_sigpipe);
   signal(SIGQUIT, old_sigquit);

   return result;
}
									/*}}}*/
// DPkgPM::DoStdin - Read stdin and pass to master pty			/*{{{*/
// ---------------------------------------------------------------------
/*
*/
void pkgDPkgPM::DoStdin(int master)
{
   unsigned char input_buf[256] = {0,}; 
   ssize_t len = read(STDIN_FILENO, input_buf, sizeof(input_buf));
   if (len)
      FileFd::Write(master, input_buf, len);
   else
      d->stdin_is_dev_null = true;
}
									/*}}}*/
// DPkgPM::DoTerminalPty - Read the terminal pty and write log		/*{{{*/
// ---------------------------------------------------------------------
/*
 * read the terminal pty and write log
 */
void pkgDPkgPM::DoTerminalPty(int master)
{
   unsigned char term_buf[1024] = {0,0, };

   ssize_t len=read(master, term_buf, sizeof(term_buf));
   if(len == -1 && errno == EIO)
   {
      // this happens when the child is about to exit, we
      // give it time to actually exit, otherwise we run
      // into a race so we sleep for half a second.
      struct timespec sleepfor = { 0, 500000000 };
      nanosleep(&sleepfor, NULL);
      return;
   }
   if(len <= 0)
      return;
   FileFd::Write(1, term_buf, len);
   if(d->term_out)
      fwrite(term_buf, len, sizeof(char), d->term_out);
}
									/*}}}*/
// DPkgPM::ProcessDpkgStatusBuf						/*{{{*/
void pkgDPkgPM::ProcessDpkgStatusLine(char *line)
{
   bool const Debug = _config->FindB("Debug::pkgDPkgProgressReporting",false);
   if (Debug == true)
      std::clog << "got from dpkg '" << line << "'" << std::endl;

   /* dpkg sends strings like this:
      'status:   <pkg>: <pkg  qstate>'
      'status:   <pkg>:<arch>: <pkg  qstate>'

      'processing: {install,upgrade,configure,remove,purge,disappear,trigproc}: pkg'
      'processing: {install,upgrade,configure,remove,purge,disappear,trigproc}: trigger'
   */

   // we need to split on ": " (note the appended space) as the ':' is
   // part of the pkgname:arch information that dpkg sends
   //
   // A dpkg error message may contain additional ":" (like
   //  "failed in buffer_write(fd) (10, ret=-1): backend dpkg-deb ..."
   // so we need to ensure to not split too much
   std::vector<std::string> list = StringSplit(line, ": ", 4);
   if(list.size() < 3)
   {
      if (Debug == true)
	 std::clog << "ignoring line: not enough ':'" << std::endl;
      return;
   }

   // build the (prefix, pkgname, action) tuple, position of this
   // is different for "processing" or "status" messages
   std::string prefix = APT::String::Strip(list[0]);
   std::string pkgname;
   std::string action;

   // "processing" has the form "processing: action: pkg or trigger"
   // with action = ["install", "upgrade", "configure", "remove", "purge",
   //                "disappear", "trigproc"]
   if (prefix == "processing")
   {
      pkgname = APT::String::Strip(list[2]);
      action = APT::String::Strip(list[1]);
   }
   // "status" has the form: "status: pkg: state"
   // with state in ["half-installed", "unpacked", "half-configured",
   //                "installed", "config-files", "not-installed"]
   else if (prefix == "status")
   {
      pkgname = APT::String::Strip(list[1]);
      action = APT::String::Strip(list[2]);

      /* handle the special cases first:

	 errors look like this:
	 'status: /var/cache/apt/archives/krecipes_0.8.1-0ubuntu1_i386.deb : error : trying to overwrite `/usr/share/doc/kde/HTML/en/krecipes/krectip.png', which is also in package krecipes-data
	 and conffile-prompt like this
	 'status:/etc/compiz.conf/compiz.conf :  conffile-prompt: 'current-conffile' 'new-conffile' useredited distedited
	 */
      if(action == "error")
      {
         d->progress->Error(pkgname, PackagesDone, PackagesTotal, list[3]);
         ++pkgFailures;
         WriteApportReport(pkgname.c_str(), list[3].c_str());
         return;
      }
      else if(action == "conffile-prompt")
      {
         d->progress->ConffilePrompt(pkgname, PackagesDone, PackagesTotal, list[3]);
         return;
      }
   } else {
      if (Debug == true)
	 std::clog << "unknown prefix '" << prefix << "'" << std::endl;
      return;
   }

   // At this point we have a pkgname, but it might not be arch-qualified !
   if (pkgname.find(":") == std::string::npos)
   {
      pkgCache::GrpIterator const Grp = Cache.FindGrp(pkgname);
      if (unlikely(Grp.end()== true))
      {
	 if (Debug == true)
	    std::clog << "unable to figure out which package is dpkg referring to with '" << pkgname << "'! (0)" << std::endl;
	 return;
      }
      /* No arch means that dpkg believes there can only be one package
         this can refer to so lets see what could be candidates here: */
      std::vector<pkgCache::PkgIterator> candset;
      for (auto P = Grp.PackageList(); P.end() != true; P = Grp.NextPkg(P))
      {
	 if (PackageOps.find(P.FullName()) != PackageOps.end())
	    candset.push_back(P);
	 // packages can disappear without them having any interaction itself
	 // so we have to consider these as candidates, too
	 else if (P->CurrentVer != 0 && action == "disappear")
	    candset.push_back(P);
      }
      if (unlikely(candset.empty()))
      {
	 if (Debug == true)
	    std::clog << "unable to figure out which package is dpkg referring to with '" << pkgname << "'! (1)" << std::endl;
	 return;
      }
      else if (candset.size() == 1) // we are lucky
	 pkgname = candset.cbegin()->FullName();
      else
      {
	 /* here be dragons^Wassumptions about dpkg:
	    - an M-A:same version is always arch-qualified
	    - a package from a foreign arch is (in newer versions) */
	 size_t installedInstances = 0, wannabeInstances = 0;
	 for (auto const &P: candset)
	 {
	    if (P->CurrentVer != 0)
	    {
	       ++installedInstances;
	       if (Cache[P].Delete() == false)
		  ++wannabeInstances;
	    }
	    else if (Cache[P].Install())
	       ++wannabeInstances;
	 }
	 // the package becomes M-A:same, so we are still talking about current
	 if (installedInstances == 1 && wannabeInstances >= 2)
	 {
	    for (auto const &P: candset)
	    {
	       if (P->CurrentVer == 0)
		  continue;
	       pkgname = P.FullName();
	       break;
	    }
	 }
	 // the package was M-A:same, it isn't now, so we can only talk about that
	 else if (installedInstances >= 2 && wannabeInstances == 1)
	 {
	    for (auto const &P: candset)
	    {
	       auto const IV = Cache[P].InstVerIter(Cache);
	       if (IV.end())
		  continue;
	       pkgname = P.FullName();
	       break;
	    }
	 }
	 // that is a crossgrade
	 else if (installedInstances == 1 && wannabeInstances == 1 && candset.size() == 2)
	 {
	    auto const PkgHasCurrentVersion = [](pkgCache::PkgIterator const &P) { return P->CurrentVer != 0; };
	    auto const P = std::find_if(candset.begin(), candset.end(), PkgHasCurrentVersion);
	    if (unlikely(P == candset.end()))
	    {
	       if (Debug == true)
		  std::clog << "situation for '" << pkgname << "' looked like a crossgrade, but no current version?!" << std::endl;
	       return;
	    }
	    auto fullname = P->FullName();
	    if (PackageOps[fullname].size() != PackageOpsDone[fullname])
	       pkgname = std::move(fullname);
	    else
	    {
	       auto const pkgi = std::find_if_not(candset.begin(), candset.end(), PkgHasCurrentVersion);
	       if (unlikely(pkgi == candset.end()))
	       {
		  if (Debug == true)
		     std::clog << "situation for '" << pkgname << "' looked like a crossgrade, but all are installed?!" << std::endl;
		  return;
	       }
	       pkgname = pkgi->FullName();
	    }
	 }
	 // we are desperate: so "just" take the native one, but that might change mid-air,
	 // so we have to ask dpkg what it believes native is at the moment… all the time
	 else
	 {
	    std::vector<std::string> sArgs = debSystem::GetDpkgBaseCommand();
	    sArgs.push_back("--print-architecture");
	    int outputFd = -1;
	    pid_t const dpkgNativeArch = debSystem::ExecDpkg(sArgs, nullptr, &outputFd, true);
	    if (unlikely(dpkgNativeArch == -1))
	    {
	       if (Debug == true)
		  std::clog << "calling dpkg failed to ask it for its current native architecture to expand '" << pkgname << "'!" << std::endl;
	       return;
	    }
	    FILE *dpkg = fdopen(outputFd, "r");
	    if(dpkg != NULL)
	    {
	       char* buf = NULL;
	       size_t bufsize = 0;
	       if (getline(&buf, &bufsize, dpkg) != -1)
		  pkgname += ':' + bufsize;
	       free(buf);
	       fclose(dpkg);
	    }
	    ExecWait(dpkgNativeArch, "dpkg --print-architecture", true);
	    if (pkgname.find(':') != std::string::npos)
	    {
	       if (Debug == true)
		  std::clog << "unable to figure out which package is dpkg referring to with '" << pkgname << "'! (2)" << std::endl;
	       return;
	    }
	 }
      }
   }

   std::string arch = "";
   if (pkgname.find(":") != string::npos)
      arch = StringSplit(pkgname, ":")[1];
   std::string i18n_pkgname = pkgname;
   if (arch.size() != 0)
      strprintf(i18n_pkgname, "%s (%s)", StringSplit(pkgname, ":")[0].c_str(), arch.c_str());

   // 'processing' from dpkg looks like
   // 'processing: action: pkg'
   if(prefix == "processing")
   {
      auto const iter = std::find_if(PackageProcessingOpsBegin, PackageProcessingOpsEnd, MatchProcessingOp(action.c_str()));
      if(iter == PackageProcessingOpsEnd)
      {
	 if (Debug == true)
	    std::clog << "ignoring unknown action: " << action << std::endl;
	 return;
      }
      std::string msg;
      strprintf(msg, _(iter->second), i18n_pkgname.c_str());
      d->progress->StatusChanged(pkgname, PackagesDone, PackagesTotal, msg);

      // FIXME: this needs a muliarch testcase
      // FIXME2: is "pkgname" here reliable with dpkg only sending us
      //         short pkgnames?
      if (action == "disappear")
	 handleDisappearAction(pkgname);
      else if (action == "upgrade")
	 handleCrossUpgradeAction(pkgname);
      return;
   }

   if (prefix == "status")
   {
      std::vector<struct DpkgState> &states = PackageOps[pkgname];
      if(PackageOpsDone[pkgname] < states.size())
      {
	 char const * next_action = states[PackageOpsDone[pkgname]].state;
	 if (next_action)
	 {
	    /*
	    if (action == "half-installed" && strcmp("half-configured", next_action) == 0 &&
		  PackageOpsDone[pkg] + 2 < states.size() && action == states[PackageOpsDone[pkg] + 2].state)
	    {
	       if (Debug == true)
		  std::clog << "(parsed from dpkg) pkg: " << short_pkgname << " action: " << action
		     << " pending trigger defused by unpack" << std::endl;
	       // unpacking a package defuses the pending trigger
	       PackageOpsDone[pkg] += 2;
	       PackagesDone += 2;
	       next_action = states[PackageOpsDone[pkg]].state;
	    }
	    */
	    if (Debug == true)
	       std::clog << "(parsed from dpkg) pkg: " << pkgname
		  << " action: " << action << " (expected: '" << next_action << "' "
		  << PackageOpsDone[pkgname] << " of " << states.size() << ")" << endl;

	    // check if the package moved to the next dpkg state
	    if(action == next_action)
	    {
	       // only read the translation if there is actually a next action
	       char const * const translation = _(states[PackageOpsDone[pkgname]].str);

	       // we moved from one dpkg state to a new one, report that
	       ++PackageOpsDone[pkgname];
	       ++PackagesDone;

	       std::string msg;
	       strprintf(msg, translation, i18n_pkgname.c_str());
	       d->progress->StatusChanged(pkgname, PackagesDone, PackagesTotal, msg);
	    }
	 }
      }
      else if (action == "triggers-pending")
      {
	 if (Debug == true)
	    std::clog << "(parsed from dpkg) pkg: " << pkgname
	       << " action: " << action << " (prefix 2 to "
	       << PackageOpsDone[pkgname] << " of " << states.size() << ")" << endl;

	 states.insert(states.begin(), {"installed", N_("Installed %s")});
	 states.insert(states.begin(), {"half-configured", N_("Configuring %s")});
	 PackagesTotal += 2;
      }
   }
}
									/*}}}*/
// DPkgPM::handleDisappearAction					/*{{{*/
void pkgDPkgPM::handleDisappearAction(string const &pkgname)
{
   pkgCache::PkgIterator Pkg = Cache.FindPkg(pkgname);
   if (unlikely(Pkg.end() == true))
      return;

   // a disappeared package has no further actions
   auto const ROps = PackageOps[Pkg.FullName()].size();
   auto && ROpsDone = PackageOpsDone[Pkg.FullName()];
   PackagesDone += ROps - ROpsDone;
   ROpsDone = ROps;

   // record the package name for display and stuff later
   disappearedPkgs.insert(Pkg.FullName(true));

   // the disappeared package was auto-installed - nothing to do
   if ((Cache[Pkg].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
      return;
   pkgCache::VerIterator PkgVer = Cache[Pkg].InstVerIter(Cache);
   if (unlikely(PkgVer.end() == true))
      return;
   /* search in the list of dependencies for (Pre)Depends,
      check if this dependency has a Replaces on our package
      and if so transfer the manual installed flag to it */
   for (pkgCache::DepIterator Dep = PkgVer.DependsList(); Dep.end() != true; ++Dep)
   {
      if (Dep->Type != pkgCache::Dep::Depends &&
	  Dep->Type != pkgCache::Dep::PreDepends)
	 continue;
      pkgCache::PkgIterator Tar = Dep.TargetPkg();
      if (unlikely(Tar.end() == true))
	 continue;
      // the package is already marked as manual
      if ((Cache[Tar].Flags & pkgCache::Flag::Auto) != pkgCache::Flag::Auto)
	 continue;
      pkgCache::VerIterator TarVer =  Cache[Tar].InstVerIter(Cache);
      if (TarVer.end() == true)
	 continue;
      for (pkgCache::DepIterator Rep = TarVer.DependsList(); Rep.end() != true; ++Rep)
      {
	 if (Rep->Type != pkgCache::Dep::Replaces)
	    continue;
	 if (Pkg != Rep.TargetPkg())
	    continue;
	 // okay, they are strongly connected - transfer manual-bit
	 if (Debug == true)
	    std::clog << "transfer manual-bit from disappeared »" << pkgname << "« to »" << Tar.FullName() << "«" << std::endl;
	 Cache[Tar].Flags &= ~Flag::Auto;
	 break;
      }
   }
}
									/*}}}*/
void pkgDPkgPM::handleCrossUpgradeAction(string const &pkgname)		/*{{{*/
{
   // in a crossgrade what looked like a remove first is really an unpack over it
   auto const Pkg = Cache.FindPkg(pkgname);
   if (likely(Pkg.end() == false) && Cache[Pkg].Delete())
   {
      auto const Grp = Pkg.Group();
      if (likely(Grp.end() == false))
      {
	 for (auto P = Grp.PackageList(); P.end() != true; P = Grp.NextPkg(P))
	    if(Cache[P].Install())
	    {
	       auto && Ops = PackageOps[P.FullName()];
	       auto const unpackOp = std::find_if(Ops.cbegin(), Ops.cend(), [](DpkgState const &s) { return strcmp(s.state, "unpacked") == 0; });
	       if (unpackOp != Ops.cend())
	       {
		  // skip ahead in the crossgraded packages
		  auto const skipped = std::distance(Ops.cbegin(), unpackOp);
		  PackagesDone += skipped;
		  PackageOpsDone[P.FullName()] += skipped;
		  // finish the crossremoved package
		  auto const ROps = PackageOps[Pkg.FullName()].size();
		  auto && ROpsDone = PackageOpsDone[Pkg.FullName()];
		  PackagesDone += ROps - ROpsDone;
		  ROpsDone = ROps;
		  break;
	       }
	    }
      }
   }
}
									/*}}}*/
// DPkgPM::DoDpkgStatusFd						/*{{{*/
void pkgDPkgPM::DoDpkgStatusFd(int statusfd)
{
   auto const remainingBuffer = (sizeof(d->dpkgbuf) / sizeof(d->dpkgbuf[0])) - d->dpkgbuf_pos;
   if (likely(remainingBuffer > 0) && d->status_fd_reached_end_of_file == false)
   {
      auto const len = read(statusfd, &d->dpkgbuf[d->dpkgbuf_pos], remainingBuffer);
      if (len < 0)
	 return;
      else if (len == 0 && d->dpkgbuf_pos == 0)
      {
	 d->status_fd_reached_end_of_file = true;
	 return;
      }
      d->dpkgbuf_pos += (len / sizeof(d->dpkgbuf[0]));
   }

   // process line by line from the buffer
   char *p = d->dpkgbuf, *q = nullptr;
   while((q=(char*)memchr(p, '\n', (d->dpkgbuf + d->dpkgbuf_pos) - p)) != nullptr)
   {
      *q = '\0';
      ProcessDpkgStatusLine(p);
      p = q + 1; // continue with next line
   }

   // check if we stripped the buffer clean
   if (p > (d->dpkgbuf + d->dpkgbuf_pos))
   {
      d->dpkgbuf_pos = 0;
      return;
   }

   // otherwise move the unprocessed tail to the start and update pos
   memmove(d->dpkgbuf, p, (p - d->dpkgbuf));
   d->dpkgbuf_pos = (d->dpkgbuf + d->dpkgbuf_pos) - p;
}
									/*}}}*/
// DPkgPM::WriteHistoryTag						/*{{{*/
void pkgDPkgPM::WriteHistoryTag(string const &tag, string value)
{
   size_t const length = value.length();
   if (length == 0)
      return;
   // poor mans rstrip(", ")
   if (value[length-2] == ',' && value[length-1] == ' ')
      value.erase(length - 2, 2);
   fprintf(d->history_out, "%s: %s\n", tag.c_str(), value.c_str());
}									/*}}}*/
// DPkgPM::OpenLog							/*{{{*/
bool pkgDPkgPM::OpenLog()
{
   string const logfile_name =  _config->FindFile("Dir::Log::Terminal", "/dev/null");
   string logdir = flNotFile(logfile_name);
   if(CreateAPTDirectoryIfNeeded(logdir, logdir) == false)
      // FIXME: use a better string after freeze
      return _error->Error(_("Directory '%s' missing"), logdir.c_str());

   // get current time
   char timestr[200];
   time_t const t = time(NULL);
   struct tm tm_buf;
   struct tm const * const tmp = localtime_r(&t, &tm_buf);
   strftime(timestr, sizeof(timestr), "%F  %T", tmp);

   // open terminal log
   if (logfile_name != "/dev/null")
   {
      d->term_out = fopen(logfile_name.c_str(),"a");
      if (d->term_out == NULL)
	 return _error->WarningE("OpenLog", _("Could not open file '%s'"), logfile_name.c_str());
      setvbuf(d->term_out, NULL, _IONBF, 0);
      SetCloseExec(fileno(d->term_out), true);
      if (getuid() == 0) // if we aren't root, we can't chown a file, so don't try it
      {
	 struct passwd *pw = getpwnam("root");
	 struct group *gr = getgrnam("adm");
	 if (pw != NULL && gr != NULL && chown(logfile_name.c_str(), pw->pw_uid, gr->gr_gid) != 0)
	    _error->WarningE("OpenLog", "chown to root:adm of file %s failed", logfile_name.c_str());
      }
      if (chmod(logfile_name.c_str(), 0640) != 0)
	 _error->WarningE("OpenLog", "chmod 0640 of file %s failed", logfile_name.c_str());
      fprintf(d->term_out, "\nLog started: %s\n", timestr);
   }

   // write your history
   string const history_name = _config->FindFile("Dir::Log::History", "/dev/null");
   string logdir2 = flNotFile(logfile_name);
   if(logdir != logdir2 && CreateAPTDirectoryIfNeeded(logdir2, logdir2) == false)
      return _error->Error(_("Directory '%s' missing"), logdir.c_str());
   if (history_name != "/dev/null")
   {
      d->history_out = fopen(history_name.c_str(),"a");
      if (d->history_out == NULL)
	 return _error->WarningE("OpenLog", _("Could not open file '%s'"), history_name.c_str());
      SetCloseExec(fileno(d->history_out), true);
      chmod(history_name.c_str(), 0644);
      fprintf(d->history_out, "\nStart-Date: %s\n", timestr);
      string remove, purge, install, reinstall, upgrade, downgrade;
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      {
	 enum { CANDIDATE, CANDIDATE_AUTO, CURRENT_CANDIDATE, CURRENT } infostring;
	 string *line = NULL;
	 #define HISTORYINFO(X, Y) { line = &X; infostring = Y; }
	 if (Cache[I].NewInstall() == true)
	    HISTORYINFO(install, CANDIDATE_AUTO)
	 else if (Cache[I].ReInstall() == true)
	    HISTORYINFO(reinstall, CANDIDATE)
	 else if (Cache[I].Upgrade() == true)
	    HISTORYINFO(upgrade, CURRENT_CANDIDATE)
	 else if (Cache[I].Downgrade() == true)
	    HISTORYINFO(downgrade, CURRENT_CANDIDATE)
	 else if (Cache[I].Delete() == true)
	    HISTORYINFO((Cache[I].Purge() ? purge : remove), CURRENT)
	 else
	    continue;
	 #undef HISTORYINFO
	 line->append(I.FullName(false)).append(" (");
	 switch (infostring) {
	 case CANDIDATE: line->append(Cache[I].CandVersion); break;
	 case CANDIDATE_AUTO:
	    line->append(Cache[I].CandVersion);
	    if ((Cache[I].Flags & pkgCache::Flag::Auto) == pkgCache::Flag::Auto)
	       line->append(", automatic");
	    break;
	 case CURRENT_CANDIDATE: line->append(Cache[I].CurVersion).append(", ").append(Cache[I].CandVersion); break;
	 case CURRENT: line->append(Cache[I].CurVersion); break;
	 }
	 line->append("), ");
      }
      if (_config->Exists("Commandline::AsString") == true)
	 WriteHistoryTag("Commandline", _config->Find("Commandline::AsString"));
      std::string RequestingUser = AptHistoryRequestingUser();
      if (RequestingUser != "")
         WriteHistoryTag("Requested-By", RequestingUser);
      WriteHistoryTag("Install", install);
      WriteHistoryTag("Reinstall", reinstall);
      WriteHistoryTag("Upgrade", upgrade);
      WriteHistoryTag("Downgrade",downgrade);
      WriteHistoryTag("Remove",remove);
      WriteHistoryTag("Purge",purge);
      fflush(d->history_out);
   }

   return true;
}
									/*}}}*/
// DPkg::CloseLog							/*{{{*/
bool pkgDPkgPM::CloseLog()
{
   char timestr[200];
   time_t t = time(NULL);
   struct tm tm_buf;
   struct tm *tmp = localtime_r(&t, &tm_buf);
   strftime(timestr, sizeof(timestr), "%F  %T", tmp);

   if(d->term_out)
   {
      fprintf(d->term_out, "Log ended: ");
      fprintf(d->term_out, "%s", timestr);
      fprintf(d->term_out, "\n");
      fclose(d->term_out);
   }
   d->term_out = NULL;

   if(d->history_out)
   {
      if (disappearedPkgs.empty() == false)
      {
	 string disappear;
	 for (std::set<std::string>::const_iterator d = disappearedPkgs.begin();
	      d != disappearedPkgs.end(); ++d)
	 {
	    pkgCache::PkgIterator P = Cache.FindPkg(*d);
	    disappear.append(*d);
	    if (P.end() == true)
	       disappear.append(", ");
	    else
	       disappear.append(" (").append(Cache[P].CurVersion).append("), ");
	 }
	 WriteHistoryTag("Disappeared", disappear);
      }
      if (d->dpkg_error.empty() == false)
	 fprintf(d->history_out, "Error: %s\n", d->dpkg_error.c_str());
      fprintf(d->history_out, "End-Date: %s\n", timestr);
      fclose(d->history_out);
   }
   d->history_out = NULL;

   return true;
}
									/*}}}*/

// DPkgPM::BuildPackagesProgressMap					/*{{{*/
void pkgDPkgPM::BuildPackagesProgressMap()
{
   // map the dpkg states to the operations that are performed
   // (this is sorted in the same way as Item::Ops)
   static const std::array<std::array<DpkgState, 2>, 4> DpkgStatesOpMap = {{
      // Install operation
      {{
	 {"half-installed", N_("Unpacking %s")},
	 {"unpacked", N_("Installing %s") },
      }},
      // Configure operation
      {{
	 {"half-configured", N_("Configuring %s") },
	 { "installed", N_("Installed %s")},
      }},
      // Remove operation
      {{
	 {"half-configured", N_("Removing %s")},
	 {"half-installed", N_("Removing %s")},
      }},
      // Purge operation
      {{
	 {"config-files", N_("Completely removing %s")},
	 {"not-installed", N_("Completely removed %s")},
      }},
   }};
   static_assert(Item::Purge == 3, "Enum item has unexpected index for mapping array");

   // init the PackageOps map, go over the list of packages that
   // that will be [installed|configured|removed|purged] and add
   // them to the PackageOps map (the dpkg states it goes through)
   // and the PackageOpsTranslations (human readable strings)
   for (auto &&I : List)
   {
      if(I.Pkg.end() == true)
	 continue;

      string const name = I.Pkg.FullName();
      PackageOpsDone[name] = 0;
      auto AddToPackageOps = [&](decltype(I.Op) const Op) {
	 auto const DpkgOps = DpkgStatesOpMap[Op];
	 std::copy(DpkgOps.begin(), DpkgOps.end(), std::back_inserter(PackageOps[name]));
	 PackagesTotal += DpkgOps.size();
      };
      // purging a package which is installed first passes through remove states
      if (I.Op == Item::Purge && I.Pkg->CurrentVer != 0)
	 AddToPackageOps(Item::Remove);
      AddToPackageOps(I.Op);

      if ((I.Op == Item::Remove || I.Op == Item::Purge) && I.Pkg->CurrentVer != 0)
      {
	 if (I.Pkg->CurrentState == pkgCache::State::UnPacked ||
	       I.Pkg->CurrentState == pkgCache::State::HalfInstalled)
	 {
	    if (likely(strcmp(PackageOps[name][0].state, "half-configured") == 0))
	    {
	       ++PackageOpsDone[name];
	       --PackagesTotal;
	    }
	 }
      }
   }
   /* one extra: We don't want the progress bar to reach 100%, especially not
      if we call dpkg --configure --pending and process a bunch of triggers
      while showing 100%. Also, spindown takes a while, so never reaching 100%
      is way more correct than reaching 100% while still doing stuff even if
      doing it this way is slightly bending the rules */
   ++PackagesTotal;
}
                                                                        /*}}}*/
void pkgDPkgPM::StartPtyMagic()						/*{{{*/
{
   if (_config->FindB("Dpkg::Use-Pty", true) == false)
   {
      d->master = -1;
      if (d->slave != NULL)
	 free(d->slave);
      d->slave = NULL;
      return;
   }

   if (isatty(STDIN_FILENO) == 0)
      d->direct_stdin = true;

   _error->PushToStack();

   d->master = posix_openpt(O_RDWR | O_NOCTTY);
   if (d->master == -1)
      _error->Errno("posix_openpt", _("Can not write log (%s)"), _("Is /dev/pts mounted?"));
   else if (unlockpt(d->master) == -1)
      _error->Errno("unlockpt", "Unlocking the slave of master fd %d failed!", d->master);
   else
   {
#ifdef HAVE_PTSNAME_R
      char slave_name[64];	// 64 is used by bionic
      if (ptsname_r(d->master, slave_name, sizeof(slave_name)) != 0)
#else
      char const * const slave_name = ptsname(d->master);
      if (slave_name == NULL)
#endif
	 _error->Errno("ptsname", "Getting name for slave of master fd %d failed!", d->master);
      else
      {
	 d->slave = strdup(slave_name);
	 if (d->slave == NULL)
	    _error->Errno("strdup", "Copying name %s for slave of master fd %d failed!", slave_name, d->master);
	 else if (grantpt(d->master) == -1)
	    _error->Errno("grantpt", "Granting access to slave %s based on master fd %d failed!", slave_name, d->master);
	 else if (tcgetattr(STDIN_FILENO, &d->tt) == 0)
	 {
	    d->tt_is_valid = true;
	    struct termios raw_tt;
	    // copy window size of stdout if its a 'good' terminal
	    if (tcgetattr(STDOUT_FILENO, &raw_tt) == 0)
	    {
	       struct winsize win;
	       if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) < 0)
		  _error->Errno("ioctl", "Getting TIOCGWINSZ from stdout failed!");
	       if (ioctl(d->master, TIOCSWINSZ, &win) < 0)
		  _error->Errno("ioctl", "Setting TIOCSWINSZ for master fd %d failed!", d->master);
	    }
	    if (tcsetattr(d->master, TCSANOW, &d->tt) == -1)
	       _error->Errno("tcsetattr", "Setting in Start via TCSANOW for master fd %d failed!", d->master);

	    raw_tt = d->tt;
	    cfmakeraw(&raw_tt);
	    raw_tt.c_lflag &= ~ECHO;
	    raw_tt.c_lflag |= ISIG;
	    // block SIGTTOU during tcsetattr to prevent a hang if
	    // the process is a member of the background process group
	    // http://www.opengroup.org/onlinepubs/000095399/functions/tcsetattr.html
	    sigemptyset(&d->sigmask);
	    sigaddset(&d->sigmask, SIGTTOU);
	    sigprocmask(SIG_BLOCK,&d->sigmask, &d->original_sigmask);
	    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_tt) == -1)
	       _error->Errno("tcsetattr", "Setting in Start via TCSAFLUSH for stdin failed!");
	    sigprocmask(SIG_SETMASK, &d->original_sigmask, NULL);

	 }
	 if (d->slave != NULL)
	 {
	    /* on linux, closing (and later reopening) all references to the slave
	       makes the slave a death end, so we open it here to have one open all
	       the time. We could use this fd in SetupSlavePtyMagic() for linux, but
	       on kfreebsd we get an incorrect ("step like") output then while it has
	       no problem with closing all references… so to avoid platform specific
	       code here we combine both and be happy once more */
	    d->protect_slave_from_dying = open(d->slave, O_RDWR | O_CLOEXEC | O_NOCTTY);
	 }
      }
   }

   if (_error->PendingError() == true)
   {
      if (d->master != -1)
      {
	 close(d->master);
	 d->master = -1;
      }
      if (d->slave != NULL)
      {
	 free(d->slave);
	 d->slave = NULL;
      }
      _error->DumpErrors(std::cerr, GlobalError::DEBUG, false);
   }
   _error->RevertToStack();
}
									/*}}}*/
void pkgDPkgPM::SetupSlavePtyMagic()					/*{{{*/
{
   if(d->master == -1 || d->slave == NULL)
      return;

   if (close(d->master) == -1)
      _error->FatalE("close", "Closing master %d in child failed!", d->master);
   d->master = -1;
   if (setsid() == -1)
      _error->FatalE("setsid", "Starting a new session for child failed!");

   int const slaveFd = open(d->slave, O_RDWR | O_NOCTTY);
   if (slaveFd == -1)
      _error->FatalE("open", _("Can not write log (%s)"), _("Is /dev/pts mounted?"));
   else if (ioctl(slaveFd, TIOCSCTTY, 0) < 0)
      _error->FatalE("ioctl", "Setting TIOCSCTTY for slave fd %d failed!", slaveFd);
   else
   {
      unsigned short i = 0;
      if (d->direct_stdin == true)
	 ++i;
      for (; i < 3; ++i)
	 if (dup2(slaveFd, i) == -1)
	    _error->FatalE("dup2", "Dupping %d to %d in child failed!", slaveFd, i);

      if (d->tt_is_valid == true && tcsetattr(STDIN_FILENO, TCSANOW, &d->tt) < 0)
	 _error->FatalE("tcsetattr", "Setting in Setup via TCSANOW for slave fd %d failed!", slaveFd);
   }

   if (slaveFd != -1)
      close(slaveFd);
}
									/*}}}*/
void pkgDPkgPM::StopPtyMagic()						/*{{{*/
{
   if (d->slave != NULL)
      free(d->slave);
   d->slave = NULL;
   if (d->protect_slave_from_dying != -1)
   {
      close(d->protect_slave_from_dying);
      d->protect_slave_from_dying = -1;
   }
   if(d->master >= 0)
   {
      if (d->tt_is_valid == true && tcsetattr(STDIN_FILENO, TCSAFLUSH, &d->tt) == -1)
	 _error->FatalE("tcsetattr", "Setting in Stop via TCSAFLUSH for stdin failed!");
      close(d->master);
      d->master = -1;
   }
}
									/*}}}*/
static void cleanUpTmpDir(char * const tmpdir)				/*{{{*/
{
   if (tmpdir == nullptr)
      return;
   DIR * const D = opendir(tmpdir);
   if (D == nullptr)
      _error->Errno("opendir", _("Unable to read %s"), tmpdir);
   else
   {
      auto const dfd = dirfd(D);
      for (struct dirent *Ent = readdir(D); Ent != nullptr; Ent = readdir(D))
      {
	 if (Ent->d_name[0] == '.')
	    continue;
#ifdef _DIRENT_HAVE_D_TYPE
	 if (unlikely(Ent->d_type != DT_LNK && Ent->d_type != DT_UNKNOWN))
	    continue;
#endif
	 if (unlikely(unlinkat(dfd, Ent->d_name, 0) != 0))
	    break;
      }
      closedir(D);
      rmdir(tmpdir);
   }
   free(tmpdir);
}
									/*}}}*/

// DPkgPM::Go - Run the sequence					/*{{{*/
// ---------------------------------------------------------------------
/* This globs the operations and calls dpkg
 *
 * If it is called with a progress object apt will report the install
 * progress to this object. It maps the dpkg states a package goes
 * through to human readable (and i10n-able)
 * names and calculates a percentage for each step.
 */
static bool ItemIsEssential(pkgDPkgPM::Item const &I)
{
   static auto const cachegen = _config->Find("pkgCacheGen::Essential");
   if (cachegen == "none" || cachegen == "native")
      return true;
   if (unlikely(I.Pkg.end()))
      return true;
   return (I.Pkg->Flags & pkgCache::Flag::Essential) != 0;
}
static bool ItemIsProtected(pkgDPkgPM::Item const &I)
{
   static auto const cachegen = _config->Find("pkgCacheGen::Protected");
   if (cachegen == "none" || cachegen == "native")
      return true;
   if (unlikely(I.Pkg.end()))
      return true;
   return (I.Pkg->Flags & pkgCache::Flag::Important) != 0;
}
bool pkgDPkgPM::ExpandPendingCalls(std::vector<Item> &List, pkgDepCache &Cache)
{
   {
      std::unordered_set<decltype(pkgCache::Package::ID)> alreadyRemoved;
      for (auto && I : List)
	 if (I.Op == Item::Remove || I.Op == Item::Purge)
	    alreadyRemoved.insert(I.Pkg->ID);
      std::remove_reference<decltype(List)>::type AppendList;
      for (auto Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
	 if (Cache[Pkg].Delete() && alreadyRemoved.insert(Pkg->ID).second == true)
	    AppendList.emplace_back(Cache[Pkg].Purge() ? Item::Purge : Item::Remove, Pkg);
      std::move(AppendList.begin(), AppendList.end(), std::back_inserter(List));
   }
   {
      std::unordered_set<decltype(pkgCache::Package::ID)> alreadyConfigured;
      for (auto && I : List)
	 if (I.Op == Item::Configure)
	    alreadyConfigured.insert(I.Pkg->ID);
      std::remove_reference<decltype(List)>::type AppendList;
      for (auto && I : List)
	 if (I.Op == Item::Install && alreadyConfigured.insert(I.Pkg->ID).second == true)
	    AppendList.emplace_back(Item::Configure, I.Pkg);
      for (auto Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
	 if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure &&
	       Cache[Pkg].Delete() == false && alreadyConfigured.insert(Pkg->ID).second == true)
	    AppendList.emplace_back(Item::Configure, Pkg);
      std::move(AppendList.begin(), AppendList.end(), std::back_inserter(List));
   }
   return true;
}
class APT_HIDDEN BuildDpkgCall {
   std::vector<char*> args;
   std::vector<bool> to_free;
   size_t baseArguments = 0;
   size_t baseArgumentsLen = 0;
   size_t len = 0;
public:
   void clearCallArguments() {
      for (size_t i = baseArguments; i < args.size(); ++i)
	 if (to_free[i])
	    std::free(args[i]);
      args.erase(args.begin() + baseArguments, args.end());
      to_free.erase(to_free.begin() + baseArguments, to_free.end());
      len = baseArgumentsLen;
   }
   void reserve(size_t const n) {
      args.reserve(n);
      to_free.reserve(n);
   }
   void push_back(char const * const str) {
      args.push_back(const_cast<char*>(str));
      to_free.push_back(false);
      len += strlen(args.back());
   }
   void push_back(std::string &&str) {
      args.push_back(strdup(str.c_str()));
      to_free.push_back(true);
      len += str.length();
   }
   auto bytes() const { return len; }
   auto data() const { return args.data(); }
   auto begin() const { return args.cbegin(); }
   auto end() const { return args.cend(); }
   auto& front() const { return args.front(); }
   APT_NORETURN void execute(char const *const errmsg) {
      args.push_back(nullptr);
      execvp(args.front(), &args.front());
      std::cerr << errmsg << std::endl;
      _exit(100);
   }
   BuildDpkgCall() {
      for (auto &&arg : debSystem::GetDpkgBaseCommand())
	 push_back(std::move(arg));
      baseArguments = args.size();
      baseArgumentsLen = len;
   }
   BuildDpkgCall(BuildDpkgCall const &) = delete;
   BuildDpkgCall(BuildDpkgCall &&) = delete;
   BuildDpkgCall& operator=(BuildDpkgCall const &) = delete;
   BuildDpkgCall& operator=(BuildDpkgCall &&) = delete;
   ~BuildDpkgCall() {
      baseArguments = 0;
      clearCallArguments();
   }
};
bool pkgDPkgPM::Go(APT::Progress::PackageManager *progress)
{
   struct Inhibitor
   {
      int Fd = -1;
      Inhibitor()
      {
	 if (_config->FindB("DPkg::Inhibit-Shutdown", true))
	    Fd = Inhibit("shutdown", "APT", "APT is installing or removing packages", "block");
      }
      ~Inhibitor()
      {
	 if (Fd > 0)
	    close(Fd);
      }
   } inhibitor;

   // explicitly remove&configure everything for hookscripts and progress building
   // we need them only temporarily through, so keep the length and erase afterwards
   decltype(List)::const_iterator::difference_type explicitIdx =
      std::distance(List.cbegin(), List.cend());
   ExpandPendingCalls(List, Cache);

   /* if dpkg told us that it has already done everything to the package we wanted it to do,
      we shouldn't ask it for "more" later. That can e.g. happen if packages without conffiles
      are purged as they will have pass through the purge states on remove already */
   auto const StripAlreadyDoneFrom = [&](APT::VersionVector & Pending) {
      Pending.erase(std::remove_if(Pending.begin(), Pending.end(), [&](pkgCache::VerIterator const &Ver) {
	    auto const PN = Ver.ParentPkg().FullName();
	    auto const POD = PackageOpsDone.find(PN);
	    if (POD == PackageOpsDone.end())
	       return false;
	    return PackageOps[PN].size() <= POD->second;
	 }), Pending.end());
   };

   pkgPackageManager::SigINTStop = false;
   d->progress = progress;

   // try to figure out the max environment size
   int OSArgMax = sysconf(_SC_ARG_MAX);
   if(OSArgMax < 0)
      OSArgMax = 32*1024;
   OSArgMax -= EnvironmentSize() - 2*1024;
   unsigned int const MaxArgBytes = _config->FindI("Dpkg::MaxArgBytes", OSArgMax);
   bool const NoTriggers = _config->FindB("DPkg::NoTriggers", true);

   if (RunScripts("DPkg::Pre-Invoke") == false)
      return false;

   if (RunScriptsWithPkgs("DPkg::Pre-Install-Pkgs") == false)
      return false;

   auto const noopDPkgInvocation = _config->FindB("Debug::pkgDPkgPM",false);
   // store auto-bits as they are supposed to be after dpkg is run
   if (noopDPkgInvocation == false)
      Cache.writeStateFile(NULL);

   bool dpkg_recursive_install = _config->FindB("dpkg::install::recursive", false);
   if (_config->FindB("dpkg::install::recursive::force", false) == false)
   {
      // dpkg uses a sorted treewalk since that version which enables the workaround to work
      auto const dpkgpkg = Cache.FindPkg("dpkg");
      if (likely(dpkgpkg.end() == false && dpkgpkg->CurrentVer != 0))
	 dpkg_recursive_install = Cache.VS().CmpVersion("1.18.5", dpkgpkg.CurrentVer().VerStr()) <= 0;
   }
   // no point in doing this dance for a handful of packages only
   unsigned int const dpkg_recursive_install_min = _config->FindI("dpkg::install::recursive::minimum", 5);
   // FIXME: workaround for dpkg bug, see our ./test-bug-740843-versioned-up-down-breaks test
   bool const dpkg_recursive_install_numbered = _config->FindB("dpkg::install::recursive::numbered", true);

   // for the progress
   BuildPackagesProgressMap();

   APT::StateChanges approvedStates;
   if (_config->FindB("dpkg::selection::remove::approved", true))
   {
      for (auto && I : List)
	 if (I.Op == Item::Purge)
	    approvedStates.Purge(FindToBeRemovedVersion(I.Pkg));
	 else if (I.Op == Item::Remove)
	    approvedStates.Remove(FindToBeRemovedVersion(I.Pkg));
   }

   // Skip removes if we install another architecture of this package soon (crossgrade)
   // We can't just skip them all the time as it could be an ordering requirement [of another package]
   if ((approvedStates.Remove().empty() == false || approvedStates.Purge().empty() == false) &&
	 _config->FindB("dpkg::remove::crossgrade::implicit", true) == true)
   {
      std::unordered_set<decltype(pkgCache::Package::ID)> crossgraded;
      std::vector<std::pair<Item*, std::string>> toCrossgrade;
      auto const PlanedEnd = std::next(List.begin(), explicitIdx);
      for (auto I = List.begin(); I != PlanedEnd; ++I)
      {
	 if (I->Op != Item::Remove && I->Op != Item::Purge)
	    continue;

	 auto const Grp = I->Pkg.Group();
	 size_t installedInstances = 0, wannabeInstances = 0;
	 bool multiArchInstances = false;
	 for (auto Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
	 {
	    if (Pkg->CurrentVer != 0)
	    {
	       ++installedInstances;
	       if (Cache[Pkg].Delete() == false)
		  ++wannabeInstances;
	    }
	    else if (PackageOps.find(Pkg.FullName()) != PackageOps.end())
	       ++wannabeInstances;
	    if (multiArchInstances == false)
	    {
	       auto const V = Cache[Pkg].InstVerIter(Cache);
	       if (V.end() == false && (Pkg->CurrentVer == 0 || V != Pkg.CurrentVer()))
		  multiArchInstances = ((V->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same);
	    }
	 }
	 /* theoretically the installed check would be enough as some wannabe will
	    be first and hence be the crossgrade we were looking for, but #844300
	    prevents this so we keep these situations explicit removes.
	    It is also the reason why neither of them can be a M-A:same package */
	 if (installedInstances == 1 && wannabeInstances == 1 && multiArchInstances == false)
	 {
	    auto const FirstInstall = std::find_if_not(I, List.end(),
		  [](Item const &i) { return i.Op == Item::Remove || i.Op == Item::Purge; });
	    auto const LastInstall = std::find_if_not(FirstInstall, List.end(),
		  [](Item const &i) { return i.Op == Item::Install; });
	    auto const crosser = std::find_if(FirstInstall, LastInstall,
		  [&I](Item const &i) { return i.Pkg->Group == I->Pkg->Group; });
	    if (crosser != LastInstall)
	    {
	       crossgraded.insert(I->Pkg->ID);
	       toCrossgrade.emplace_back(&(*I), crosser->Pkg.FullName());
	    }
	 }
      }
      for (auto I = PlanedEnd; I != List.end(); ++I)
      {
	 if (I->Op != Item::Remove && I->Op != Item::Purge)
	    continue;

	 auto const Grp = I->Pkg.Group();
	 for (auto Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
	 {
	    if (Pkg == I->Pkg || Cache[Pkg].Install() == false)
	       continue;
	    toCrossgrade.emplace_back(&(*I), Pkg.FullName());
	    break;
	 }
      }
      for (auto C : toCrossgrade)
      {
	 // we never do purges on packages which are crossgraded, even if "requested"
	 if (C.first->Op == Item::Purge)
	 {
	    C.first->Op = Item::Remove; // crossgrades should never be purged
	    auto && Purges = approvedStates.Purge();
	    auto const Ver = std::find_if(
#if __GNUC__ >= 5 || (__GNUC_MINOR__ >= 9 && __GNUC__ >= 4)
	       Purges.cbegin(), Purges.cend(),
#else
	       Purges.begin(), Purges.end(),
#endif
	       [&C](pkgCache::VerIterator const &V) { return V.ParentPkg() == C.first->Pkg; });
	    approvedStates.Remove(*Ver);
	    Purges.erase(Ver);
	    auto && RemOp = PackageOps[C.first->Pkg.FullName()];
	    if (RemOp.size() == 4)
	    {
	       RemOp.erase(std::next(RemOp.begin(), 2), RemOp.end());
	       PackagesTotal -= 2;
	    }
	    else
	    _error->Warning("Unexpected amount of planned ops for package %s: %lu", C.first->Pkg.FullName().c_str(), RemOp.size());
	 }
      }
      if (crossgraded.empty() == false)
      {
	 auto const oldsize = List.size();
	 List.erase(std::remove_if(List.begin(), PlanedEnd,
	       [&crossgraded](Item const &i){
		  return (i.Op == Item::Remove || i.Op == Item::Purge) &&
		     crossgraded.find(i.Pkg->ID) != crossgraded.end();
	       }), PlanedEnd);
	 explicitIdx -= (oldsize - List.size());
      }
   }

   APT::StateChanges currentStates;
   if (_config->FindB("dpkg::selection::current::saveandrestore", true))
   {
      for (auto Pkg = Cache.PkgBegin(); Pkg.end() == false; ++Pkg)
	 if (Pkg->CurrentVer == 0)
	    continue;
	 else if (Pkg->SelectedState == pkgCache::State::Purge)
	    currentStates.Purge(FindToBeRemovedVersion(Pkg));
	 else if (Pkg->SelectedState == pkgCache::State::DeInstall)
	    currentStates.Remove(FindToBeRemovedVersion(Pkg));
      if (currentStates.empty() == false)
      {
	 APT::StateChanges cleanStates;
	 for (auto && P: currentStates.Remove())
	    cleanStates.Install(P);
	 for (auto && P: currentStates.Purge())
	    cleanStates.Install(P);
	 if (cleanStates.Save(false) == false)
	    return _error->Error("Couldn't clean the currently selected dpkg states");
      }
   }

   if (_config->FindB("dpkg::selection::remove::approved", true))
   {
      if (approvedStates.Save(false) == false)
      {
	 _error->Error("Couldn't record the approved state changes as dpkg selection states");
	 if (currentStates.Save(false) == false)
	    _error->Error("Couldn't restore dpkg selection states which were present before this interaction!");
	 return false;
      }

      List.erase(std::next(List.begin(), explicitIdx), List.end());

      std::vector<bool> toBeRemoved(Cache.Head().PackageCount, false);
      for (auto && I: approvedStates.Remove())
	 toBeRemoved[I.ParentPkg()->ID] = true;
      for (auto && I: approvedStates.Purge())
	 toBeRemoved[I.ParentPkg()->ID] = true;

      for (auto && I: List)
	 if (I.Op == Item::Remove || I.Op == Item::Purge)
	    toBeRemoved[I.Pkg->ID] = false;

      bool const RemovePending = std::find(toBeRemoved.begin(), toBeRemoved.end(), true) != toBeRemoved.end();
      bool const PurgePending = approvedStates.Purge().empty() == false;
      if (RemovePending != false || PurgePending != false)
	 List.emplace_back(Item::ConfigurePending, pkgCache::PkgIterator());
      if (RemovePending)
	 List.emplace_back(Item::RemovePending, pkgCache::PkgIterator());
      if (PurgePending)
	 List.emplace_back(Item::PurgePending, pkgCache::PkgIterator());

      // support subpressing of triggers processing for special
      // cases like d-i that runs the triggers handling manually
      if (_config->FindB("DPkg::ConfigurePending", true))
	 List.emplace_back(Item::ConfigurePending, pkgCache::PkgIterator());
   }
   bool const TriggersPending = _config->FindB("DPkg::TriggersPending", false);

   d->stdin_is_dev_null = false;

   // create log
   OpenLog();

   bool dpkgMultiArch = _system->MultiArchSupported();
   bool dpkgProtectedField = debSystem::AssertFeature("protected-field");

   // start pty magic before the loop
   StartPtyMagic();

   // Tell the progress that its starting and fork dpkg
   d->progress->Start(d->master);

   // this loop is runs once per dpkg operation
   vector<Item>::const_iterator I = List.cbegin();
   BuildDpkgCall Args;
   while (I != List.end())
   {
      // Do all actions with the same Op in one run
      vector<Item>::const_iterator J = I;
      if (TriggersPending == true)
	 for (; J != List.end(); ++J)
	 {
	    if (J->Op == I->Op)
	       continue;
	    if (J->Op != Item::TriggersPending)
	       break;
	    vector<Item>::const_iterator T = J + 1;
	    if (T != List.end() && T->Op == I->Op)
	       continue;
	    break;
	 }
      else if (J->Op == Item::Remove || J->Op == Item::Purge)
	 J = std::find_if(J, List.cend(), [](Item const &I) { return I.Op != Item::Remove && I.Op != Item::Purge; });
      else
	 J = std::find_if(J, List.cend(), [&J](Item const &I) { return I.Op != J->Op; });

      Args.clearCallArguments();
      Args.reserve((J - I) + 10);

      int fd[2];
      if (pipe(fd) != 0)
	 return _error->Errno("pipe","Failed to create IPC pipe to dpkg");

      Args.push_back("--status-fd");
      Args.push_back(std::to_string(fd[1]));
      unsigned long const Op = I->Op;

      if (NoTriggers == true && I->Op != Item::TriggersPending &&
	  (I->Op != Item::ConfigurePending || std::next(I) != List.end()))
	 Args.push_back("--no-triggers");

      switch (I->Op)
      {
	 case Item::Remove:
	 case Item::Purge:
	 Args.push_back("--force-depends");
	 Args.push_back("--abort-after=1");
	 if (std::any_of(I, J, ItemIsEssential))
	    Args.push_back("--force-remove-essential");
	 if (dpkgProtectedField && std::any_of(I, J, ItemIsProtected))
	    Args.push_back("--force-remove-protected");
	 Args.push_back("--remove");
	 break;

	 case Item::Configure:
	 Args.push_back("--configure");
	 break;

	 case Item::ConfigurePending:
	 Args.push_back("--configure");
	 Args.push_back("--pending");
	 break;

	 case Item::TriggersPending:
	 Args.push_back("--triggers-only");
	 Args.push_back("--pending");
	 break;

	 case Item::RemovePending:
	 Args.push_back("--remove");
	 Args.push_back("--pending");
	 break;

	 case Item::PurgePending:
	 Args.push_back("--purge");
	 Args.push_back("--pending");
	 break;

	 case Item::Install:
	 Args.push_back("--unpack");
	 Args.push_back("--auto-deconfigure");
	 break;
      }

      std::unique_ptr<char, decltype(&cleanUpTmpDir)> tmpdir_for_dpkg_recursive{nullptr, &cleanUpTmpDir};
      std::string const dpkg_chroot_dir = _config->FindDir("DPkg::Chroot-Directory", "/");

      // Write in the file or package names
      if (I->Op == Item::Install)
      {
	 auto const installsToDo = J - I;
	 if (dpkg_recursive_install == true && dpkg_recursive_install_min < installsToDo)
	 {
	    {
	       std::string basetmpdir = (dpkg_chroot_dir == "/") ? GetTempDir() : flCombine(dpkg_chroot_dir, "tmp");
	       std::string tmpdir;
	       strprintf(tmpdir, "%s/apt-dpkg-install-XXXXXX", basetmpdir.c_str());
	       tmpdir_for_dpkg_recursive.reset(strndup(tmpdir.data(), tmpdir.length()));
	       if (mkdtemp(tmpdir_for_dpkg_recursive.get()) == nullptr)
		  return _error->Errno("DPkg::Go", "mkdtemp of %s failed in preparation of calling dpkg unpack", tmpdir_for_dpkg_recursive.get());
	    }

	    char p = 1;
	    for (auto c = installsToDo - 1; (c = c/10) != 0; ++p);
	    for (unsigned long n = 0; I != J; ++n, ++I)
	    {
	       if (I->File[0] != '/')
		  return _error->Error("Internal Error, Pathname to install is not absolute '%s'",I->File.c_str());
	       auto file = flNotDir(I->File);
	       if (flExtension(file) != "deb")
		  file.append(".deb");
	       std::string linkpath;
	       if (dpkg_recursive_install_numbered)
		  strprintf(linkpath, "%s/%.*lu-%s", tmpdir_for_dpkg_recursive.get(), p, n, file.c_str());
	       else
		  strprintf(linkpath, "%s/%s", tmpdir_for_dpkg_recursive.get(), file.c_str());
	       std::string linktarget = I->File;
	       if (dpkg_chroot_dir != "/") {
		  char * fakechroot = getenv("FAKECHROOT");
		  if (fakechroot != nullptr && strcmp(fakechroot, "true") == 0) {
		     // if apt is run with DPkg::Chroot-Directory under
		     // fakechroot, absolulte symbolic links must be prefixed
		     // with the chroot path to be valid inside fakechroot
		     strprintf(linktarget, "%s/%s", dpkg_chroot_dir.c_str(), I->File.c_str());
		  }
	       }
	       if (symlink(linktarget.c_str(), linkpath.c_str()) != 0)
		  return _error->Errno("DPkg::Go", "Symlinking %s to %s failed!", linktarget.c_str(), linkpath.c_str());
	    }
	    Args.push_back("--recursive");
	    Args.push_back(debSystem::StripDpkgChrootDirectory(tmpdir_for_dpkg_recursive.get()));
	 }
	 else
	 {
	    for (;I != J && Args.bytes() < MaxArgBytes; ++I)
	    {
	       if (I->File[0] != '/')
		  return _error->Error("Internal Error, Pathname to install is not absolute '%s'",I->File.c_str());
	       Args.push_back(I->File.c_str());
	    }
	 }
      }
      else if (I->Op == Item::RemovePending)
      {
	 ++I;
	 StripAlreadyDoneFrom(approvedStates.Remove());
	 if (approvedStates.Remove().empty())
	    continue;
      }
      else if (I->Op == Item::PurgePending)
      {
	 ++I;
	 // explicit removes of packages without conffiles passthrough the purge states instantly, too.
	 // Setting these non-installed packages up for purging generates 'unknown pkg' warnings from dpkg
	 StripAlreadyDoneFrom(approvedStates.Purge());
	 if (approvedStates.Purge().empty())
	    continue;
	 std::remove_reference<decltype(approvedStates.Remove())>::type approvedRemoves;
	 std::swap(approvedRemoves, approvedStates.Remove());
	 // we apply it again here as an explicit remove in the ordering will have cleared the purge state
	 if (approvedStates.Save(false) == false)
	 {
	    _error->Error("Couldn't record the approved purges as dpkg selection states");
	    if (currentStates.Save(false) == false)
	       _error->Error("Couldn't restore dpkg selection states which were present before this interaction!");
	    return false;
	 }
	 std::swap(approvedRemoves, approvedStates.Remove());
      }
      else
      {
	 string const nativeArch = _config->Find("APT::Architecture");
	 auto const oldSize = I->Pkg.end() ? 0ull : Args.bytes();
	 for (;I != J && Args.bytes() < MaxArgBytes; ++I)
	 {
	    if((*I).Pkg.end() == true)
	       continue;
	    if (I->Op == Item::Configure && disappearedPkgs.find(I->Pkg.FullName(true)) != disappearedPkgs.end())
	       continue;
	    // We keep this here to allow "smooth" transitions from e.g. multiarch dpkg/ubuntu to dpkg/debian
	    if (dpkgMultiArch == false && (I->Pkg.Arch() == nativeArch ||
					   strcmp(I->Pkg.Arch(), "all") == 0 ||
					   strcmp(I->Pkg.Arch(), "none") == 0))
	       Args.push_back(I->Pkg.Name());
	    else if (Op == Item::Purge && I->Pkg->CurrentVer == 0)
	       continue; // we purge later with --purge --pending, so if it isn't installed (aka rc-only), skip it here
	    else if (strcmp(I->Pkg.Arch(), "none") == 0)
	       Args.push_back(I->Pkg.Name()); // never arch-qualify a package without an arch
	    else
	    {
	       pkgCache::VerIterator PkgVer;
	       if (Op == Item::Remove || Op == Item::Purge)
		  PkgVer = I->Pkg.CurrentVer();
	       else
		  PkgVer = Cache[I->Pkg].InstVerIter(Cache);
	       if (PkgVer.end())
	       {
		  _error->Warning("Can not find PkgVer for '%s'", I->Pkg.Name());
		  Args.push_back(I->Pkg.Name());
		  continue;
	       }
	       Args.push_back(std::string(I->Pkg.Name()) + ":" + PkgVer.Arch());
	    }
	 }
	 // skip configure action if all scheduled packages disappeared
	 if (oldSize == Args.bytes())
	    continue;
      }

      J = I;

      if (noopDPkgInvocation == true)
      {
	 for (auto const a : Args)
	    clog << a << ' ';
	 clog << endl;
	 close(fd[0]);
	 close(fd[1]);
	 continue;
      }
      cout << flush;
      clog << flush;
      cerr << flush;

      /* Mask off sig int/quit. We do this because dpkg also does when
         it forks scripts. What happens is that when you hit ctrl-c it sends
	 it to all processes in the group. Since dpkg ignores the signal
	 it doesn't die but we do! So we must also ignore it */
      sighandler_t old_SIGQUIT = signal(SIGQUIT,SIG_IGN);
      sighandler_t old_SIGINT = signal(SIGINT,SigINT);

      // Check here for any SIGINT
      if (pkgPackageManager::SigINTStop && (Op == Item::Remove || Op == Item::Purge || Op == Item::Install))
         break;

      // ignore SIGHUP as well (debian #463030)
      sighandler_t old_SIGHUP = signal(SIGHUP,SIG_IGN);

      // now run dpkg
      d->progress->StartDpkg();
      std::set<int> KeepFDs;
      KeepFDs.insert(fd[1]);
      MergeKeepFdsFromConfiguration(KeepFDs);
      pid_t Child = ExecFork(KeepFDs);
      if (Child == 0)
      {
	 // This is the child
	 SetupSlavePtyMagic();
	 close(fd[0]); // close the read end of the pipe

	 debSystem::DpkgChrootDirectory();

	 if (chdir(_config->FindDir("DPkg::Run-Directory","/").c_str()) != 0)
	    _exit(100);

	 if (_config->FindB("DPkg::FlushSTDIN",true) == true && isatty(STDIN_FILENO))
	 {
	    int Flags;
            int dummy = 0;
	    if ((Flags = fcntl(STDIN_FILENO,F_GETFL,dummy)) < 0)
	       _exit(100);

	    // Discard everything in stdin before forking dpkg
	    if (fcntl(STDIN_FILENO,F_SETFL,Flags | O_NONBLOCK) < 0)
	       _exit(100);

	    while (read(STDIN_FILENO,&dummy,1) == 1);

	    if (fcntl(STDIN_FILENO,F_SETFL,Flags & (~(long)O_NONBLOCK)) < 0)
	       _exit(100);
	 }

	 // if color support isn't enabled/disabled explicitly tell
	 // dpkg to use the same state apt is using for its color support
	 if (_config->FindB("APT::Color", false) == true)
	    setenv("DPKG_COLORS", "always", 0);
	 else
	    setenv("DPKG_COLORS", "never", 0);

	 if (_system->IsLocked() == true) {
	    setenv("DPKG_FRONTEND_LOCKED", "true", 1);
	 }
	 if (_config->Find("DPkg::Path", "").empty() == false)
	    setenv("PATH", _config->Find("DPkg::Path", "").c_str(), 1);

	 Args.execute("Could not exec dpkg!");
      }

      // we read from dpkg here
      int const _dpkgin = fd[0];
      close(fd[1]);                        // close the write end of the pipe
      d->status_fd_reached_end_of_file = false;

      // apply ionice
      if (_config->FindB("DPkg::UseIoNice", false) == true)
	 ionice(Child);

      // setups fds
      sigemptyset(&d->sigmask);
      sigprocmask(SIG_BLOCK,&d->sigmask,&d->original_sigmask);

      // the result of the waitpid call
      int Status = 0;
      int res;
      bool waitpid_failure = false;
      bool dpkg_finished = false;
      do
      {
	 if (dpkg_finished == false)
	 {
	    if ((res = waitpid(Child, &Status, WNOHANG)) == Child)
	       dpkg_finished = true;
	    else if (res < 0)
	    {
	       // error handling, waitpid returned -1
	       if (errno == EINTR)
		  continue;
	       waitpid_failure = true;
	       break;
	    }
	 }
	 if (dpkg_finished && d->status_fd_reached_end_of_file)
	    break;

	 // wait for input or output here
	 fd_set rfds;
	 FD_ZERO(&rfds);
	 if (d->master >= 0 && d->direct_stdin == false && d->stdin_is_dev_null == false)
	    FD_SET(STDIN_FILENO, &rfds);
	 FD_SET(_dpkgin, &rfds);
	 if(d->master >= 0)
	    FD_SET(d->master, &rfds);
	 struct timespec tv;
	 tv.tv_sec = 0;
	 tv.tv_nsec = d->progress->GetPulseInterval();
	 auto const select_ret = pselect(max(d->master, _dpkgin)+1, &rfds, NULL, NULL,
			      &tv, &d->original_sigmask);
         d->progress->Pulse();
	 if (select_ret == 0)
	    continue;
	 else if (select_ret < 0 && errno == EINTR)
	    continue;
	 else if (select_ret < 0)
	 {
	    perror("select() returned error");
	    continue;
	 }

	 if(d->master >= 0 && FD_ISSET(d->master, &rfds))
	    DoTerminalPty(d->master);
	 if(d->master >= 0 && FD_ISSET(0, &rfds))
	    DoStdin(d->master);
	 if(FD_ISSET(_dpkgin, &rfds))
	    DoDpkgStatusFd(_dpkgin);

      } while (true);
      close(_dpkgin);

      // Restore sig int/quit
      signal(SIGQUIT,old_SIGQUIT);
      signal(SIGINT,old_SIGINT);
      signal(SIGHUP,old_SIGHUP);

      if (waitpid_failure == true)
      {
	 strprintf(d->dpkg_error, "Sub-process %s couldn't be waited for.",Args.front());
	 _error->Error("%s", d->dpkg_error.c_str());
	 break;
      }

      // Check for an error code.
      if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      {
	 // if it was set to "keep-dpkg-running" then we won't return
	 // here but keep the loop going and just report it as a error
	 // for later
	 bool const stopOnError = _config->FindB("Dpkg::StopOnError",true);

	 if (WIFSIGNALED(Status) != 0 && WTERMSIG(Status) == SIGSEGV)
	    strprintf(d->dpkg_error, "Sub-process %s received a segmentation fault.",Args.front());
	 else if (WIFEXITED(Status) != 0)
	    strprintf(d->dpkg_error, "Sub-process %s returned an error code (%u)",Args.front(),WEXITSTATUS(Status));
	 else
	    strprintf(d->dpkg_error, "Sub-process %s exited unexpectedly",Args.front());
	 _error->Error("%s", d->dpkg_error.c_str());

	 if(stopOnError)
	    break;
      }
   }
   // dpkg is done at this point
   StopPtyMagic();
   CloseLog();

   if (d->dpkg_error.empty() == false)
   {
      // no point in resetting packages we already completed removal for
      StripAlreadyDoneFrom(approvedStates.Remove());
      StripAlreadyDoneFrom(approvedStates.Purge());
      APT::StateChanges undo;
      auto && undoRem = approvedStates.Remove();
      std::move(undoRem.begin(), undoRem.end(), std::back_inserter(undo.Install()));
      auto && undoPur = approvedStates.Purge();
      std::move(undoPur.begin(), undoPur.end(), std::back_inserter(undo.Install()));
      approvedStates.clear();
      if (undo.Save(false) == false)
	 _error->Error("Couldn't revert dpkg selection for approved remove/purge after an error was encountered!");
   }

   StripAlreadyDoneFrom(currentStates.Remove());
   StripAlreadyDoneFrom(currentStates.Purge());
   if (currentStates.Save(false) == false)
      _error->Error("Couldn't restore dpkg selection states which were present before this interaction!");

   if (pkgPackageManager::SigINTStop)
       _error->Warning(_("Operation was interrupted before it could finish"));

   if (noopDPkgInvocation == false)
   {
      if (d->dpkg_error.empty() && (PackagesDone + 1) != PackagesTotal)
      {
	 std::string pkglist;
	 for (auto const &PO: PackageOps)
	    if (PO.second.size() != PackageOpsDone[PO.first])
	    {
	       if (pkglist.empty() == false)
		  pkglist.append(" ");
	       pkglist.append(PO.first);
	    }
	 /* who cares about correct progress? As we depend on it for skipping actions
	    our parsing should be correct. People will no doubt be confused if they see
	    this message, but the dpkg warning about unknown packages isn't much better
	    from a user POV and combined we might have a chance to figure out what is wrong */
	 _error->Warning("APT had planned for dpkg to do more than it reported back (%u vs %u).\n"
	       "Affected packages: %s", PackagesDone, PackagesTotal, pkglist.c_str());
      }

      std::string const oldpkgcache = _config->FindFile("Dir::cache::pkgcache");
      if (oldpkgcache.empty() == false && RealFileExists(oldpkgcache) == true &&
	  RemoveFile("pkgDPkgPM::Go", oldpkgcache))
      {
	 std::string const srcpkgcache = _config->FindFile("Dir::cache::srcpkgcache");
	 if (srcpkgcache.empty() == false && RealFileExists(srcpkgcache) == true)
	 {
	    _error->PushToStack();
	    pkgCacheFile CacheFile;
	    CacheFile.BuildCaches(NULL, true);
	    _error->RevertToStack();
	 }
      }
   }

   // disappearing packages can forward their auto-bit
   if (disappearedPkgs.empty() == false)
      Cache.writeStateFile(NULL);

   d->progress->Stop();

   if (RunScripts("DPkg::Post-Invoke") == false)
      return false;

   return d->dpkg_error.empty();
}

void SigINT(int /*sig*/) {
   pkgPackageManager::SigINTStop = true;
}
									/*}}}*/
// pkgDpkgPM::Reset - Dump the contents of the command list		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDPkgPM::Reset()
{
   List.erase(List.begin(),List.end());
}
									/*}}}*/
// pkgDpkgPM::WriteApportReport - write out error report pkg failure	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDPkgPM::WriteApportReport(const char *pkgpath, const char *errormsg)
{
   // If apport doesn't exist or isn't installed do nothing
   // This e.g. prevents messages in 'universes' without apport
   pkgCache::PkgIterator apportPkg = Cache.FindPkg("apport");
   if (apportPkg.end() == true || apportPkg->CurrentVer == 0)
      return;

   string pkgname, reportfile, pkgver, arch;
   string::size_type pos;
   FILE *report;

   if (_config->FindB("Dpkg::ApportFailureReport", true) == false)
   {
      std::clog << "configured to not write apport reports" << std::endl;
      return;
   }

   // only report the first errors
   if(pkgFailures > _config->FindI("APT::Apport::MaxReports", 3))
   {
      std::clog << _("No apport report written because MaxReports is reached already") << std::endl;
      return;
   }

   // check if its not a follow up error
   const char *needle = dgettext("dpkg", "dependency problems - leaving unconfigured");
   if(strstr(errormsg, needle) != NULL) {
      std::clog << _("No apport report written because the error message indicates its a followup error from a previous failure.") << std::endl;
      return;
   }

   // do not report disk-full failures
   if(strstr(errormsg, strerror(ENOSPC)) != NULL) {
      std::clog << _("No apport report written because the error message indicates a disk full error") << std::endl;
      return;
   }

   // do not report out-of-memory failures
   if(strstr(errormsg, strerror(ENOMEM)) != NULL ||
      strstr(errormsg, "failed to allocate memory") != NULL) {
      std::clog << _("No apport report written because the error message indicates a out of memory error") << std::endl;
      return;
   }

   // do not report bugs regarding inaccessible local files
   if(strstr(errormsg, strerror(ENOENT)) != NULL ||
      strstr(errormsg, "cannot access archive") != NULL) {
      std::clog << _("No apport report written because the error message indicates an issue on the local system") << std::endl;
      return;
   }

   // do not report errors encountered when decompressing packages
   if(strstr(errormsg, "--fsys-tarfile returned error exit status 2") != NULL) {
      std::clog << _("No apport report written because the error message indicates an issue on the local system") << std::endl;
      return;
   }

   // do not report dpkg I/O errors, this is a format string, so we compare
   // the prefix and the suffix of the error with the dpkg error message
   vector<string> io_errors;
   io_errors.push_back(string("failed to read"));
   io_errors.push_back(string("failed to write"));
   io_errors.push_back(string("failed to seek"));
   io_errors.push_back(string("unexpected end of file or stream"));

   for (vector<string>::iterator I = io_errors.begin(); I != io_errors.end(); ++I)
   {
      vector<string> list = VectorizeString(dgettext("dpkg", (*I).c_str()), '%');
      if (list.size() > 1) {
         // we need to split %s, VectorizeString only allows char so we need
         // to kill the "s" manually
         if (list[1].size() > 1) {
            list[1].erase(0, 1);
            if(strstr(errormsg, list[0].c_str()) &&
               strstr(errormsg, list[1].c_str())) {
               std::clog << _("No apport report written because the error message indicates a dpkg I/O error") << std::endl;
               return;
            }
         }
      }
   }

   // get the pkgname and reportfile
   pkgname = flNotDir(pkgpath);
   pos = pkgname.find('_');
   if(pos != string::npos)
      pkgname = pkgname.substr(0, pos);

   // find the package version and source package name
   pkgCache::PkgIterator Pkg = Cache.FindPkg(pkgname);
   if (Pkg.end() == true)
   {
      if (pos == std::string::npos || _config->FindB("dpkg::install::recursive::numbered", true) == false)
	 return;
      auto const dash = pkgname.find_first_not_of("0123456789");
      if (dash == std::string::npos || pkgname[dash] != '-')
	 return;
      pkgname.erase(0, dash + 1);
      Pkg = Cache.FindPkg(pkgname);
      if (Pkg.end() == true)
	 return;
   }
   pkgCache::VerIterator Ver = Cache.GetCandidateVersion(Pkg);
   if (Ver.end() == true)
      return;
   pkgver = Ver.VerStr() == NULL ? "unknown" : Ver.VerStr();

   // if the file exists already, we check:
   // - if it was reported already (touched by apport). 
   //   If not, we do nothing, otherwise
   //    we overwrite it. This is the same behaviour as apport
   // - if we have a report with the same pkgversion already
   //   then we skip it
   _config->CndSet("Dir::Apport", "var/crash");
   reportfile = flCombine(_config->FindDir("Dir::Apport", "var/crash"), pkgname+".0.crash");
   if(FileExists(reportfile))
   {
      struct stat buf;
      char strbuf[255];

      // check atime/mtime
      stat(reportfile.c_str(), &buf);
      if(buf.st_mtime > buf.st_atime)
	 return;

      // check if the existing report is the same version
      report = fopen(reportfile.c_str(),"r");
      while(fgets(strbuf, sizeof(strbuf), report) != NULL)
      {
	 if(strstr(strbuf,"Package:") == strbuf)
	 {
	    char pkgname[255], version[255];
	    if(sscanf(strbuf, "Package: %254s %254s", pkgname, version) == 2)
	       if(strcmp(pkgver.c_str(), version) == 0)
	       {
		  fclose(report);
		  return;
	       }
	 }
      }
      fclose(report);
   }

   // now write the report
   arch = _config->Find("APT::Architecture");
   report = fopen(reportfile.c_str(),"w");
   if(report == NULL)
      return;
   if(_config->FindB("DPkgPM::InitialReportOnly",false) == true)
      chmod(reportfile.c_str(), 0);
   else
      chmod(reportfile.c_str(), 0600);
   fprintf(report, "ProblemType: Package\n");
   fprintf(report, "Architecture: %s\n", arch.c_str());
   time_t now = time(NULL);
   char ctime_buf[26];	// need at least 26 bytes according to ctime(3)
   fprintf(report, "Date: %s" , ctime_r(&now, ctime_buf));
   fprintf(report, "Package: %s %s\n", pkgname.c_str(), pkgver.c_str());
   fprintf(report, "SourcePackage: %s\n", Ver.SourcePkgName());
   fprintf(report, "ErrorMessage:\n %s\n", errormsg);

   // ensure that the log is flushed
   if(d->term_out)
      fflush(d->term_out);

   // attach terminal log it if we have it
   string logfile_name = _config->FindFile("Dir::Log::Terminal", "/dev/null");
   if (logfile_name != "/dev/null")
   {
      FILE *log = NULL;

      fprintf(report, "DpkgTerminalLog:\n");
      log = fopen(logfile_name.c_str(),"r");
      if(log != NULL)
      {
	 char buf[1024];
	 while( fgets(buf, sizeof(buf), log) != NULL)
	    fprintf(report, " %s", buf);
         fprintf(report, " \n");
	 fclose(log);
      }
   }

   // attach history log it if we have it
   string histfile_name = _config->FindFile("Dir::Log::History", "/dev/null");
   if (histfile_name != "/dev/null")
   {
      fprintf(report, "DpkgHistoryLog:\n");
      FILE* log = fopen(histfile_name.c_str(),"r");
      if(log != NULL)
      {
	 char buf[1024];
	 while( fgets(buf, sizeof(buf), log) != NULL)
	    fprintf(report, " %s", buf);
	 fclose(log);
      }
   }

   // log the ordering, see dpkgpm.h and the "Ops" enum there
   fprintf(report, "AptOrdering:\n");
   for (auto && I : List)
   {
      char const * opstr = nullptr;
      switch (I.Op)
      {
	 case Item::Install: opstr = "Install"; break;
	 case Item::Configure: opstr = "Configure"; break;
	 case Item::Remove: opstr = "Remove"; break;
	 case Item::Purge: opstr = "Purge"; break;
	 case Item::ConfigurePending: opstr = "ConfigurePending"; break;
	 case Item::TriggersPending: opstr = "TriggersPending"; break;
	 case Item::RemovePending: opstr = "RemovePending"; break;
	 case Item::PurgePending: opstr = "PurgePending"; break;
      }
      auto const pkgname = I.Pkg.end() ? "NULL" : I.Pkg.FullName();
      fprintf(report, " %s: %s\n", pkgname.c_str(), opstr);
   }

   // attach dmesg log (to learn about segfaults)
   if (FileExists("/bin/dmesg"))
   {
      fprintf(report, "Dmesg:\n");
      FILE *log = popen("/bin/dmesg","r");
      if(log != NULL)
      {
	 char buf[1024];
	 while( fgets(buf, sizeof(buf), log) != NULL)
	    fprintf(report, " %s", buf);
	 pclose(log);
      }
   }

   // attach df -l log (to learn about filesystem status)
   if (FileExists("/bin/df"))
   {

      fprintf(report, "Df:\n");
      FILE *log = popen("/bin/df -l -x squashfs","r");
      if(log != NULL)
      {
	 char buf[1024];
	 while( fgets(buf, sizeof(buf), log) != NULL)
	    fprintf(report, " %s", buf);
	 pclose(log);
      }
   }

   fclose(report);

}
									/*}}}*/

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
#include <apt-pkg/depcache.h>
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/debsystem.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/install-progress.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

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
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <numeric>

#include <apti18n.h>
									/*}}}*/

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
   pkgDPkgPMPrivate() : stdin_is_dev_null(false), dpkgbuf_pos(0),
			term_out(NULL), history_out(NULL),
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
    std::make_pair("install",   N_("Installing %s")),
    std::make_pair("configure", N_("Configuring %s")),
    std::make_pair("remove",    N_("Removing %s")),
    std::make_pair("purge",    N_("Completely removing %s")),
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

   // If the filename string begins with DPkg::Chroot-Directory, return the
   // substr that is within the chroot so dpkg can access it.
   string const chrootdir = _config->FindDir("DPkg::Chroot-Directory","/");
   if (chrootdir != "/" && File.find(chrootdir) == 0)
   {
      size_t len = chrootdir.length();
      if (chrootdir.at(len - 1) == '/')
        len--;
      List.push_back(Item(Item::Install,Pkg,File.substr(len)));
   }
   else
      List.push_back(Item(Item::Install,Pkg,File));

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
bool pkgDPkgPM::SendV2Pkgs(FILE *F)
{
   return SendPkgsInfo(F, 2);
}
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
   signal(SIGPIPE, old_sigpipe);

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
      // we don't care for the difference (as dpkg doesn't really either)
      if (action == "upgrade")
	 action = "install";
   }
   // "status" has the form: "status: pkg: state"
   // with state in ["half-installed", "unpacked", "half-configured", 
   //                "installed", "config-files", "not-installed"]
   else if (prefix == "status")
   {
      pkgname = APT::String::Strip(list[1]);
      action = APT::String::Strip(list[2]);
   } else {
      if (Debug == true)
	 std::clog << "unknown prefix '" << prefix << "'" << std::endl;
      return;
   }


   /* handle the special cases first:

      errors look like this:
      'status: /var/cache/apt/archives/krecipes_0.8.1-0ubuntu1_i386.deb : error : trying to overwrite `/usr/share/doc/kde/HTML/en/krecipes/krectip.png', which is also in package krecipes-data 
      and conffile-prompt like this
      'status:/etc/compiz.conf/compiz.conf :  conffile-prompt: 'current-conffile' 'new-conffile' useredited distedited
   */
   if (prefix == "status")
   {
      if(action == "error")
      {
         d->progress->Error(pkgname, PackagesDone, PackagesTotal,
                            list[3]);
         pkgFailures++;
         WriteApportReport(pkgname.c_str(), list[3].c_str());
         return;
      }
      else if(action == "conffile-prompt")
      {
         d->progress->ConffilePrompt(pkgname, PackagesDone, PackagesTotal,
                                     list[3]);
         return;
      }
   }

   // at this point we know that we should have a valid pkgname, so build all 
   // the info from it

   // dpkg does not always send "pkgname:arch" so we add it here if needed
   if (pkgname.find(":") == std::string::npos)
   {
      // find the package in the group that is touched by dpkg
      // if there are multiple pkgs dpkg would send us a full pkgname:arch
      pkgCache::GrpIterator Grp = Cache.FindGrp(pkgname);
      if (Grp.end() == false)
      {
	 pkgCache::PkgIterator P = Grp.PackageList();
	 for (; P.end() != true; P = Grp.NextPkg(P))
	 {
	    if(Cache[P].Keep() == false || Cache[P].ReInstall() == true)
	    {
	       pkgname = P.FullName();
	       break;
	    }
	 }
      }
   }

   const char* const pkg = pkgname.c_str();
   std::string short_pkgname = StringSplit(pkgname, ":")[0];
   std::string arch = "";
   if (pkgname.find(":") != string::npos)
      arch = StringSplit(pkgname, ":")[1];
   std::string i18n_pkgname = pkgname;
   if (arch.size() != 0)
      strprintf(i18n_pkgname, "%s (%s)", short_pkgname.c_str(), arch.c_str());

   // 'processing' from dpkg looks like
   // 'processing: action: pkg'
   if(prefix == "processing")
   {
      const std::pair<const char *, const char *> * const iter =
	std::find_if(PackageProcessingOpsBegin,
		     PackageProcessingOpsEnd,
		     MatchProcessingOp(action.c_str()));
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
      return;
   }

   if (prefix == "status")
   {
      std::vector<struct DpkgState> &states = PackageOps[pkg];
      if (action == "triggers-pending")
      {
	 if (Debug == true)
	    std::clog << "(parsed from dpkg) pkg: " << short_pkgname
	       << " action: " << action << " (prefix 2 to "
	       << PackageOpsDone[pkg] << " of " << states.size() << ")" << endl;

	 states.insert(states.begin(), {"installed", N_("Installed %s")});
	 states.insert(states.begin(), {"half-configured", N_("Configuring %s")});
	 PackagesTotal += 2;
      }
      else if(PackageOpsDone[pkg] < states.size())
      {
	 char const * next_action = states[PackageOpsDone[pkg]].state;
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
	       std::clog << "(parsed from dpkg) pkg: " << short_pkgname
		  << " action: " << action << " (expected: '" << next_action << "' "
		  << PackageOpsDone[pkg] << " of " << states.size() << ")" << endl;

	    // check if the package moved to the next dpkg state
	    if(action == next_action)
	    {
	       // only read the translation if there is actually a next action
	       char const * const translation = _(states[PackageOpsDone[pkg]].str);

	       // we moved from one dpkg state to a new one, report that
	       ++PackageOpsDone[pkg];
	       ++PackagesDone;

	       std::string msg;
	       strprintf(msg, translation, i18n_pkgname.c_str());
	       d->progress->StatusChanged(pkgname, PackagesDone, PackagesTotal, msg);
	    }
	 }
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
// DPkgPM::DoDpkgStatusFd						/*{{{*/
void pkgDPkgPM::DoDpkgStatusFd(int statusfd)
{
   ssize_t const len = read(statusfd, &d->dpkgbuf[d->dpkgbuf_pos],
	 (sizeof(d->dpkgbuf)/sizeof(d->dpkgbuf[0])) - d->dpkgbuf_pos);
   if(len <= 0)
      return;
   d->dpkgbuf_pos += (len / sizeof(d->dpkgbuf[0]));

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
   string const logdir = _config->FindDir("Dir::Log");
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
   string const logfile_name = flCombine(logdir,
				   _config->Find("Dir::Log::Terminal"));
   if (!logfile_name.empty())
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
   string const history_name = flCombine(logdir,
				   _config->Find("Dir::Log::History"));
   if (!history_name.empty())
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
   static const std::array<std::array<DpkgState, 3>, 4> DpkgStatesOpMap = {{
      // Install operation
      {{
	 {"half-installed", N_("Preparing %s")},
	 {"unpacked", N_("Unpacking %s") },
	 {nullptr, nullptr}
      }},
      // Configure operation
      {{
	 {"unpacked",N_("Preparing to configure %s") },
	 {"half-configured", N_("Configuring %s") },
	 { "installed", N_("Installed %s")},
      }},
      // Remove operation
      {{
	 {"half-configured", N_("Preparing for removal of %s")},
	 {"half-installed", N_("Removing %s")},
	 {"config-files",  N_("Removed %s")},
      }},
      // Purge operation
      {{
	 {"config-files", N_("Preparing to completely remove %s")},
	 {"not-installed", N_("Completely removed %s")},
	 {nullptr, nullptr}
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
      auto AddToPackageOps = std::back_inserter(PackageOps[name]);
      if (I.Op == Item::Purge && I.Pkg->CurrentVer != 0)
      {
	 // purging a package which is installed first passes through remove states
	 auto const DpkgOps = DpkgStatesOpMap[Item::Remove];
	 std::copy(DpkgOps.begin(), DpkgOps.end(), AddToPackageOps);
	 PackagesTotal += DpkgOps.size();
      }
      auto const DpkgOps = DpkgStatesOpMap[I.Op];
      std::copy_if(DpkgOps.begin(), DpkgOps.end(), AddToPackageOps, [&](DpkgState const &state) {
	 if (state.state == nullptr)
	    return false;
	 ++PackagesTotal;
	 return true;
      });
   }
   /* one extra: We don't want the progress bar to reach 100%, especially not
      if we call dpkg --configure --pending and process a bunch of triggers
      while showing 100%. Also, spindown takes a while, so never reaching 100%
      is way more correct than reaching 100% while still doing stuff even if
      doing it this way is slightly bending the rules */
   ++PackagesTotal;
}
                                                                        /*}}}*/
bool pkgDPkgPM::Go(int StatusFd)					/*{{{*/
{
   APT::Progress::PackageManager *progress = NULL;
   if (StatusFd == -1)
      progress = APT::Progress::PackageManagerProgressFactory();
   else
      progress = new APT::Progress::PackageManagerProgressFd(StatusFd);

   return Go(progress);
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

// DPkgPM::Go - Run the sequence					/*{{{*/
// ---------------------------------------------------------------------
/* This globs the operations and calls dpkg
 *
 * If it is called with a progress object apt will report the install
 * progress to this object. It maps the dpkg states a package goes
 * through to human readable (and i10n-able)
 * names and calculates a percentage for each step.
 */
bool pkgDPkgPM::Go(APT::Progress::PackageManager *progress)
{
   pkgPackageManager::SigINTStop = false;
   d->progress = progress;

   // Generate the base argument list for dpkg
   std::vector<std::string> const sArgs = debSystem::GetDpkgBaseCommand();
   std::vector<const char *> Args(sArgs.size(), NULL);
   std::transform(sArgs.begin(), sArgs.end(), Args.begin(),
	 [](std::string const &s) { return s.c_str(); });
   unsigned long long const StartSize = std::accumulate(sArgs.begin(), sArgs.end(), 0llu,
	 [](unsigned long long const i, std::string const &s) { return i + s.length(); });
   size_t const BaseArgs = Args.size();

   fd_set rfds;
   struct timespec tv;

   // try to figure out the max environment size
   int OSArgMax = sysconf(_SC_ARG_MAX);
   if(OSArgMax < 0)
      OSArgMax = 32*1024;
   OSArgMax -= EnvironmentSize() - 2*1024;
   unsigned int const MaxArgBytes = _config->FindI("Dpkg::MaxArgBytes", OSArgMax);
   bool const NoTriggers = _config->FindB("DPkg::NoTriggers", false);

   if (RunScripts("DPkg::Pre-Invoke") == false)
      return false;

   if (RunScriptsWithPkgs("DPkg::Pre-Install-Pkgs") == false)
      return false;

   auto const noopDPkgInvocation = _config->FindB("Debug::pkgDPkgPM",false);
   // store auto-bits as they are supposed to be after dpkg is run
   if (noopDPkgInvocation == false)
      Cache.writeStateFile(NULL);

   decltype(List)::const_iterator::difference_type const notconfidx =
      _config->FindB("Dpkg::ExplicitLastConfigure", false) ? std::numeric_limits<decltype(notconfidx)>::max() :
      std::distance(List.cbegin(), std::find_if_not(List.crbegin(), List.crend(), [](Item const &i) { return i.Op == Item::Configure; }).base());

   // support subpressing of triggers processing for special
   // cases like d-i that runs the triggers handling manually
   bool const TriggersPending = _config->FindB("DPkg::TriggersPending", false);
   bool const ConfigurePending = _config->FindB("DPkg::ConfigurePending", true);
   if (ConfigurePending)
      List.push_back(Item(Item::ConfigurePending, PkgIterator()));

   // for the progress
   BuildPackagesProgressMap();

   if (notconfidx != std::numeric_limits<decltype(notconfidx)>::max())
   {
      if (ConfigurePending)
	 List.erase(std::next(List.begin(), notconfidx), std::prev(List.end()));
      else
	 List.erase(std::next(List.begin(), notconfidx), List.end());
   }

   d->stdin_is_dev_null = false;

   // create log
   OpenLog();

   bool dpkgMultiArch = debSystem::SupportsMultiArch();

   // start pty magic before the loop
   StartPtyMagic();

   // Tell the progress that its starting and fork dpkg
   d->progress->Start(d->master);

   // this loop is runs once per dpkg operation
   vector<Item>::const_iterator I = List.begin();
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
      else
	 for (; J != List.end() && J->Op == I->Op; ++J)
	    /* nothing */;

      auto const size = (J - I) + 10;

      // start with the baseset of arguments
      auto Size = StartSize;
      Args.erase(Args.begin() + BaseArgs, Args.end());
      Args.reserve(size);
      // keep track of allocated strings for multiarch package names
      std::vector<char *> Packages(size, nullptr);

      int fd[2];
      if (pipe(fd) != 0)
	 return _error->Errno("pipe","Failed to create IPC pipe to dpkg");

#define ADDARG(X) Args.push_back(X); Size += strlen(X)
#define ADDARGC(X) Args.push_back(X); Size += sizeof(X) - 1

      ADDARGC("--status-fd");
      char status_fd_buf[20];
      snprintf(status_fd_buf,sizeof(status_fd_buf),"%i", fd[1]);
      ADDARG(status_fd_buf);
      unsigned long const Op = I->Op;

      switch (I->Op)
      {
	 case Item::Remove:
	 ADDARGC("--force-depends");
	 ADDARGC("--force-remove-essential");
	 ADDARGC("--remove");
	 break;

	 case Item::Purge:
	 ADDARGC("--force-depends");
	 ADDARGC("--force-remove-essential");
	 ADDARGC("--purge");
	 break;

	 case Item::Configure:
	 ADDARGC("--configure");
	 break;

	 case Item::ConfigurePending:
	 ADDARGC("--configure");
	 ADDARGC("--pending");
	 break;

	 case Item::TriggersPending:
	 ADDARGC("--triggers-only");
	 ADDARGC("--pending");
	 break;

	 case Item::Install:
	 ADDARGC("--unpack");
	 ADDARGC("--auto-deconfigure");
	 break;
      }

      if (NoTriggers == true && I->Op != Item::TriggersPending &&
	  I->Op != Item::ConfigurePending)
      {
	 ADDARGC("--no-triggers");
      }
#undef ADDARGC

      // Write in the file or package names
      if (I->Op == Item::Install)
      {
	 for (;I != J && Size < MaxArgBytes; ++I)
	 {
	    if (I->File[0] != '/')
	       return _error->Error("Internal Error, Pathname to install is not absolute '%s'",I->File.c_str());
	    Args.push_back(I->File.c_str());
	    Size += I->File.length();
	 }
      }
      else
      {
	 string const nativeArch = _config->Find("APT::Architecture");
	 unsigned long const oldSize = I->Op == Item::Configure ? Size : 0;
	 for (;I != J && Size < MaxArgBytes; ++I)
	 {
	    if((*I).Pkg.end() == true)
	       continue;
	    if (I->Op == Item::Configure && disappearedPkgs.find(I->Pkg.FullName(true)) != disappearedPkgs.end())
	       continue;
	    // We keep this here to allow "smooth" transitions from e.g. multiarch dpkg/ubuntu to dpkg/debian
	    if (dpkgMultiArch == false && (I->Pkg.Arch() == nativeArch ||
					   strcmp(I->Pkg.Arch(), "all") == 0 ||
					   strcmp(I->Pkg.Arch(), "none") == 0))
	    {
	       char const * const name = I->Pkg.Name();
	       ADDARG(name);
	    }
	    else
	    {
	       pkgCache::VerIterator PkgVer;
	       std::string name = I->Pkg.Name();
	       if (Op == Item::Remove || Op == Item::Purge) 
               {
		  PkgVer = I->Pkg.CurrentVer();
                  if(PkgVer.end() == true)
                     PkgVer = FindNowVersion(I->Pkg);
               }
	       else
		  PkgVer = Cache[I->Pkg].InstVerIter(Cache);
	       if (strcmp(I->Pkg.Arch(), "none") == 0)
		  ; // never arch-qualify a package without an arch
	       else if (PkgVer.end() == false)
                  name.append(":").append(PkgVer.Arch());
               else
                  _error->Warning("Can not find PkgVer for '%s'", name.c_str());
	       char * const fullname = strdup(name.c_str());
	       Packages.push_back(fullname);
	       ADDARG(fullname);
	    }
	 }
	 // skip configure action if all sheduled packages disappeared
	 if (oldSize == Size)
	    continue;
      }
#undef ADDARG

      J = I;

      if (noopDPkgInvocation == true)
      {
	 for (std::vector<const char *>::const_iterator a = Args.begin();
	      a != Args.end(); ++a)
	    clog << *a << ' ';
	 clog << endl;
	 for (std::vector<char *>::const_iterator p = Packages.begin();
	       p != Packages.end(); ++p)
	    free(*p);
	 Packages.clear();
	 close(fd[0]);
	 close(fd[1]);
	 continue;
      }
      Args.push_back(NULL);

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

	 execvp(Args[0], (char**) &Args[0]);
	 cerr << "Could not exec dpkg!" << endl;
	 _exit(100);
      }

      // we read from dpkg here
      int const _dpkgin = fd[0];
      close(fd[1]);                        // close the write end of the pipe

      // apply ionice
      if (_config->FindB("DPkg::UseIoNice", false) == true)
	 ionice(Child);

      // setups fds
      sigemptyset(&d->sigmask);
      sigprocmask(SIG_BLOCK,&d->sigmask,&d->original_sigmask);

      /* free vectors (and therefore memory) as we don't need the included data anymore */
      for (std::vector<char *>::const_iterator p = Packages.begin();
	   p != Packages.end(); ++p)
	 free(*p);
      Packages.clear();

      // the result of the waitpid call
      int Status = 0;
      int res;
      bool waitpid_failure = false;
      while ((res=waitpid(Child,&Status, WNOHANG)) != Child) {
	 if(res < 0) {
	    // error handling, waitpid returned -1
	    if (errno == EINTR)
	       continue;
	    waitpid_failure = true;
	    break;
	 }

	 // wait for input or output here
	 FD_ZERO(&rfds);
	 if (d->master >= 0 && d->direct_stdin == false && d->stdin_is_dev_null == false)
	    FD_SET(STDIN_FILENO, &rfds);
	 FD_SET(_dpkgin, &rfds);
	 if(d->master >= 0)
	    FD_SET(d->master, &rfds);
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
      }
      close(_dpkgin);

      // Restore sig int/quit
      signal(SIGQUIT,old_SIGQUIT);
      signal(SIGINT,old_SIGINT);
      signal(SIGHUP,old_SIGHUP);

      if (waitpid_failure == true)
      {
	 strprintf(d->dpkg_error, "Sub-process %s couldn't be waited for.",Args[0]);
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
	    strprintf(d->dpkg_error, "Sub-process %s received a segmentation fault.",Args[0]);
	 else if (WIFEXITED(Status) != 0)
	    strprintf(d->dpkg_error, "Sub-process %s returned an error code (%u)",Args[0],WEXITSTATUS(Status));
	 else
	    strprintf(d->dpkg_error, "Sub-process %s exited unexpectedly",Args[0]);
	 _error->Error("%s", d->dpkg_error.c_str());

	 if(stopOnError)
	    break;
      }
   }
   // dpkg is done at this point
   StopPtyMagic();
   CloseLog();

   if (pkgPackageManager::SigINTStop)
       _error->Warning(_("Operation was interrupted before it could finish"));

   if (noopDPkgInvocation == false)
   {
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
      return;
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
   string logfile_name = _config->FindFile("Dir::Log::Terminal");
   if (!logfile_name.empty())
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
   string histfile_name = _config->FindFile("Dir::Log::History");
   if (!histfile_name.empty())
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
   const char *ops_str[] = {
      "Install",
      "Configure",
      "Remove",
      "Purge",
      "ConfigurePending",
      "TriggersPending",
   };
   fprintf(report, "AptOrdering:\n");
   for (vector<Item>::iterator I = List.begin(); I != List.end(); ++I)
      if ((*I).Pkg != NULL)
         fprintf(report, " %s: %s\n", (*I).Pkg.Name(), ops_str[(*I).Op]);
      else
         fprintf(report, " %s: %s\n", "NULL", ops_str[(*I).Op]);

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
      FILE *log = popen("/bin/df -l","r");
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

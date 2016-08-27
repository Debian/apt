// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsystem.cc,v 1.4 2004/01/26 17:01:53 mdz Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debsystem.h>
#include <apt-pkg/debversion.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <algorithm>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

debSystem debSys;

class APT_HIDDEN debSystemPrivate {
public:
   debSystemPrivate() : LockFD(-1), LockCount(0), StatusFile(0)
   {
   }
   // For locking support
   int LockFD;
   unsigned LockCount;
   
   debStatusIndex *StatusFile;
};

// System::debSystem - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debSystem::debSystem() : pkgSystem("Debian dpkg interface", &debVS), d(new debSystemPrivate())
{
}
									/*}}}*/
// System::~debSystem - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debSystem::~debSystem()
{
   delete d->StatusFile;
   delete d;
}
									/*}}}*/
// System::Lock - Get the lock						/*{{{*/
// ---------------------------------------------------------------------
/* This mirrors the operations dpkg does when it starts up. Note the
   checking of the updates directory. */
bool debSystem::Lock()
{
   // Disable file locking
   if (_config->FindB("Debug::NoLocking",false) == true || d->LockCount > 1)
   {
      d->LockCount++;
      return true;
   }

   // Create the lockfile
   string AdminDir = flNotFile(_config->FindFile("Dir::State::status"));
   d->LockFD = GetLock(AdminDir + "lock");
   if (d->LockFD == -1)
   {
      if (errno == EACCES || errno == EAGAIN)
	 return _error->Error(_("Unable to lock the administration directory (%s), "
	                        "is another process using it?"),AdminDir.c_str());
      else
	 return _error->Error(_("Unable to lock the administration directory (%s), "
	                        "are you root?"),AdminDir.c_str());
   }
   
   // See if we need to abort with a dirty journal
   if (CheckUpdates() == true)
   {
      close(d->LockFD);
      d->LockFD = -1;
      const char *cmd;
      if (getenv("SUDO_USER") != NULL)
	 cmd = "sudo dpkg --configure -a";
      else
	 cmd = "dpkg --configure -a";
      // TRANSLATORS: the %s contains the recovery command, usually
      //              dpkg --configure -a
      return _error->Error(_("dpkg was interrupted, you must manually "
                             "run '%s' to correct the problem. "), cmd);
   }

	 d->LockCount++;
      
   return true;
}
									/*}}}*/
// System::UnLock - Drop a lock						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSystem::UnLock(bool NoErrors)
{
   if (d->LockCount == 0 && NoErrors == true)
      return false;
   
   if (d->LockCount < 1)
      return _error->Error(_("Not locked"));
   if (--d->LockCount == 0)
   {
      close(d->LockFD);
      d->LockCount = 0;
   }
   
   return true;
}
									/*}}}*/
// System::CheckUpdates - Check if the updates dir is dirty		/*{{{*/
// ---------------------------------------------------------------------
/* This does a check of the updates directory (dpkg journal) to see if it has 
   any entries in it. */
bool debSystem::CheckUpdates()
{
   // Check for updates.. (dirty)
   string File = flNotFile(_config->FindFile("Dir::State::status")) + "updates/";
   DIR *DirP = opendir(File.c_str());
   if (DirP == 0)
      return false;
   
   /* We ignore any files that are not all digits, this skips .,.. and 
      some tmp files dpkg will leave behind.. */
   bool Damaged = false;
   for (struct dirent *Ent = readdir(DirP); Ent != 0; Ent = readdir(DirP))
   {
      Damaged = true;
      for (unsigned int I = 0; Ent->d_name[I] != 0; I++)
      {
	 // Check if its not a digit..
	 if (isdigit(Ent->d_name[I]) == 0)
	 {
	    Damaged = false;
	    break;
	 }
      }
      if (Damaged == true)
	 break;
   }
   closedir(DirP);

   return Damaged;
}
									/*}}}*/
// System::CreatePM - Create the underlying package manager		/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgPackageManager *debSystem::CreatePM(pkgDepCache *Cache) const
{
   return new pkgDPkgPM(Cache);
}
									/*}}}*/
// System::Initialize - Setup the configuration space..			/*{{{*/
// ---------------------------------------------------------------------
/* These are the Debian specific configuration variables.. */
static std::string getDpkgStatusLocation(Configuration const &Cnf) {
   Configuration PathCnf;
   PathCnf.Set("Dir", Cnf.Find("Dir", "/"));
   PathCnf.Set("Dir::State::status", "status");
   auto const cnfstatedir = Cnf.Find("Dir::State", STATE_DIR + 1);
   // if the state dir ends in apt, replace it with dpkg -
   // for the default this gives us the same as the fallback below.
   // This can't be a ../dpkg as that would play bad with symlinks
   std::string statedir;
   if (APT::String::Endswith(cnfstatedir, "/apt/"))
      statedir.assign(cnfstatedir, 0, cnfstatedir.length() - 5);
   else if (APT::String::Endswith(cnfstatedir, "/apt"))
      statedir.assign(cnfstatedir, 0, cnfstatedir.length() - 4);
   if (statedir.empty())
      PathCnf.Set("Dir::State", "var/lib/dpkg");
   else
      PathCnf.Set("Dir::State", flCombine(statedir, "dpkg"));
   return PathCnf.FindFile("Dir::State::status");
}
bool debSystem::Initialize(Configuration &Cnf)
{
   /* These really should be jammed into a generic 'Local Database' engine
      which is yet to be determined. The functions in pkgcachegen should
      be the only users of these */
   Cnf.CndSet("Dir::State::extended_states", "extended_states");
   if (Cnf.Exists("Dir::State::status") == false)
      Cnf.Set("Dir::State::status", getDpkgStatusLocation(Cnf));
   Cnf.CndSet("Dir::Bin::dpkg",BIN_DIR"/dpkg");

   if (d->StatusFile) {
     delete d->StatusFile;
     d->StatusFile = 0;
   }

   return true;
}
									/*}}}*/
// System::ArchiveSupported - Is a file format supported		/*{{{*/
// ---------------------------------------------------------------------
/* The standard name for a deb is 'deb'.. There are no separate versions
   of .deb to worry about.. */
APT_PURE bool debSystem::ArchiveSupported(const char *Type)
{
   if (strcmp(Type,"deb") == 0)
      return true;
   return false;
}
									/*}}}*/
// System::Score - Determine how 'Debiany' this sys is..		/*{{{*/
// ---------------------------------------------------------------------
/* We check some files that are sure tell signs of this being a Debian
   System.. */
signed debSystem::Score(Configuration const &Cnf)
{
   signed Score = 0;
   if (FileExists(Cnf.FindFile("Dir::State::status",getDpkgStatusLocation(Cnf).c_str())) == true)
       Score += 10;
   if (FileExists(Cnf.Find("Dir::Bin::dpkg",BIN_DIR"/dpkg")) == true)
      Score += 10;
   if (FileExists("/etc/debian_version") == true)
      Score += 10;
   return Score;
}
									/*}}}*/
// System::AddStatusFiles - Register the status files			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSystem::AddStatusFiles(std::vector<pkgIndexFile *> &List)
{
   if (d->StatusFile == 0)
      d->StatusFile = new debStatusIndex(_config->FindFile("Dir::State::status"));
   List.push_back(d->StatusFile);
   return true;
}
									/*}}}*/
// System::FindIndex - Get an index file for status files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSystem::FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const
{
   if (d->StatusFile == 0)
      return false;
   if (d->StatusFile->FindInCache(*File.Cache()) == File)
   {
      Found = d->StatusFile;
      return true;
   }
   
   return false;
}
									/*}}}*/

std::string debSystem::GetDpkgExecutable()				/*{{{*/
{
   string Tmp = _config->Find("Dir::Bin::dpkg","dpkg");
   string const dpkgChrootDir = _config->FindDir("DPkg::Chroot-Directory", "/");
   size_t dpkgChrootLen = dpkgChrootDir.length();
   if (dpkgChrootDir != "/" && Tmp.find(dpkgChrootDir) == 0)
   {
      if (dpkgChrootDir[dpkgChrootLen - 1] == '/')
         --dpkgChrootLen;
      Tmp = Tmp.substr(dpkgChrootLen);
   }
   return Tmp;
}
									/*}}}*/
std::vector<std::string> debSystem::GetDpkgBaseCommand()		/*{{{*/
{
   // Generate the base argument list for dpkg
   std::vector<std::string> Args = { GetDpkgExecutable() };
   // Stick in any custom dpkg options
   Configuration::Item const *Opts = _config->Tree("DPkg::Options");
   if (Opts != 0)
   {
      Opts = Opts->Child;
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 Args.push_back(Opts->Value);
      }
   }
   return Args;
}
									/*}}}*/
void debSystem::DpkgChrootDirectory()					/*{{{*/
{
   std::string const chrootDir = _config->FindDir("DPkg::Chroot-Directory");
   if (chrootDir == "/")
      return;
   std::cerr << "Chrooting into " << chrootDir << std::endl;
   if (chroot(chrootDir.c_str()) != 0)
      _exit(100);
   if (chdir("/") != 0)
      _exit(100);
}
									/*}}}*/
pid_t debSystem::ExecDpkg(std::vector<std::string> const &sArgs, int * const inputFd, int * const outputFd, bool const DiscardOutput)/*{{{*/
{
   std::vector<const char *> Args(sArgs.size(), NULL);
   std::transform(sArgs.begin(), sArgs.end(), Args.begin(), [](std::string const &s) { return s.c_str(); });
   Args.push_back(NULL);

   int external[2] = {-1, -1};
   if (inputFd != nullptr || outputFd != nullptr)
      if (pipe(external) != 0)
      {
	 _error->WarningE("dpkg", "Can't create IPC pipe for dpkg call");
	 return -1;
      }

   pid_t const dpkg = ExecFork();
   if (dpkg == 0) {
      int const nullfd = open("/dev/null", O_RDONLY);
      if (inputFd == nullptr)
	 dup2(nullfd, STDIN_FILENO);
      else
      {
	 close(external[1]);
	 dup2(external[0], STDIN_FILENO);
      }
      if (outputFd == nullptr)
	 dup2(nullfd, STDOUT_FILENO);
      else
      {
	 close(external[0]);
	 dup2(external[1], STDOUT_FILENO);
      }
      if (DiscardOutput == true)
	 dup2(nullfd, STDERR_FILENO);
      debSystem::DpkgChrootDirectory();
      execvp(Args[0], (char**) &Args[0]);
      _error->WarningE("dpkg", "Can't execute dpkg!");
      _exit(100);
   }
   if (outputFd != nullptr)
   {
      close(external[1]);
      *outputFd = external[0];
   }
   else if (inputFd != nullptr)
   {
      close(external[0]);
      *inputFd = external[1];
   }
   return dpkg;
}
									/*}}}*/
bool debSystem::SupportsMultiArch()					/*{{{*/
{
   std::vector<std::string> Args = GetDpkgBaseCommand();
   Args.push_back("--assert-multi-arch");
   pid_t const dpkgAssertMultiArch = ExecDpkg(Args, nullptr, nullptr, true);
   if (dpkgAssertMultiArch > 0)
   {
      int Status = 0;
      while (waitpid(dpkgAssertMultiArch, &Status, 0) != dpkgAssertMultiArch)
      {
	 if (errno == EINTR)
	    continue;
	 _error->WarningE("dpkgGo", _("Waited for %s but it wasn't there"), "dpkg --assert-multi-arch");
	 break;
      }
      if (WIFEXITED(Status) == true && WEXITSTATUS(Status) == 0)
	 return true;
   }
   return false;
}
									/*}}}*/
std::vector<std::string> debSystem::SupportedArchitectures()		/*{{{*/
{
   std::vector<std::string> archs;
   {
      string const arch = _config->Find("APT::Architecture");
      if (arch.empty() == false)
	 archs.push_back(std::move(arch));
   }

   std::vector<std::string> sArgs = GetDpkgBaseCommand();
   sArgs.push_back("--print-foreign-architectures");
   int outputFd = -1;
   pid_t const dpkgMultiArch = ExecDpkg(sArgs, nullptr, &outputFd, true);
   if (dpkgMultiArch == -1)
      return archs;

   FILE *dpkg = fdopen(outputFd, "r");
   if(dpkg != NULL) {
      char* buf = NULL;
      size_t bufsize = 0;
      while (getline(&buf, &bufsize, dpkg) != -1)
      {
	 char* tok_saveptr;
	 char* arch = strtok_r(buf, " ", &tok_saveptr);
	 while (arch != NULL) {
	    for (; isspace_ascii(*arch) != 0; ++arch);
	    if (arch[0] != '\0') {
	       char const* archend = arch;
	       for (; isspace_ascii(*archend) == 0 && *archend != '\0'; ++archend);
	       string a(arch, (archend - arch));
	       if (std::find(archs.begin(), archs.end(), a) == archs.end())
		  archs.push_back(a);
	    }
	    arch = strtok_r(NULL, " ", &tok_saveptr);
	 }
      }
      free(buf);
      fclose(dpkg);
   }
   ExecWait(dpkgMultiArch, "dpkg --print-foreign-architectures", true);
   return archs;
}
									/*}}}*/

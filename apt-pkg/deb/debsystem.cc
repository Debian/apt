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
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

debSystem debSys;

class debSystemPrivate {
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
debSystem::debSystem()
{
   d = new debSystemPrivate();
   Label = "Debian dpkg interface";
   VS = &debVS;
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
   string AdminDir = flNotFile(_config->Find("Dir::State::status"));
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
   string File = flNotFile(_config->Find("Dir::State::status")) + "updates/";
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
bool debSystem::Initialize(Configuration &Cnf)
{
   /* These really should be jammed into a generic 'Local Database' engine
      which is yet to be determined. The functions in pkgcachegen should
      be the only users of these */
   Cnf.CndSet("Dir::State::extended_states", "extended_states");
   Cnf.CndSet("Dir::State::status","/var/lib/dpkg/status");
   Cnf.CndSet("Dir::Bin::dpkg","/usr/bin/dpkg");

   if (d->StatusFile) {
     delete d->StatusFile;
     d->StatusFile = 0;
   }

   return true;
}
									/*}}}*/
// System::ArchiveSupported - Is a file format supported		/*{{{*/
// ---------------------------------------------------------------------
/* The standard name for a deb is 'deb'.. There are no seperate versions
   of .deb to worry about.. */
bool debSystem::ArchiveSupported(const char *Type)
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
   if (FileExists(Cnf.FindFile("Dir::State::status","/var/lib/dpkg/status")) == true)
       Score += 10;
   if (FileExists(Cnf.FindFile("Dir::Bin::dpkg","/usr/bin/dpkg")) == true)
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

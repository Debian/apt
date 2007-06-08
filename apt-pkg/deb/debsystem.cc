// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsystem.cc,v 1.4 2004/01/26 17:01:53 mdz Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
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
									/*}}}*/

debSystem debSys;

// System::debSystem - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debSystem::debSystem()
{
   LockFD = -1;
   LockCount = 0;
   StatusFile = 0;
   
   Label = "Debian dpkg interface";
   VS = &debVS;
}
									/*}}}*/
// System::~debSystem - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debSystem::~debSystem()
{
   delete StatusFile;
}
									/*}}}*/
// System::Lock - Get the lock						/*{{{*/
// ---------------------------------------------------------------------
/* This mirrors the operations dpkg does when it starts up. Note the
   checking of the updates directory. */
bool debSystem::Lock()
{
   // Disable file locking
   if (_config->FindB("Debug::NoLocking",false) == true || LockCount > 1)
   {
      LockCount++;
      return true;
   }

   // Create the lockfile
   string AdminDir = flNotFile(_config->Find("Dir::State::status"));
   LockFD = GetLock(AdminDir + "lock");
   if (LockFD == -1)
   {
      if (errno == EACCES || errno == EAGAIN)
	 return _error->Error("Unable to lock the administration directory (%s), "
			      "is another process using it?",AdminDir.c_str());
      else
	 return _error->Error("Unable to lock the administration directory (%s), "
			      "are you root?",AdminDir.c_str());
   }
   
   // See if we need to abort with a dirty journal
   if (CheckUpdates() == true)
   {
      close(LockFD);
      LockFD = -1;
      return _error->Error("dpkg was interrupted, you must manually "
			   "run 'dpkg --configure -a' to correct the problem. ");
   }

	 LockCount++;
      
   return true;
}
									/*}}}*/
// System::UnLock - Drop a lock						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSystem::UnLock(bool NoErrors)
{
   if (LockCount == 0 && NoErrors == true)
      return false;
   
   if (LockCount < 1)
      return _error->Error("Not locked");
   if (--LockCount == 0)
   {
      close(LockFD);
      LockCount = 0;
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
   Cnf.CndSet("Dir::State::userstatus","status.user"); // Defunct
   Cnf.CndSet("Dir::State::status","/var/lib/dpkg/status");
   Cnf.CndSet("Dir::Bin::dpkg","/usr/bin/dpkg");

   if (StatusFile) {
     delete StatusFile;
     StatusFile = 0;
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
bool debSystem::AddStatusFiles(vector<pkgIndexFile *> &List)
{
   if (StatusFile == 0)
      StatusFile = new debStatusIndex(_config->FindFile("Dir::State::status"));
   List.push_back(StatusFile);
   return true;
}
									/*}}}*/
// System::FindIndex - Get an index file for status files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSystem::FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const
{
   if (StatusFile == 0)
      return false;
   if (StatusFile->FindInCache(*File.Cache()) == File)
   {
      Found = StatusFile;
      return true;
   }
   
   return false;
}
									/*}}}*/

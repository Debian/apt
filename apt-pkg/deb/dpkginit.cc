// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkginit.cc,v 1.1 1998/11/23 07:03:10 jgg Exp $
/* ######################################################################

   DPKG init - Initialize the dpkg stuff

   ##################################################################### */
									/*}}}*/
// Includes								/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/dpkginit.h"
#endif
#include <apt-pkg/dpkginit.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
									/*}}}*/

// DpkgLock::pkgDpkgLock - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDpkgLock::pkgDpkgLock()
{
   LockFD = -1;
   GetLock();
}
									/*}}}*/
// DpkgLock::~pkgDpkgLock - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDpkgLock::~pkgDpkgLock()
{
   Close();
}
									/*}}}*/
// DpkgLock::GetLock - Get the lock					/*{{{*/
// ---------------------------------------------------------------------
/* This mirrors the operations dpkg does when it starts up. Note the
   checking of the updates directory. */
bool pkgDpkgLock::GetLock()
{
   // Disable file locking
   if (_config->FindB("Debug::NoLocking",false) == true)
      return true;
   
   Close();
   
   // Create the lockfile
   string AdminDir = flNotFile(_config->Find("Dir::State::status"));
   LockFD = ::GetLock(AdminDir + "lock");
   if (LockFD == -1)
      return _error->Errno("Open","Unable to lock the administration directory "
			   "%s, are you root?",AdminDir.c_str());
   
   // Check for updates.. (dirty)
   string File = AdminDir + "updates/";
   DIR *DirP = opendir(File.c_str());
   if (DirP != 0)
   {
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
       	 
      // Woops, we have to run dpkg to rewrite the status file
      if (Damaged == true)
      {
	 Close();
	 return _error->Error("dpkg was interrupted, you must manually "
			      "run 'dpkg --configure -a' to correct the problem. ");
      }
   }
   
   return true;
}
									/*}}}*/
// DpkgLock::Close - Close the lock					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDpkgLock::Close()
{
   close(LockFD);
   LockFD = -1;
}
									/*}}}*/

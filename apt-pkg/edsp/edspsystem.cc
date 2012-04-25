// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   This system provides the abstraction to use the scenario file as the
   only source of package information to be able to feed the created file
   back to APT for its own consumption (eat your own dogfood).

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/edspsystem.h>
#include <apt-pkg/debversion.h>
#include <apt-pkg/edspindexfile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <apti18n.h>
									/*}}}*/

edspSystem edspSys;

// System::debSystem - Constructor					/*{{{*/
edspSystem::edspSystem()
{
   StatusFile = 0;

   Label = "Debian APT solver interface";
   VS = &debVS;
}
									/*}}}*/
// System::~debSystem - Destructor					/*{{{*/
edspSystem::~edspSystem()
{
   delete StatusFile;
}
									/*}}}*/
// System::Lock - Get the lock						/*{{{*/
bool edspSystem::Lock()
{
   return true;
}
									/*}}}*/
// System::UnLock - Drop a lock						/*{{{*/
bool edspSystem::UnLock(bool NoErrors)
{
   return true;
}
									/*}}}*/
// System::CreatePM - Create the underlying package manager		/*{{{*/
// ---------------------------------------------------------------------
/* we can't use edsp input as input for real installations - just a
   simulation can work, but everything else will fail bigtime */
pkgPackageManager *edspSystem::CreatePM(pkgDepCache *Cache) const
{
   return NULL;
}
									/*}}}*/
// System::Initialize - Setup the configuration space..			/*{{{*/
bool edspSystem::Initialize(Configuration &Cnf)
{
   Cnf.Set("Dir::State::extended_states", "/dev/null");
   Cnf.Set("Dir::State::status","/dev/null");
   Cnf.Set("Dir::State::lists","/dev/null");

   Cnf.Set("Debug::NoLocking", "true");
   Cnf.Set("APT::Get::Simulate", "true");

   if (StatusFile) {
     delete StatusFile;
     StatusFile = 0;
   }
   return true;
}
									/*}}}*/
// System::ArchiveSupported - Is a file format supported		/*{{{*/
bool edspSystem::ArchiveSupported(const char *Type)
{
   return false;
}
									/*}}}*/
// System::Score - Determine if we should use the edsp system		/*{{{*/
signed edspSystem::Score(Configuration const &Cnf)
{
   if (Cnf.Find("edsp::scenario", "") == "stdin")
      return 1000;
   if (RealFileExists(Cnf.FindFile("edsp::scenario","")) == true)
      return 1000;
   return -1000;
}
									/*}}}*/
// System::AddStatusFiles - Register the status files			/*{{{*/
bool edspSystem::AddStatusFiles(std::vector<pkgIndexFile *> &List)
{
   if (StatusFile == 0)
   {
      if (_config->Find("edsp::scenario", "") == "stdin")
	 StatusFile = new edspIndex("stdin");
      else
	 StatusFile = new edspIndex(_config->FindFile("edsp::scenario"));
   }
   List.push_back(StatusFile);
   return true;
}
									/*}}}*/
// System::FindIndex - Get an index file for status files		/*{{{*/
bool edspSystem::FindIndex(pkgCache::PkgFileIterator File,
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

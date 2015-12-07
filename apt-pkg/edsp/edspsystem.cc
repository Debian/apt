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

#include <apt-pkg/configuration.h>
#include <apt-pkg/debversion.h>
#include <apt-pkg/edspindexfile.h>
#include <apt-pkg/edspsystem.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/fileutl.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

									/*}}}*/

class edspSystemPrivate {
   std::string tempDir;
   std::string tempStatesFile;
   std::string tempPrefsFile;

public:
   edspSystemPrivate() {}

   void Initialize(Configuration &Cnf)
   {
      DeInitialize();
      Cnf.Set("Dir::State::extended_states", "/dev/null");
      Cnf.Set("Dir::Etc::preferences", "/dev/null");
      std::string const tmp = GetTempDir();
      char tmpname[100];
      snprintf(tmpname, sizeof(tmpname), "%s/apt-edsp-solver-XXXXXX", tmp.c_str());
      if (NULL == mkdtemp(tmpname))
	 return;
      tempDir = tmpname;
      tempStatesFile = flCombine(tempDir, "extended_states");
      Cnf.Set("Dir::State::extended_states", tempStatesFile);
      tempPrefsFile = flCombine(tempDir, "apt_preferences");
      Cnf.Set("Dir::Etc::preferences", tempPrefsFile);
   }

   void DeInitialize()
   {
      if (tempDir.empty())
	 return;

      RemoveFile("DeInitialize", tempStatesFile);
      RemoveFile("DeInitialize", tempPrefsFile);
      rmdir(tempDir.c_str());
   }

   ~edspSystemPrivate() { DeInitialize(); }
};
// System::edspSystem - Constructor					/*{{{*/
edspSystem::edspSystem() : pkgSystem("Debian APT solver interface", &debVS), d(new edspSystemPrivate()), StatusFile(NULL)
{
}
									/*}}}*/
// System::~debSystem - Destructor					/*{{{*/
edspSystem::~edspSystem()
{
   delete StatusFile;
   delete d;
}
									/*}}}*/
// System::Lock - Get the lock						/*{{{*/
bool edspSystem::Lock()
{
   return true;
}
									/*}}}*/
// System::UnLock - Drop a lock						/*{{{*/
bool edspSystem::UnLock(bool /*NoErrors*/)
{
   return true;
}
									/*}}}*/
// System::CreatePM - Create the underlying package manager		/*{{{*/
// ---------------------------------------------------------------------
/* we can't use edsp input as input for real installations - just a
   simulation can work, but everything else will fail bigtime */
pkgPackageManager *edspSystem::CreatePM(pkgDepCache * /*Cache*/) const
{
   return NULL;
}
									/*}}}*/
// System::Initialize - Setup the configuration space..			/*{{{*/
bool edspSystem::Initialize(Configuration &Cnf)
{
   d->Initialize(Cnf);
   Cnf.Set("Dir::Etc::preferencesparts", "/dev/null");
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
bool edspSystem::ArchiveSupported(const char * /*Type*/)
{
   return false;
}
									/*}}}*/
// System::Score - Never use the EDSP system automatically		/*{{{*/
signed edspSystem::Score(Configuration const &)
{
   return -1000;
}
									/*}}}*/
bool edspSystem::AddStatusFiles(std::vector<pkgIndexFile *> &List)	/*{{{*/
{
   if (StatusFile == 0)
   {
      if (_config->Find("edsp::scenario", "") == "/nonexistent/stdin")
	 StatusFile = new edspIndex("/nonexistent/stdin");
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

APT_HIDDEN edspSystem edspSys;

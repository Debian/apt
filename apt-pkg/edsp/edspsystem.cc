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
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

									/*}}}*/

// System::System - Constructor						/*{{{*/
edspLikeSystem::edspLikeSystem(char const * const Label) : pkgSystem(Label, &debVS)
{
}
edspSystem::edspSystem() : edspLikeSystem("Debian APT solver interface")
{
}
eippSystem::eippSystem() : edspLikeSystem("Debian APT planner interface")
{
}
									/*}}}*/
// System::Lock - Get the lock						/*{{{*/
bool edspLikeSystem::Lock(OpProgress *)
{
   return true;
}
									/*}}}*/
// System::UnLock - Drop a lock						/*{{{*/
bool edspLikeSystem::UnLock(bool /*NoErrors*/)
{
   return true;
}
									/*}}}*/
// System::CreatePM - Create the underlying package manager		/*{{{*/
// ---------------------------------------------------------------------
/* we can't use edsp input as input for real installations - just a
   simulation can work, but everything else will fail bigtime */
pkgPackageManager *edspLikeSystem::CreatePM(pkgDepCache * /*Cache*/) const
{
   return nullptr;
}
									/*}}}*/
// System::Initialize - Setup the configuration space..			/*{{{*/
bool edspLikeSystem::Initialize(Configuration &Cnf)
{
   Cnf.Set("Dir::Log", "/dev/null");
   // state is included completely in the input files
   Cnf.Set("Dir::Etc::preferences", "/dev/null");
   Cnf.Set("Dir::Etc::preferencesparts", "/dev/null");
   Cnf.Set("Dir::State::status","/dev/null");
   Cnf.Set("Dir::State::extended_states","/dev/null");
   Cnf.Set("Dir::State::lists","/dev/null");
   // do not store an mmap cache
   Cnf.Set("Dir::Cache::pkgcache", "");
   Cnf.Set("Dir::Cache::srcpkgcache", "");
   // the protocols only propose actions, not do them
   Cnf.Set("Debug::NoLocking", "true");
   Cnf.Set("APT::Get::Simulate", "true");

   StatusFile.reset(nullptr);
   return true;
}
bool edspSystem::Initialize(Configuration &Cnf)
{
   if (edspLikeSystem::Initialize(Cnf) == false)
      return false;
   std::string const tmp = GetTempDir();
   char tmpname[300];
   snprintf(tmpname, sizeof(tmpname), "%s/apt-edsp-solver-XXXXXX", tmp.c_str());
   if (nullptr == mkdtemp(tmpname))
      return false;
   tempDir = tmpname;
   tempStatesFile = flCombine(tempDir, "extended_states");
   Cnf.Set("Dir::State::extended_states", tempStatesFile);
   tempPrefsFile = flCombine(tempDir, "apt_preferences");
   Cnf.Set("Dir::Etc::preferences", tempPrefsFile);
   return true;
}
									/*}}}*/
// System::ArchiveSupported - Is a file format supported		/*{{{*/
bool edspLikeSystem::ArchiveSupported(const char * /*Type*/)
{
   return false;
}
									/*}}}*/
// System::Score - Never use the EDSP system automatically		/*{{{*/
signed edspLikeSystem::Score(Configuration const &)
{
   return -1000;
}
									/*}}}*/
// System::FindIndex - Get an index file for status files		/*{{{*/
bool edspLikeSystem::FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const
{
   if (StatusFile == 0)
      return false;
   if (StatusFile->FindInCache(*File.Cache()) == File)
   {
      Found = StatusFile.get();
      return true;
   }

   return false;
}
									/*}}}*/
bool edspSystem::AddStatusFiles(std::vector<pkgIndexFile *> &List)	/*{{{*/
{
   if (StatusFile == nullptr)
   {
      if (_config->Find("edsp::scenario", "") == "/nonexistent/stdin")
	 StatusFile.reset(new edspIndex("/nonexistent/stdin"));
      else
	 StatusFile.reset(new edspIndex(_config->FindFile("edsp::scenario")));
   }
   List.push_back(StatusFile.get());
   return true;
}
									/*}}}*/
bool eippSystem::AddStatusFiles(std::vector<pkgIndexFile *> &List)	/*{{{*/
{
   if (StatusFile == nullptr)
   {
      if (_config->Find("eipp::scenario", "") == "/nonexistent/stdin")
	 StatusFile.reset(new eippIndex("/nonexistent/stdin"));
      else
	 StatusFile.reset(new eippIndex(_config->FindFile("eipp::scenario")));
   }
   List.push_back(StatusFile.get());
   return true;
}
									/*}}}*/

edspLikeSystem::~edspLikeSystem() {}
edspSystem::~edspSystem()
{
   if (tempDir.empty())
      return;

   RemoveFile("~edspSystem", tempStatesFile);
   RemoveFile("~edspSystem", tempPrefsFile);
   rmdir(tempDir.c_str());
}
eippSystem::~eippSystem() {}

APT_HIDDEN edspSystem edspSys;
APT_HIDDEN eippSystem eippSys;

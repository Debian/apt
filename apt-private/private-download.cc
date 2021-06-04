// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/clean.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-download.h>
#include <apt-private/private-output.h>
#include <apt-private/private-utils.h>

#include <fstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_VFS_H
#include <sys/vfs.h>
#else
#ifdef HAVE_PARAMS_H
#include <sys/params.h>
#endif
#include <sys/mount.h>
#endif
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <apti18n.h>
									/*}}}*/

// CheckAuth - check if each download comes form a trusted source	/*{{{*/
bool CheckAuth(pkgAcquire& Fetcher, bool const PromptUser)
{
   std::vector<std::string> UntrustedList;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I < Fetcher.ItemsEnd(); ++I)
      if (!(*I)->IsTrusted())
	 UntrustedList.push_back((*I)->ShortDesc());

   if (UntrustedList.empty())
      return true;

   return AuthPrompt(UntrustedList, PromptUser);
}
									/*}}}*/
bool AuthPrompt(std::vector<std::string> const &UntrustedList, bool const PromptUser)/*{{{*/
{
   ShowList(c2out,_("WARNING: The following packages cannot be authenticated!"), UntrustedList,
	 [](std::string const&) { return true; },
	 [](std::string const&str) { return str; },
	 [](std::string const&) { return ""; });

   if (_config->FindB("APT::Get::AllowUnauthenticated",false) == true)
   {
      c2out << _("Authentication warning overridden.\n");
      return true;
   }

   if (PromptUser == false)
      return _error->Error(_("Some packages could not be authenticated"));

   if (_config->FindI("quiet",0) < 2
       && _config->FindB("APT::Get::Assume-Yes",false) == false)
   {
      if (!YnPrompt(_("Install these packages without verification?"), false))
         return _error->Error(_("Some packages could not be authenticated"));

      return true;
   }
   else if (_config->FindB("APT::Get::Force-Yes",false) == true) {
      return true;
   }

   return _error->Error(_("There were unauthenticated packages and -y was used without --allow-unauthenticated"));
}
									/*}}}*/
bool AcquireRun(pkgAcquire &Fetcher, int const PulseInterval, bool * const Failure, bool * const TransientNetworkFailure)/*{{{*/
{
   pkgAcquire::RunResult res;
   if(PulseInterval > 0)
      res = Fetcher.Run(PulseInterval);
   else
      res = Fetcher.Run();

   if (res == pkgAcquire::Failed)
      return false;

   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin();
	I != Fetcher.ItemsEnd(); ++I)
   {

      if ((*I)->Status == pkgAcquire::Item::StatDone &&
	    (*I)->Complete == true)
	 continue;

      if (TransientNetworkFailure != NULL && (*I)->Status == pkgAcquire::Item::StatIdle)
      {
	 *TransientNetworkFailure = true;
	 continue;
      }

      ::URI uri((*I)->DescURI());
      uri.User.clear();
      uri.Password.clear();
      std::string descUri = std::string(uri);
      _error->Error(_("Failed to fetch %s  %s"), descUri.c_str(),
	    (*I)->ErrorText.c_str());

      if (Failure != NULL)
	 *Failure = true;
   }

   return true;
}
									/*}}}*/
bool CheckFreeSpaceBeforeDownload(std::string const &Dir, unsigned long long FetchBytes)/*{{{*/
{
   uint32_t const RAMFS_MAGIC = 0x858458f6;
   /* Check for enough free space, but only if we are actually going to
      download */
   if (_config->FindB("APT::Get::Print-URIs", false) == true ||
       _config->FindB("APT::Get::Download", true) == false)
      return true;

   struct statvfs Buf;
   if (statvfs(Dir.c_str(),&Buf) != 0) {
      if (errno == EOVERFLOW)
	 return _error->WarningE("statvfs",_("Couldn't determine free space in %s"),
	       Dir.c_str());
      else
	 return _error->Errno("statvfs",_("Couldn't determine free space in %s"),
	       Dir.c_str());
   }
   else
   {
      unsigned long long const FreeBlocks = _config->Find("APT::Sandbox::User").empty() ? Buf.f_bfree : Buf.f_bavail;
      if (FreeBlocks < (FetchBytes / Buf.f_bsize))
      {
	 struct statfs Stat;
	 if (statfs(Dir.c_str(),&Stat) != 0
#ifdef HAVE_STRUCT_STATFS_F_TYPE
	       || Stat.f_type != RAMFS_MAGIC
#endif
	    )
	    return _error->Error(_("You don't have enough free space in %s."),
		  Dir.c_str());
      }
   }
   return true;
}
									/*}}}*/

aptAcquireWithTextStatus::aptAcquireWithTextStatus() : pkgAcquire::pkgAcquire(),
   Stat(std::cout, ScreenWidth, _config->FindI("quiet",0))
{
   SetLog(&Stat);
}

// DoDownload - download a binary					/*{{{*/
bool DoDownload(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;

   APT::CacheSetHelper helper;
   APT::VersionSet verset = APT::VersionSet::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::CacheSetHelper::CANDIDATE, helper);

   if (verset.empty() == true)
      return false;

   pkgRecords Recs(Cache);
   pkgSourceList *SrcList = Cache.GetSourceList();

   // reuse the usual acquire methods for deb files, but don't drop them into
   // the usual directories - keep everything in the current directory
   aptAcquireWithTextStatus Fetcher;
   std::vector<std::string> storefile(verset.size());
   std::string const cwd = SafeGetCWD();
   _config->Set("Dir::Cache::Archives", cwd);
   int i = 0;
   for (APT::VersionSet::const_iterator Ver = verset.begin();
	 Ver != verset.end(); ++Ver, ++i)
   {
      pkgAcquire::Item *I = new pkgAcqArchive(&Fetcher, SrcList, &Recs, *Ver, storefile[i]);
      if (storefile[i].empty())
	 continue;
      std::string const filename = cwd + flNotDir(storefile[i]);
      storefile[i].assign(filename);
      I->DestFile.assign(filename);
   }

   // Just print out the uris and exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 std::cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile)  << ' ' <<
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << std::endl;
      return true;
   }
   auto const storecopy = storefile;

   if (_error->PendingError() == true || CheckAuth(Fetcher, false) == false)
      return false;

   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false)
      return false;

   // copy files in local sources to the current directory
   i = 0;
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); ++I)
   {
      if (dynamic_cast<pkgAcqArchive*>(*I) == nullptr)
	 continue;

      if ((*I)->Local == true &&
	  (*I)->Status == pkgAcquire::Item::StatDone &&
	  (*I)->DestFile != storecopy[i])
      {
	 std::ifstream src((*I)->DestFile.c_str(), std::ios::binary);
	 std::ofstream dst(storecopy[i].c_str(), std::ios::binary);
	 dst << src.rdbuf();
	 chmod(storecopy[i].c_str(), 0644);
      }
      ++i;
   }
   return Failed == false;
}
									/*}}}*/
// DoChangelog - Get changelog from the command line			/*{{{*/
bool DoChangelog(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;

   APT::CacheSetHelper helper;
   APT::VersionList verset = APT::VersionList::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::CacheSetHelper::CANDIDATE, helper);
   if (verset.empty() == true)
      return _error->Error(_("No packages found"));

   bool const downOnly = _config->FindB("APT::Get::Download-Only", false);
   bool const printOnly = _config->FindB("APT::Get::Print-URIs", false);
   if (printOnly)
      _config->CndSet("Acquire::Changelogs::AlwaysOnline", true);

   aptAcquireWithTextStatus Fetcher;
   for (APT::VersionList::const_iterator Ver = verset.begin();
        Ver != verset.end();
        ++Ver)
   {
      if (printOnly)
	 new pkgAcqChangelog(&Fetcher, Ver, "/dev/null");
      else if (downOnly)
	 new pkgAcqChangelog(&Fetcher, Ver, ".");
      else
	 new pkgAcqChangelog(&Fetcher, Ver);
   }

   if (printOnly == false)
   {
      bool Failed = false;
      if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true)
	 return false;
   }

   if (downOnly == false || printOnly == true)
   {
      bool Failed = false;
      for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); ++I)
      {
	 if (printOnly)
	 {
	    if ((*I)->ErrorText.empty() == false)
	    {
	       Failed = true;
	       _error->Error("%s", (*I)->ErrorText.c_str());
	    }
	    else
	       std::cout << '\'' << (*I)->DescURI() << "' " << flNotDir((*I)->DestFile)  << std::endl;
	 }
	 else
	    DisplayFileInPager((*I)->DestFile);
      }
      return Failed == false;
   }

   return true;
}
									/*}}}*/

// DoClean - Remove download archives					/*{{{*/
bool DoClean(CommandLine &)
{
   std::string const archivedir = _config->FindDir("Dir::Cache::archives");
   std::string const listsdir = _config->FindDir("Dir::state::lists");

   if (_config->FindB("APT::Get::Simulate") == true)
   {
      std::string const pkgcache = _config->FindFile("Dir::cache::pkgcache");
      std::string const srcpkgcache = _config->FindFile("Dir::cache::srcpkgcache");
      std::cout << "Del " << archivedir << "* " << archivedir << "partial/*"<< std::endl
	   << "Del " << listsdir << "partial/*" << std::endl
	   << "Del " << pkgcache << " " << srcpkgcache << std::endl;
      return true;
   }

   pkgAcquire Fetcher;
   if (archivedir.empty() == false && FileExists(archivedir) == true &&
	 Fetcher.GetLock(archivedir) == true)
   {
      Fetcher.Clean(archivedir);
      Fetcher.Clean(archivedir + "partial/");
   }

   if (listsdir.empty() == false && FileExists(listsdir) == true &&
	 Fetcher.GetLock(listsdir) == true)
   {
      Fetcher.Clean(listsdir + "partial/");
   }

   pkgCacheFile::RemoveCaches();

   return true;
}
									/*}}}*/
// DoAutoClean - Smartly remove downloaded archives			/*{{{*/
// ---------------------------------------------------------------------
/* This is similar to clean but it only purges things that cannot be 
   downloaded, that is old versions of cached packages. */
 class LogCleaner : public pkgArchiveCleaner
{
   protected:
      virtual void Erase(int const dirfd, char const * const File, std::string const &Pkg, std::string const &Ver,struct stat const &St) APT_OVERRIDE
      {
	 c1out << "Del " << Pkg << " " << Ver << " [" << SizeToStr(St.st_size) << "B]" << std::endl;

	 if (_config->FindB("APT::Get::Simulate") == false)
	    RemoveFileAt("Cleaner::Erase", dirfd, File);
      };
};
bool DoAutoClean(CommandLine &)
{
   std::string const archivedir = _config->FindDir("Dir::Cache::Archives");
   if (FileExists(archivedir) == false)
      return true;

   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      int lock_fd = GetLock(flCombine(archivedir, "lock"));
      if (lock_fd < 0)
	 return _error->Error(_("Unable to lock the download directory"));
      Lock.Fd(lock_fd);
   }

   CacheFile Cache;
   if (Cache.Open(false) == false)
      return false;

   LogCleaner Cleaner;

   return Cleaner.Go(archivedir, *Cache) &&
      Cleaner.Go(flCombine(archivedir, "partial/"), *Cache);
}
									/*}}}*/

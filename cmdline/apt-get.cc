// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-get.cc,v 1.156 2004/08/28 01:05:16 mdz Exp $
/* ######################################################################
   
   apt-get - Cover for dpkg
   
   This is an allout cover for dpkg implementing a safer front end. It is
   based largely on libapt-pkg.

   The syntax is different, 
      apt-get [opt] command [things]
   Where command is:
      update - Resyncronize the package files from their sources
      upgrade - Smart-Download the newest versions of all packages
      dselect-upgrade - Follows dselect's changes to the Status: field
                       and installes new and removes old packages
      dist-upgrade - Powerful upgrader designed to handle the issues with
                    a new distribution.
      install - Download and install a given package (by name, not by .deb)
      check - Update the package cache and check for broken packages
      clean - Erase the .debs downloaded to /var/cache/apt/archives and
              the partial dir too

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/clean.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/init.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/upgrade.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-download.h>
#include <apt-private/private-install.h>
#include <apt-private/private-main.h>
#include <apt-private/private-moo.h>
#include <apt-private/private-output.h>
#include <apt-private/private-update.h>
#include <apt-private/private-upgrade.h>
#include <apt-private/private-utils.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// TryToInstallBuildDep - Try to install a single package		/*{{{*/
// ---------------------------------------------------------------------
/* This used to be inlined in DoInstall, but with the advent of regex package
   name matching it was split out.. */
static bool TryToInstallBuildDep(pkgCache::PkgIterator Pkg,pkgCacheFile &Cache,
		  pkgProblemResolver &Fix,bool Remove,bool BrokenFix,
		  bool AllowFail = true)
{
   if (Cache[Pkg].CandidateVer == 0 && Pkg->ProvidesList != 0)
   {
      CacheSetHelperAPTGet helper(c1out);
      helper.showErrors(false);
      pkgCache::VerIterator Ver = helper.canNotFindNewestVer(Cache, Pkg);
      if (Ver.end() == false)
	 Pkg = Ver.ParentPkg();
      else if (helper.showVirtualPackageErrors(Cache) == false)
	 return AllowFail;
   }

   if (_config->FindB("Debug::BuildDeps",false) == true)
   {
      if (Remove == true)
	 cout << "  Trying to remove " << Pkg << endl;
      else
	 cout << "  Trying to install " << Pkg << endl;
   }

   if (Remove == true)
   {
      TryToRemove RemoveAction(Cache, &Fix);
      RemoveAction(Pkg.VersionList());
   } else if (Cache[Pkg].CandidateVer != 0) {
      TryToInstall InstallAction(Cache, &Fix, BrokenFix);
      InstallAction(Cache[Pkg].CandidateVerIter(Cache));
      InstallAction.doAutoInstall();
   } else
      return AllowFail;

   return true;
}
									/*}}}*/


// helper that can go wit hthe next ABI break
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR < 13)
static std::string MetaIndexFileNameOnDisk(metaIndex *metaindex)
{
   // FIXME: this cast is the horror, the horror
   debReleaseIndex *r = (debReleaseIndex*)metaindex;

   // see if we have a InRelease file
   std::string PathInRelease =  r->MetaIndexFile("InRelease");
   if (FileExists(PathInRelease))
      return PathInRelease;

   // and if not return the normal one
   if (FileExists(PathInRelease))
      return r->MetaIndexFile("Release");

   return "";
}
#endif

// GetReleaseForSourceRecord - Return Suite for the given srcrecord	/*{{{*/
// ---------------------------------------------------------------------
/* */
static std::string GetReleaseForSourceRecord(pkgSourceList *SrcList,
                                      pkgSrcRecords::Parser *Parse)
{
   // try to find release
   const pkgIndexFile& CurrentIndexFile = Parse->Index();

   for (pkgSourceList::const_iterator S = SrcList->begin(); 
        S != SrcList->end(); ++S)
   {
      vector<pkgIndexFile *> *Indexes = (*S)->GetIndexFiles();
      for (vector<pkgIndexFile *>::const_iterator IF = Indexes->begin();
           IF != Indexes->end(); ++IF)
      {
         if (&CurrentIndexFile == (*IF))
         {
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR < 13)
            std::string path = MetaIndexFileNameOnDisk(*S);
#else
            std::string path = (*S)->LocalFileName();
#endif
            if (path != "") 
            {
               indexRecords records;
               records.Load(path);
               return records.GetSuite();
            }
         }
      }
   }
   return "";
}
									/*}}}*/
// FindSrc - Find a source record					/*{{{*/
// ---------------------------------------------------------------------
/* */
static pkgSrcRecords::Parser *FindSrc(const char *Name,pkgRecords &Recs,
			       pkgSrcRecords &SrcRecs,string &Src,
			       CacheFile &CacheFile)
{
   string VerTag, UserRequestedVerTag;
   string ArchTag = "";
   string RelTag = _config->Find("APT::Default-Release");
   string TmpSrc = Name;
   pkgDepCache *Cache = CacheFile.GetDepCache();

   // extract release
   size_t found = TmpSrc.find_last_of("/");
   if (found != string::npos) 
   {
      RelTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }
   // extract the version
   found = TmpSrc.find_last_of("=");
   if (found != string::npos) 
   {
      VerTag = UserRequestedVerTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }
   // extract arch 
   found = TmpSrc.find_last_of(":");
   if (found != string::npos) 
   {
      ArchTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }

   /* Lookup the version of the package we would install if we were to
      install a version and determine the source package name, then look
      in the archive for a source package of the same name. */
   bool MatchSrcOnly = _config->FindB("APT::Get::Only-Source");
   pkgCache::PkgIterator Pkg;
   if (ArchTag != "")
      Pkg = Cache->FindPkg(TmpSrc, ArchTag);
   else
      Pkg = Cache->FindPkg(TmpSrc);

   // if we can't find a package but the user qualified with a arch,
   // error out here
   if (Pkg.end() && ArchTag != "")
   {
      Src = Name;
      _error->Error(_("Can not find a package for architecture '%s'"),
                    ArchTag.c_str());
      return 0;
   }

   if (MatchSrcOnly == false && Pkg.end() == false) 
   {
      if(VerTag != "" || RelTag != "" || ArchTag != "")
      {
	 bool fuzzy = false;
	 // we have a default release, try to locate the pkg. we do it like
	 // this because GetCandidateVer() will not "downgrade", that means
	 // "apt-get source -t stable apt" won't work on a unstable system
	 for (pkgCache::VerIterator Ver = Pkg.VersionList();; ++Ver)
	 {
	    // try first only exact matches, later fuzzy matches
	    if (Ver.end() == true)
	    {
	       if (fuzzy == true)
		  break;
	       fuzzy = true;
	       Ver = Pkg.VersionList();
	       // exit right away from the Pkg.VersionList() loop if we
	       // don't have any versions
	       if (Ver.end() == true)
		  break;
	    }

            // ignore arches that are not for us
            if (ArchTag != "" && Ver.Arch() != ArchTag)
               continue;

            // pick highest version for the arch unless the user wants
            // something else
            if (ArchTag != "" && VerTag == "" && RelTag == "")
               if(Cache->VS().CmpVersion(VerTag, Ver.VerStr()) < 0)
                  VerTag = Ver.VerStr();

	    // We match against a concrete version (or a part of this version)
	    if (VerTag.empty() == false &&
		(fuzzy == true || Cache->VS().CmpVersion(VerTag, Ver.VerStr()) != 0) && // exact match
		(fuzzy == false || strncmp(VerTag.c_str(), Ver.VerStr(), VerTag.size()) != 0)) // fuzzy match
	       continue;

	    for (pkgCache::VerFileIterator VF = Ver.FileList();
		 VF.end() == false; ++VF)
	    {
	       /* If this is the status file, and the current version is not the
		  version in the status file (ie it is not installed, or somesuch)
		  then it is not a candidate for installation, ever. This weeds
		  out bogus entries that may be due to config-file states, or
		  other. */
	       if ((VF.File()->Flags & pkgCache::Flag::NotSource) ==
		   pkgCache::Flag::NotSource && Pkg.CurrentVer() != Ver)
		  continue;

	       // or we match against a release
	       if(VerTag.empty() == false ||
		  (VF.File().Archive() != 0 && VF.File().Archive() == RelTag) ||
		  (VF.File().Codename() != 0 && VF.File().Codename() == RelTag)) 
	       {
		  pkgRecords::Parser &Parse = Recs.Lookup(VF);
		  Src = Parse.SourcePkg();
		  // no SourcePkg name, so it is the "binary" name
		  if (Src.empty() == true)
		     Src = TmpSrc;
		  // the Version we have is possibly fuzzy or includes binUploads,
		  // so we use the Version of the SourcePkg (empty if same as package)
		  VerTag = Parse.SourceVer();
		  if (VerTag.empty() == true)
		     VerTag = Ver.VerStr();
		  break;
	       }
	    }
	    if (Src.empty() == false)
	       break;
	 }
      }

      if (Src == "" && ArchTag != "")
      {
         if (VerTag != "")
            _error->Error(_("Can not find a package '%s' with version '%s'"),
                          Pkg.FullName().c_str(), VerTag.c_str());
         if (RelTag != "")
            _error->Error(_("Can not find a package '%s' with release '%s'"),
                          Pkg.FullName().c_str(), RelTag.c_str());
         Src = Name;
         return 0;
      }


      if (Src.empty() == true)
      {
	 // if we don't have found a fitting package yet so we will
	 // choose a good candidate and proceed with that.
	 // Maybe we will find a source later on with the right VerTag
         // or RelTag
	 pkgCache::VerIterator Ver = Cache->GetCandidateVer(Pkg);
	 if (Ver.end() == false) 
	 {
	    pkgRecords::Parser &Parse = Recs.Lookup(Ver.FileList());
	    Src = Parse.SourcePkg();
	    if (VerTag.empty() == true)
	       VerTag = Parse.SourceVer();
	 }
      }
   }

   if (Src.empty() == true)
   {
      Src = TmpSrc;
   }
   else 
   {
      /* if we have a source pkg name, make sure to only search
	 for srcpkg names, otherwise apt gets confused if there
	 is a binary package "pkg1" and a source package "pkg1"
	 with the same name but that comes from different packages */
      MatchSrcOnly = true;
      if (Src != TmpSrc) 
      {
	 ioprintf(c1out, _("Picking '%s' as source package instead of '%s'\n"), Src.c_str(), TmpSrc.c_str());
      }
   }

   // The best hit
   pkgSrcRecords::Parser *Last = 0;
   unsigned long Offset = 0;
   string Version;
   pkgSourceList *SrcList = CacheFile.GetSourceList();

   /* Iterate over all of the hits, which includes the resulting
      binary packages in the search */
   pkgSrcRecords::Parser *Parse;
   while (true) 
   {
      SrcRecs.Restart();
      while ((Parse = SrcRecs.Find(Src.c_str(), MatchSrcOnly)) != 0) 
      {
	 const string Ver = Parse->Version();

         // See if we need to look for a specific release tag
         if (RelTag != "" && UserRequestedVerTag == "")
         {
            const string Rel = GetReleaseForSourceRecord(SrcList, Parse);

            if (Rel == RelTag)
            {
               Last = Parse;
               Offset = Parse->Offset();
               Version = Ver;
            }
         }

	 // Ignore all versions which doesn't fit
	 if (VerTag.empty() == false &&
	     Cache->VS().CmpVersion(VerTag, Ver) != 0) // exact match
	    continue;

	 // Newer version or an exact match? Save the hit
	 if (Last == 0 || Cache->VS().CmpVersion(Version,Ver) < 0) {
	    Last = Parse;
	    Offset = Parse->Offset();
	    Version = Ver;
	 }

	 // was the version check above an exact match?
         // If so, we don't need to look further
         if (VerTag.empty() == false && (VerTag == Ver))
	    break;
      }
      if (UserRequestedVerTag == "" && Version != "" && RelTag != "")
         ioprintf(c1out, "Selected version '%s' (%s) for %s\n", 
                  Version.c_str(), RelTag.c_str(), Src.c_str());

      if (Last != 0 || VerTag.empty() == true)
	 break;
      _error->Error(_("Can not find version '%s' of package '%s'"), VerTag.c_str(), TmpSrc.c_str());
      return 0;
   }

   if (Last == 0 || Last->Jump(Offset) == false)
      return 0;

   return Last;
}
									/*}}}*/
/* mark packages as automatically/manually installed.			{{{*/
static bool DoMarkAuto(CommandLine &CmdL)
{
   bool Action = true;
   int AutoMarkChanged = 0;
   OpTextProgress progress;
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;

   if (strcasecmp(CmdL.FileList[0],"markauto") == 0)
      Action = true;
   else if (strcasecmp(CmdL.FileList[0],"unmarkauto") == 0)
      Action = false;

   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      const char *S = *I;
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache->FindPkg(S);
      if (Pkg.end() == true) {
         return _error->Error(_("Couldn't find package %s"),S);
      }
      else
      {
         if (!Action)
            ioprintf(c1out,_("%s set to manually installed.\n"), Pkg.Name());
         else
            ioprintf(c1out,_("%s set to automatically installed.\n"),
                      Pkg.Name());

         Cache->MarkAuto(Pkg,Action);
         AutoMarkChanged++;
      }
   }

   _error->Notice(_("This command is deprecated. Please use 'apt-mark auto' and 'apt-mark manual' instead."));

   if (AutoMarkChanged && ! _config->FindB("APT::Get::Simulate",false))
      return Cache->writeStateFile(NULL);
   return false;
}
									/*}}}*/
// DoDSelectUpgrade - Do an upgrade by following dselects selections	/*{{{*/
// ---------------------------------------------------------------------
/* Follows dselect's selections */
static bool DoDSelectUpgrade(CommandLine &)
{
   CacheFile Cache;
   if (Cache.OpenForInstall() == false || Cache.CheckDeps() == false)
      return false;
   
   pkgDepCache::ActionGroup group(Cache);

   // Install everything with the install flag set
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; ++I)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,false);
   }

   /* Now install their deps too, if we do this above then order of
      the status file is significant for | groups */
   for (I = Cache->PkgBegin();I.end() != true; ++I)
   {
      /* Install the package only if it is a new install, the autoupgrader
         will deal with the rest */
      if (I->SelectedState == pkgCache::State::Install)
	 Cache->MarkInstall(I,true);
   }
   
   // Apply erasures now, they override everything else.
   for (I = Cache->PkgBegin();I.end() != true; ++I)
   {
      // Remove packages 
      if (I->SelectedState == pkgCache::State::DeInstall ||
	  I->SelectedState == pkgCache::State::Purge)
	 Cache->MarkDelete(I,I->SelectedState == pkgCache::State::Purge);
   }

   /* Resolve any problems that dselect created, allupgrade cannot handle
      such things. We do so quite aggressively too.. */
   if (Cache->BrokenCount() != 0)
   {      
      pkgProblemResolver Fix(Cache);

      // Hold back held packages.
      if (_config->FindB("APT::Ignore-Hold",false) == false)
      {
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); I.end() == false; ++I)
	 {
	    if (I->SelectedState == pkgCache::State::Hold)
	    {
	       Fix.Protect(I);
	       Cache->MarkKeep(I);
	    }
	 }
      }
   
      if (Fix.Resolve() == false)
      {
	 ShowBroken(c1out,Cache,false);
	 return _error->Error(_("Internal error, problem resolver broke stuff"));
      }
   }

   // Now upgrade everything
   if (pkgAllUpgrade(Cache) == false)
   {
      ShowBroken(c1out,Cache,false);
      return _error->Error(_("Internal error, problem resolver broke stuff"));
   }
   
   return InstallPackages(Cache,false);
}
									/*}}}*/
// DoClean - Remove download archives					/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool DoClean(CommandLine &)
{
   std::string const archivedir = _config->FindDir("Dir::Cache::archives");
   std::string const pkgcache = _config->FindFile("Dir::cache::pkgcache");
   std::string const srcpkgcache = _config->FindFile("Dir::cache::srcpkgcache");

   if (_config->FindB("APT::Get::Simulate") == true)
   {
      cout << "Del " << archivedir << "* " << archivedir << "partial/*"<< endl
	   << "Del " << pkgcache << " " << srcpkgcache << endl;
      return true;
   }
   
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      int lock_fd = GetLock(archivedir + "lock");
      if (lock_fd < 0)
	 return _error->Error(_("Unable to lock the download directory"));
      Lock.Fd(lock_fd);
   }
   
   pkgAcquire Fetcher;
   Fetcher.Clean(archivedir);
   Fetcher.Clean(archivedir + "partial/");

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
   virtual void Erase(const char *File,string Pkg,string Ver,struct stat &St) 
   {
      c1out << "Del " << Pkg << " " << Ver << " [" << SizeToStr(St.st_size) << "B]" << endl;
      
      if (_config->FindB("APT::Get::Simulate") == false)
	 unlink(File);      
   };
};

static bool DoAutoClean(CommandLine &)
{
   // Lock the archive directory
   FileFd Lock;
   if (_config->FindB("Debug::NoLocking",false) == false)
   {
      int lock_fd = GetLock(_config->FindDir("Dir::Cache::Archives") + "lock");
      if (lock_fd < 0)
	 return _error->Error(_("Unable to lock the download directory"));
      Lock.Fd(lock_fd);
   }
   
   CacheFile Cache;
   if (Cache.Open() == false)
      return false;
   
   LogCleaner Cleaner;
   
   return Cleaner.Go(_config->FindDir("Dir::Cache::archives"),*Cache) &&
      Cleaner.Go(_config->FindDir("Dir::Cache::archives") + "partial/",*Cache);
}
									/*}}}*/
// DoDownload - download a binary					/*{{{*/
// ---------------------------------------------------------------------
static bool DoDownload(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;

   APT::CacheSetHelper helper(c0out);
   APT::VersionSet verset = APT::VersionSet::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::VersionSet::CANDIDATE, helper);

   if (verset.empty() == true)
      return false;

   AcqTextStatus Stat(ScreenWidth, _config->FindI("quiet", 0));
   pkgAcquire Fetcher;
   if (Fetcher.Setup(&Stat) == false)
      return false;

   pkgRecords Recs(Cache);
   pkgSourceList *SrcList = Cache.GetSourceList();

   // reuse the usual acquire methods for deb files, but don't drop them into
   // the usual directories - keep everything in the current directory
   std::vector<std::string> storefile(verset.size());
   std::string const cwd = SafeGetCWD();
   _config->Set("Dir::Cache::Archives", cwd);
   int i = 0;
   for (APT::VersionSet::const_iterator Ver = verset.begin();
	 Ver != verset.end(); ++Ver, ++i)
   {
      pkgAcquire::Item *I = new pkgAcqArchive(&Fetcher, SrcList, &Recs, *Ver, storefile[i]);
      std::string const filename = cwd + flNotDir(storefile[i]);
      storefile[i].assign(filename);
      I->DestFile.assign(filename);
   }

   // Just print out the uris and exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile)  << ' ' <<
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      return true;
   }

   if (_error->PendingError() == true || CheckAuth(Fetcher, false) == false)
      return false;

   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false)
      return false;

   // copy files in local sources to the current directory
   for (pkgAcquire::ItemIterator I = Fetcher.ItemsBegin(); I != Fetcher.ItemsEnd(); ++I)
   {
      std::string const filename = cwd + flNotDir((*I)->DestFile);
      if ((*I)->Local == true &&
          filename != (*I)->DestFile &&
          (*I)->Status == pkgAcquire::Item::StatDone)
      {
	 std::ifstream src((*I)->DestFile.c_str(), std::ios::binary);
	 std::ofstream dst(filename.c_str(), std::ios::binary);
	 dst << src.rdbuf();
      }
   }
   return Failed == false;
}
									/*}}}*/
// DoCheck - Perform the check operation				/*{{{*/
// ---------------------------------------------------------------------
/* Opening automatically checks the system, this command is mostly used
   for debugging */
static bool DoCheck(CommandLine &)
{
   CacheFile Cache;
   Cache.Open();
   Cache.CheckDeps();
   
   return true;
}
									/*}}}*/
// DoSource - Fetch a source archive					/*{{{*/
// ---------------------------------------------------------------------
/* Fetch souce packages */
struct DscFile
{
   string Package;
   string Version;
   string Dsc;
};

static bool DoSource(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.Open(false) == false)
      return false;

   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to fetch source for"));
   
   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   
   // Create the text record parsers
   pkgRecords Recs(Cache);
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher;
   Fetcher.SetLog(&Stat);

   SPtrArray<DscFile> Dsc = new DscFile[CmdL.FileSize()];
   
   // insert all downloaded uris into this set to avoid downloading them
   // twice
   set<string> queued;

   // Diff only mode only fetches .diff files
   bool const diffOnly = _config->FindB("APT::Get::Diff-Only", false);
   // Tar only mode only fetches .tar files
   bool const tarOnly = _config->FindB("APT::Get::Tar-Only", false);
   // Dsc only mode only fetches .dsc files
   bool const dscOnly = _config->FindB("APT::Get::Dsc-Only", false);

   // Load the requestd sources into the fetcher
   unsigned J = 0;
   std::string UntrustedList;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      string Src;
      pkgSrcRecords::Parser *Last = FindSrc(*I,Recs,SrcRecs,Src,Cache);
      
      if (Last == 0) {
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());
      }

      if (Last->Index().IsTrusted() == false)
         UntrustedList += Src + " ";
      
      string srec = Last->AsStr();
      string::size_type pos = srec.find("\nVcs-");
      while (pos != string::npos)
      {
	 pos += strlen("\nVcs-");
	 string vcs = srec.substr(pos,srec.find(":",pos)-pos);
	 if(vcs == "Browser") 
	 {
	    pos = srec.find("\nVcs-", pos);
	    continue;
	 }
	 pos += vcs.length()+2;
	 string::size_type epos = srec.find("\n", pos);
	 string uri = srec.substr(pos,epos-pos).c_str();
	 ioprintf(c1out, _("NOTICE: '%s' packaging is maintained in "
			   "the '%s' version control system at:\n"
			   "%s\n"),
		  Src.c_str(), vcs.c_str(), uri.c_str());
	 if(vcs == "Bzr") 
	    ioprintf(c1out,_("Please use:\n"
			     "bzr branch %s\n"
			     "to retrieve the latest (possibly unreleased) "
			     "updates to the package.\n"),
		     uri.c_str());
	 break;
      }

      // Back track
      vector<pkgSrcRecords::File2> Lst;
      if (Last->Files2(Lst) == false) {
	 return false;
      }

      // Load them into the fetcher
      for (vector<pkgSrcRecords::File2>::const_iterator I = Lst.begin();
	   I != Lst.end(); ++I)
      {
	 // Try to guess what sort of file it is we are getting.
	 if (I->Type == "dsc")
	 {
	    Dsc[J].Package = Last->Package();
	    Dsc[J].Version = Last->Version();
	    Dsc[J].Dsc = flNotDir(I->Path);
	 }

	 // Handle the only options so that multiple can be used at once
	 if (diffOnly == true || tarOnly == true || dscOnly == true)
	 {
	    if ((diffOnly == true && I->Type == "diff") ||
	        (tarOnly == true && I->Type == "tar") ||
	        (dscOnly == true && I->Type == "dsc"))
		; // Fine, we want this file downloaded
	    else
	       continue;
	 }

	 // don't download the same uri twice (should this be moved to
	 // the fetcher interface itself?)
	 if(queued.find(Last->Index().ArchiveURI(I->Path)) != queued.end())
	    continue;
	 queued.insert(Last->Index().ArchiveURI(I->Path));

	 // check if we have a file with that md5 sum already localy
	 std::string localFile = flNotDir(I->Path);
	 if (FileExists(localFile) == true)
	    if(I->Hashes.VerifyFile(localFile) == true)
	    {
	       ioprintf(c1out,_("Skipping already downloaded file '%s'\n"),
			localFile.c_str());
	       continue;
	    }

	 // see if we have a hash (Acquire::ForceHash is the only way to have none)
	 HashString const * const hs = I->Hashes.find(NULL);
	 if (hs == NULL && _config->FindB("APT::Get::AllowUnauthenticated",false) == false)
	 {
	    ioprintf(c1out, "Skipping download of file '%s' as requested hashsum is not available for authentication\n",
		     localFile.c_str());
	    continue;
	 }

	 new pkgAcqFile(&Fetcher,Last->Index().ArchiveURI(I->Path),
			hs != NULL ? hs->toStr() : "", I->FileSize,
			Last->Index().SourceInfo(*Last,*I),Src);
      }
   }

   // check authentication status of the source as well
   if (UntrustedList != "" && !AuthPrompt(UntrustedList, false))
      return false;
   
   // Display statistics
   unsigned long long FetchBytes = Fetcher.FetchNeeded();
   unsigned long long FetchPBytes = Fetcher.PartialPresent();
   unsigned long long DebBytes = Fetcher.TotalNeeded();

   // Check for enough free space
   struct statvfs Buf;
   string OutputDir = ".";
   if (statvfs(OutputDir.c_str(),&Buf) != 0) {
      if (errno == EOVERFLOW)
	 return _error->WarningE("statvfs",_("Couldn't determine free space in %s"),
				OutputDir.c_str());
      else
	 return _error->Errno("statvfs",_("Couldn't determine free space in %s"),
				OutputDir.c_str());
   } else if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
     {
       struct statfs Stat;
       if (statfs(OutputDir.c_str(),&Stat) != 0
#if HAVE_STRUCT_STATFS_F_TYPE
           || unsigned(Stat.f_type) != RAMFS_MAGIC
#endif
           )  {
          return _error->Error(_("You don't have enough free space in %s"),
              OutputDir.c_str());
       }
     }
   
   // Number of bytes
   if (DebBytes != FetchBytes)
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement strings, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB/%sB of source archives.\n"),
	       SizeToStr(FetchBytes).c_str(),SizeToStr(DebBytes).c_str());
   else
      //TRANSLATOR: The required space between number and unit is already included
      // in the replacement string, so %sB will be correctly translate in e.g. 1,5 MB
      ioprintf(c1out,_("Need to get %sB of source archives.\n"),
	       SizeToStr(DebBytes).c_str());
   
   if (_config->FindB("APT::Get::Simulate",false) == true)
   {
      for (unsigned I = 0; I != J; I++)
	 ioprintf(cout,_("Fetch source %s\n"),Dsc[I].Package.c_str());
      return true;
   }
   
   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	       I->Owner->FileSize << ' ' << I->Owner->HashSum() << endl;
      return true;
   }

   // Run it
   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true)
   {
      return _error->Error(_("Failed to fetch some archives."));
   }

   if (_config->FindB("APT::Get::Download-only",false) == true)
   {
      c1out << _("Download complete and in download only mode") << endl;
      return true;
   }

   // Unpack the sources
   pid_t Process = ExecFork();
   
   if (Process == 0)
   {
      bool const fixBroken = _config->FindB("APT::Get::Fix-Broken", false);
      for (unsigned I = 0; I != J; ++I)
      {
	 string Dir = Dsc[I].Package + '-' + Cache->VS().UpstreamVersion(Dsc[I].Version.c_str());
	 
	 // Diff only mode only fetches .diff files
	 if (_config->FindB("APT::Get::Diff-Only",false) == true ||
	     _config->FindB("APT::Get::Tar-Only",false) == true ||
	     Dsc[I].Dsc.empty() == true)
	    continue;

	 // See if the package is already unpacked
	 struct stat Stat;
	 if (fixBroken == false && stat(Dir.c_str(),&Stat) == 0 &&
	     S_ISDIR(Stat.st_mode) != 0)
	 {
	    ioprintf(c0out ,_("Skipping unpack of already unpacked source in %s\n"),
			      Dir.c_str());
	 }
	 else
	 {
	    // Call dpkg-source
	    std::string const sourceopts = _config->Find("DPkg::Source-Options", "-x");
	    std::string S;
	    strprintf(S, "%s %s %s",
		     _config->Find("Dir::Bin::dpkg-source","dpkg-source").c_str(),
		     sourceopts.c_str(), Dsc[I].Dsc.c_str());
	    if (system(S.c_str()) != 0)
	    {
	       fprintf(stderr, _("Unpack command '%s' failed.\n"), S.c_str());
	       fprintf(stderr, _("Check if the 'dpkg-dev' package is installed.\n"));
	       _exit(1);
	    }
	 }

	 // Try to compile it with dpkg-buildpackage
	 if (_config->FindB("APT::Get::Compile",false) == true)
	 {
	    string buildopts = _config->Find("APT::Get::Host-Architecture");
	    if (buildopts.empty() == false)
	       buildopts = "-a" + buildopts + " ";

	    // get all active build profiles
	    std::string const profiles = APT::Configuration::getBuildProfilesString();
	    if (profiles.empty() == false)
	       buildopts.append(" -P").append(profiles).append(" ");

	    buildopts.append(_config->Find("DPkg::Build-Options","-b -uc"));

	    // Call dpkg-buildpackage
	    std::string S;
	    strprintf(S, "cd %s && %s %s",
		     Dir.c_str(),
		     _config->Find("Dir::Bin::dpkg-buildpackage","dpkg-buildpackage").c_str(),
		     buildopts.c_str());

	    if (system(S.c_str()) != 0)
	    {
	       fprintf(stderr, _("Build command '%s' failed.\n"), S.c_str());
	       _exit(1);
	    }
	 }
      }

      _exit(0);
   }

   // Wait for the subprocess
   int Status = 0;
   while (waitpid(Process,&Status,0) != Process)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }

   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      return _error->Error(_("Child process failed"));
   
   return true;
}
									/*}}}*/
// DoBuildDep - Install/removes packages to satisfy build dependencies  /*{{{*/
// ---------------------------------------------------------------------
/* This function will look at the build depends list of the given source 
   package and install the necessary packages to make it true, or fail. */
static bool DoBuildDep(CommandLine &CmdL)
{
   CacheFile Cache;

   _config->Set("APT::Install-Recommends", false);
   
   if (Cache.Open(true) == false)
      return false;

   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to check builddeps for"));
   
   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   
   // Create the text record parsers
   pkgRecords Recs(Cache);
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   // Create the download object
   AcqTextStatus Stat(ScreenWidth,_config->FindI("quiet",0));   
   pkgAcquire Fetcher;
   if (Fetcher.Setup(&Stat) == false)
      return false;

   bool StripMultiArch;
   string hostArch = _config->Find("APT::Get::Host-Architecture");
   if (hostArch.empty() == false)
   {
      std::vector<std::string> archs = APT::Configuration::getArchitectures();
      if (std::find(archs.begin(), archs.end(), hostArch) == archs.end())
	 return _error->Error(_("No architecture information available for %s. See apt.conf(5) APT::Architectures for setup"), hostArch.c_str());
      StripMultiArch = false;
   }
   else
      StripMultiArch = true;

   unsigned J = 0;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      string Src;
      pkgSrcRecords::Parser *Last = FindSrc(*I,Recs,SrcRecs,Src,Cache);
      if (Last == 0)
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());
            
      // Process the build-dependencies
      vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;
      // FIXME: Can't specify architecture to use for [wildcard] matching, so switch default arch temporary
      if (hostArch.empty() == false)
      {
	 std::string nativeArch = _config->Find("APT::Architecture");
	 _config->Set("APT::Architecture", hostArch);
	 bool Success = Last->BuildDepends(BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch);
	 _config->Set("APT::Architecture", nativeArch);
	 if (Success == false)
	    return _error->Error(_("Unable to get build-dependency information for %s"),Src.c_str());
      }
      else if (Last->BuildDepends(BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch) == false)
	    return _error->Error(_("Unable to get build-dependency information for %s"),Src.c_str());
   
      // Also ensure that build-essential packages are present
      Configuration::Item const *Opts = _config->Tree("APT::Build-Essential");
      if (Opts) 
	 Opts = Opts->Child;
      for (; Opts; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;

         pkgSrcRecords::Parser::BuildDepRec rec;
	 rec.Package = Opts->Value;
	 rec.Type = pkgSrcRecords::Parser::BuildDependIndep;
	 rec.Op = 0;
	 BuildDeps.push_back(rec);
      }

      if (BuildDeps.empty() == true)
      {
	 ioprintf(c1out,_("%s has no build depends.\n"),Src.c_str());
	 continue;
      }

      // Install the requested packages
      vector <pkgSrcRecords::Parser::BuildDepRec>::iterator D;
      pkgProblemResolver Fix(Cache);
      bool skipAlternatives = false; // skip remaining alternatives in an or group
      for (D = BuildDeps.begin(); D != BuildDeps.end(); ++D)
      {
         bool hasAlternatives = (((*D).Op & pkgCache::Dep::Or) == pkgCache::Dep::Or);

         if (skipAlternatives == true)
         {
            /*
             * if there are alternatives, we've already picked one, so skip
             * the rest
             *
             * TODO: this means that if there's a build-dep on A|B and B is
             * installed, we'll still try to install A; more importantly,
             * if A is currently broken, we cannot go back and try B. To fix 
             * this would require we do a Resolve cycle for each package we 
             * add to the install list. Ugh
             */
            if (!hasAlternatives)
               skipAlternatives = false; // end of or group
            continue;
         }

         if ((*D).Type == pkgSrcRecords::Parser::BuildConflict ||
	     (*D).Type == pkgSrcRecords::Parser::BuildConflictIndep)
         {
            pkgCache::GrpIterator Grp = Cache->FindGrp((*D).Package);
            // Build-conflicts on unknown packages are silently ignored
            if (Grp.end() == true)
               continue;

	    for (pkgCache::PkgIterator Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
	    {
	       pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);
	       /*
		* Remove if we have an installed version that satisfies the
		* version criteria
		*/
	       if (IV.end() == false &&
		   Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
		  TryToInstallBuildDep(Pkg,Cache,Fix,true,false);
	    }
         }
	 else // BuildDep || BuildDepIndep
         {
            if (_config->FindB("Debug::BuildDeps",false) == true)
                 cout << "Looking for " << (*D).Package << "...\n";

	    pkgCache::PkgIterator Pkg;

	    // Cross-Building?
	    if (StripMultiArch == false && D->Type != pkgSrcRecords::Parser::BuildDependIndep)
	    {
	       size_t const colon = D->Package.find(":");
	       if (colon != string::npos)
	       {
		  if (strcmp(D->Package.c_str() + colon, ":any") == 0 || strcmp(D->Package.c_str() + colon, ":native") == 0)
		     Pkg = Cache->FindPkg(D->Package.substr(0,colon));
		  else
		     Pkg = Cache->FindPkg(D->Package);
	       }
	       else
		  Pkg = Cache->FindPkg(D->Package, hostArch);

	       // a bad version either is invalid or doesn't satify dependency
	       #define BADVER(Ver) (Ver.end() == true || \
				    (D->Version.empty() == false && \
				     Cache->VS().CheckDep(Ver.VerStr(),D->Op,D->Version.c_str()) == false))

	       APT::VersionList verlist;
	       if (Pkg.end() == false)
	       {
		  pkgCache::VerIterator Ver = (*Cache)[Pkg].InstVerIter(*Cache);
		  if (BADVER(Ver) == false)
		     verlist.insert(Ver);
		  Ver = (*Cache)[Pkg].CandidateVerIter(*Cache);
		  if (BADVER(Ver) == false)
		     verlist.insert(Ver);
	       }
	       if (verlist.empty() == true)
	       {
		  pkgCache::PkgIterator BuildPkg = Cache->FindPkg(D->Package, "native");
		  if (BuildPkg.end() == false && Pkg != BuildPkg)
		  {
		     pkgCache::VerIterator Ver = (*Cache)[BuildPkg].InstVerIter(*Cache);
		     if (BADVER(Ver) == false)
			verlist.insert(Ver);
		     Ver = (*Cache)[BuildPkg].CandidateVerIter(*Cache);
		     if (BADVER(Ver) == false)
			verlist.insert(Ver);
		  }
	       }
	       #undef BADVER

	       string forbidden;
	       // We need to decide if host or build arch, so find a version we can look at
	       APT::VersionList::const_iterator Ver = verlist.begin();
	       for (; Ver != verlist.end(); ++Ver)
	       {
		  forbidden.clear();
		  if (Ver->MultiArch == pkgCache::Version::None || Ver->MultiArch == pkgCache::Version::All)
		  {
		     if (colon == string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
			forbidden = "Multi-Arch: none";
		     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		  }
		  else if (Ver->MultiArch == pkgCache::Version::Same)
		  {
		     if (colon == string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
			forbidden = "Multi-Arch: same";
		     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		  }
		  else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
		  {
		     if (colon == string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0 ||
			      strcmp(D->Package.c_str() + colon, ":native") == 0)
			forbidden = "Multi-Arch: foreign";
		  }
		  else if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
		  {
		     if (colon == string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
		     {
			// prefer any installed over preferred non-installed architectures
			pkgCache::GrpIterator Grp = Ver.ParentPkg().Group();
			// we don't check for version here as we are better of with upgrading than remove and install
			for (Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
			   if (Pkg.CurrentVer().end() == false)
			      break;
			if (Pkg.end() == true)
			   Pkg = Grp.FindPreferredPkg(true);
		     }
		     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		  }

		  if (forbidden.empty() == false)
		  {
		     if (_config->FindB("Debug::BuildDeps",false) == true)
			cout << D->Package.substr(colon, string::npos) << " is not allowed from " << forbidden << " package " << (*D).Package << " (" << Ver.VerStr() << ")" << endl;
		     continue;
		  }

		  //we found a good version
		  break;
	       }
	       if (Ver == verlist.end())
	       {
		  if (_config->FindB("Debug::BuildDeps",false) == true)
		     cout << " No multiarch info as we have no satisfying installed nor candidate for " << D->Package << " on build or host arch" << endl;

		  if (forbidden.empty() == false)
		  {
		     if (hasAlternatives)
			continue;
		     return _error->Error(_("%s dependency for %s can't be satisfied "
					    "because %s is not allowed on '%s' packages"),
					  Last->BuildDepType(D->Type), Src.c_str(),
					  D->Package.c_str(), forbidden.c_str());
		  }
	       }
	    }
	    else
	       Pkg = Cache->FindPkg(D->Package);

	    if (Pkg.end() == true || (Pkg->VersionList == 0 && Pkg->ProvidesList == 0))
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                    cout << " (not found)" << (*D).Package << endl;

               if (hasAlternatives)
                  continue;

               return _error->Error(_("%s dependency for %s cannot be satisfied "
                                      "because the package %s cannot be found"),
                                    Last->BuildDepType((*D).Type),Src.c_str(),
                                    (*D).Package.c_str());
            }

	    pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);
	    if (IV.end() == false)
	    {
	       if (_config->FindB("Debug::BuildDeps",false) == true)
		  cout << "  Is installed\n";

	       if (D->Version.empty() == true ||
		   Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
	       {
		  skipAlternatives = hasAlternatives;
		  continue;
	       }

	       if (_config->FindB("Debug::BuildDeps",false) == true)
		  cout << "    ...but the installed version doesn't meet the version requirement\n";

	       if (((*D).Op & pkgCache::Dep::LessEq) == pkgCache::Dep::LessEq)
		  return _error->Error(_("Failed to satisfy %s dependency for %s: Installed package %s is too new"),
					Last->BuildDepType((*D).Type), Src.c_str(), Pkg.FullName(true).c_str());
	    }

	    // Only consider virtual packages if there is no versioned dependency
	    if ((*D).Version.empty() == true)
	    {
	       /*
		* If this is a virtual package, we need to check the list of
		* packages that provide it and see if any of those are
		* installed
		*/
	       pkgCache::PrvIterator Prv = Pkg.ProvidesList();
	       for (; Prv.end() != true; ++Prv)
	       {
		  if (_config->FindB("Debug::BuildDeps",false) == true)
		     cout << "  Checking provider " << Prv.OwnerPkg().FullName() << endl;

		  if ((*Cache)[Prv.OwnerPkg()].InstVerIter(*Cache).end() == false)
		     break;
	       }

	       if (Prv.end() == false)
	       {
		  if (_config->FindB("Debug::BuildDeps",false) == true)
		     cout << "  Is provided by installed package " << Prv.OwnerPkg().FullName() << endl;
		  skipAlternatives = hasAlternatives;
		  continue;
	       }
	    }
	    else // versioned dependency
	    {
	       pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);
	       if (CV.end() == true ||
		   Cache->VS().CheckDep(CV.VerStr(),(*D).Op,(*D).Version.c_str()) == false)
	       {
		  if (hasAlternatives)
		     continue;
		  else if (CV.end() == false)
		     return _error->Error(_("%s dependency for %s cannot be satisfied "
					    "because candidate version of package %s "
					    "can't satisfy version requirements"),
					  Last->BuildDepType(D->Type), Src.c_str(),
					  D->Package.c_str());
		  else
		     return _error->Error(_("%s dependency for %s cannot be satisfied "
					    "because package %s has no candidate version"),
					  Last->BuildDepType(D->Type), Src.c_str(),
					  D->Package.c_str());
	       }
	    }

            if (TryToInstallBuildDep(Pkg,Cache,Fix,false,false,false) == true)
            {
               // We successfully installed something; skip remaining alternatives
               skipAlternatives = hasAlternatives;
	       if(_config->FindB("APT::Get::Build-Dep-Automatic", false) == true)
		  Cache->MarkAuto(Pkg, true);
               continue;
            }
            else if (hasAlternatives)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cout << "  Unsatisfiable, trying alternatives\n";
               continue;
            }
            else
            {
               return _error->Error(_("Failed to satisfy %s dependency for %s: %s"),
                                    Last->BuildDepType((*D).Type),
                                    Src.c_str(),
                                    (*D).Package.c_str());
            }
	 }	       
      }

      if (Fix.Resolve(true) == false)
	 _error->Discard();
      
      // Now we check the state of the packages,
      if (Cache->BrokenCount() != 0)
      {
	 ShowBroken(cout, Cache, false);
	 return _error->Error(_("Build-dependencies for %s could not be satisfied."),*I);
      }
   }
  
   if (InstallPackages(Cache, false, true) == false)
      return _error->Error(_("Failed to process build dependencies"));
   return true;
}
									/*}}}*/
// GetChangelogPath - return a path pointing to a changelog file or dir /*{{{*/
// ---------------------------------------------------------------------
/* This returns a "path" string for the changelog url construction.
 * Please note that its not complete, it either needs a "/changelog"
 * appended (for the packages.debian.org/changelogs site) or a
 * ".changelog" (for third party sites that store the changelog in the
 * pool/ next to the deb itself)
 * Example return: "pool/main/a/apt/apt_0.8.8ubuntu3" 
 */
static string GetChangelogPath(CacheFile &Cache, 
                        pkgCache::PkgIterator Pkg,
                        pkgCache::VerIterator Ver)
{
   string path;

   pkgRecords Recs(Cache);
   pkgRecords::Parser &rec=Recs.Lookup(Ver.FileList());
   string srcpkg = rec.SourcePkg().empty() ? Pkg.Name() : rec.SourcePkg();
   string ver = Ver.VerStr();
   // if there is a source version it always wins
   if (rec.SourceVer() != "")
      ver = rec.SourceVer();
   path = flNotFile(rec.FileName());
   path += srcpkg + "_" + StripEpoch(ver);
   return path;
}
									/*}}}*/
// GuessThirdPartyChangelogUri - return url 			        /*{{{*/
// ---------------------------------------------------------------------
/* Contruct a changelog file path for third party sites that do not use
 * packages.debian.org/changelogs
 * This simply uses the ArchiveURI() of the source pkg and looks for
 * a .changelog file there, Example for "mediabuntu":
 * apt-get changelog mplayer-doc:
 *  http://packages.medibuntu.org/pool/non-free/m/mplayer/mplayer_1.0~rc4~try1.dsfg1-1ubuntu1+medibuntu1.changelog
 */
static bool GuessThirdPartyChangelogUri(CacheFile &Cache, 
                                 pkgCache::PkgIterator Pkg,
                                 pkgCache::VerIterator Ver,
                                 string &out_uri)
{
   // get the binary deb server path
   pkgCache::VerFileIterator Vf = Ver.FileList();
   if (Vf.end() == true)
      return false;
   pkgCache::PkgFileIterator F = Vf.File();
   pkgIndexFile *index;
   pkgSourceList *SrcList = Cache.GetSourceList();
   if(SrcList->FindIndex(F, index) == false)
      return false;

   // get archive uri for the binary deb
   string path_without_dot_changelog = GetChangelogPath(Cache, Pkg, Ver);
   out_uri = index->ArchiveURI(path_without_dot_changelog + ".changelog");

   // now strip away the filename and add srcpkg_srcver.changelog
   return true;
}
									/*}}}*/
// DownloadChangelog - Download the changelog 			        /*{{{*/
// ---------------------------------------------------------------------
static bool DownloadChangelog(CacheFile &CacheFile, pkgAcquire &Fetcher, 
                       pkgCache::VerIterator Ver, string targetfile)
/* Download a changelog file for the given package version to
 * targetfile. This will first try the server from Apt::Changelogs::Server
 * (http://packages.debian.org/changelogs by default) and if that gives
 * a 404 tries to get it from the archive directly (see 
 * GuessThirdPartyChangelogUri for details how)
 */
{
   string path;
   string descr;
   string server;
   string changelog_uri;

   // data structures we need
   pkgCache::PkgIterator Pkg = Ver.ParentPkg();

   // make the server root configurable
   server = _config->Find("Apt::Changelogs::Server",
                          "http://packages.debian.org/changelogs");
   path = GetChangelogPath(CacheFile, Pkg, Ver);
   strprintf(changelog_uri, "%s/%s/changelog", server.c_str(), path.c_str());
   if (_config->FindB("APT::Get::Print-URIs", false) == true)
   {
      std::cout << '\'' << changelog_uri << '\'' << std::endl;
      return true;
   }

   strprintf(descr, _("Changelog for %s (%s)"), Pkg.Name(), changelog_uri.c_str());
   // queue it
   new pkgAcqFile(&Fetcher, changelog_uri, "", 0, descr, Pkg.Name(), "ignored", targetfile);

   // try downloading it, if that fails, try third-party-changelogs location
   // FIXME: Fetcher.Run() is "Continue" even if I get a 404?!?
   Fetcher.Run();
   if (!FileExists(targetfile))
   {
      string third_party_uri;
      if (GuessThirdPartyChangelogUri(CacheFile, Pkg, Ver, third_party_uri))
      {
         strprintf(descr, _("Changelog for %s (%s)"), Pkg.Name(), third_party_uri.c_str());
         new pkgAcqFile(&Fetcher, third_party_uri, "", 0, descr, Pkg.Name(), "ignored", targetfile);
         Fetcher.Run();
      }
   }

   if (FileExists(targetfile))
      return true;

   // error
   return _error->Error("changelog download failed");
}
									/*}}}*/
// DoChangelog - Get changelog from the command line			/*{{{*/
// ---------------------------------------------------------------------
static bool DoChangelog(CommandLine &CmdL)
{
   CacheFile Cache;
   if (Cache.ReadOnlyOpen() == false)
      return false;
   
   APT::CacheSetHelper helper(c0out);
   APT::VersionList verset = APT::VersionList::FromCommandLine(Cache,
		CmdL.FileList + 1, APT::VersionList::CANDIDATE, helper);
   if (verset.empty() == true)
      return false;
   pkgAcquire Fetcher;

   if (_config->FindB("APT::Get::Print-URIs", false) == true)
   {
      bool Success = true;
      for (APT::VersionList::const_iterator Ver = verset.begin();
	   Ver != verset.end(); ++Ver)
	 Success &= DownloadChangelog(Cache, Fetcher, Ver, "");
      return Success;
   }

   AcqTextStatus Stat(ScreenWidth, _config->FindI("quiet",0));
   Fetcher.Setup(&Stat);

   bool const downOnly = _config->FindB("APT::Get::Download-Only", false);

   char tmpname[100];
   const char* tmpdir = NULL;
   if (downOnly == false)
   {
      std::string systemTemp = GetTempDir();
      snprintf(tmpname, sizeof(tmpname), "%s/apt-changelog-XXXXXX", 
               systemTemp.c_str());
      tmpdir = mkdtemp(tmpname);
      if (tmpdir == NULL)
	 return _error->Errno("mkdtemp", "mkdtemp failed");
   }

   for (APT::VersionList::const_iterator Ver = verset.begin(); 
        Ver != verset.end(); 
        ++Ver) 
   {
      string changelogfile;
      if (downOnly == false)
	 changelogfile.append(tmpname).append("/changelog");
      else
	 changelogfile.append(Ver.ParentPkg().Name()).append(".changelog");
      if (DownloadChangelog(Cache, Fetcher, Ver, changelogfile) && downOnly == false)
      {
         DisplayFileInPager(changelogfile);
         // cleanup temp file
         unlink(changelogfile.c_str());
      }
   }
   // clenaup tmp dir
   if (tmpdir != NULL)
      rmdir(tmpdir);
   return true;
}
									/*}}}*/
// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool ShowHelp(CommandLine &)
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
	    
   if (_config->FindB("version") == true)
   {
      cout << _("Supported modules:") << endl;
      
      for (unsigned I = 0; I != pkgVersioningSystem::GlobalListLen; I++)
      {
	 pkgVersioningSystem *VS = pkgVersioningSystem::GlobalList[I];
	 if (_system != 0 && _system->VS == VS)
	    cout << '*';
	 else
	    cout << ' ';
	 cout << "Ver: " << VS->Label << endl;
	 
	 /* Print out all the packaging systems that will work with 
	    this VS */
	 for (unsigned J = 0; J != pkgSystem::GlobalListLen; J++)
	 {
	    pkgSystem *Sys = pkgSystem::GlobalList[J];
	    if (_system == Sys)
	       cout << '*';
	    else
	       cout << ' ';
	    if (Sys->VS->TestCompatibility(*VS) == true)
	       cout << "Pkg:  " << Sys->Label << " (Priority " << Sys->Score(*_config) << ")" << endl;
	 }
      }
      
      for (unsigned I = 0; I != pkgSourceList::Type::GlobalListLen; I++)
      {
	 pkgSourceList::Type *Type = pkgSourceList::Type::GlobalList[I];
	 cout << " S.L: '" << Type->Name << "' " << Type->Label << endl;
      }      
      
      for (unsigned I = 0; I != pkgIndexFile::Type::GlobalListLen; I++)
      {
	 pkgIndexFile::Type *Type = pkgIndexFile::Type::GlobalList[I];
	 cout << " Idx: " << Type->Label << endl;
      }      
      
      return true;
   }
   
   cout << 
    _("Usage: apt-get [options] command\n"
      "       apt-get [options] install|remove pkg1 [pkg2 ...]\n"
      "       apt-get [options] source pkg1 [pkg2 ...]\n"
      "\n"
      "apt-get is a simple command line interface for downloading and\n"
      "installing packages. The most frequently used commands are update\n"
      "and install.\n"   
      "\n"
      "Commands:\n"
      "   update - Retrieve new lists of packages\n"
      "   upgrade - Perform an upgrade\n"
      "   install - Install new packages (pkg is libc6 not libc6.deb)\n"
      "   remove - Remove packages\n"
      "   autoremove - Remove automatically all unused packages\n"
      "   purge - Remove packages and config files\n"
      "   source - Download source archives\n"
      "   build-dep - Configure build-dependencies for source packages\n"
      "   dist-upgrade - Distribution upgrade, see apt-get(8)\n"
      "   dselect-upgrade - Follow dselect selections\n"
      "   clean - Erase downloaded archive files\n"
      "   autoclean - Erase old downloaded archive files\n"
      "   check - Verify that there are no broken dependencies\n"
      "   changelog - Download and display the changelog for the given package\n"
      "   download - Download the binary package into the current directory\n"
      "\n"
      "Options:\n"
      "  -h  This help text.\n"
      "  -q  Loggable output - no progress indicator\n"
      "  -qq No output except for errors\n"
      "  -d  Download only - do NOT install or unpack archives\n"
      "  -s  No-act. Perform ordering simulation\n"
      "  -y  Assume Yes to all queries and do not prompt\n"
      "  -f  Attempt to correct a system with broken dependencies in place\n"
      "  -m  Attempt to continue if archives are unlocatable\n"
      "  -u  Show a list of upgraded packages as well\n"
      "  -b  Build the source package after fetching it\n"
      "  -V  Show verbose version numbers\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See the apt-get(8), sources.list(5) and apt.conf(5) manual\n"
      "pages for more information and options.\n"
      "                       This APT has Super Cow Powers.\n");
   return true;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine::Dispatch Cmds[] = {{"update",&DoUpdate},
                                   {"upgrade",&DoUpgrade},
                                   {"install",&DoInstall},
                                   {"remove",&DoInstall},
                                   {"purge",&DoInstall},
				   {"autoremove",&DoInstall},
				   {"markauto",&DoMarkAuto},
				   {"unmarkauto",&DoMarkAuto},
                                   {"dist-upgrade",&DoDistUpgrade},
                                   {"dselect-upgrade",&DoDSelectUpgrade},
				   {"build-dep",&DoBuildDep},
                                   {"clean",&DoClean},
                                   {"autoclean",&DoAutoClean},
                                   {"check",&DoCheck},
				   {"source",&DoSource},
                                   {"download",&DoDownload},
                                   {"changelog",&DoChangelog},
				   {"moo",&DoMoo},
				   {"help",&ShowHelp},
                                   {0,0}};

   std::vector<CommandLine::Args> Args = getCommandArgs("apt-get", CommandLine::GetCommand(Cmds, argc, argv));

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(),_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      if (_config->FindB("version") == true)
	 ShowHelp(CmdL);
	 
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
   {
      ShowHelp(CmdL);
      return 0;
   }

   // see if we are in simulate mode
   CheckSimulateMode(CmdL);

   // Init the signals
   InitSignals();

   // Setup the output streams
   InitOutput();

   // Match the operation
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   return Errors == true ? 100 : 0;
}
									/*}}}*/

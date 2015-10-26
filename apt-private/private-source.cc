// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-download.h>
#include <apt-private/private-install.h>
#include <apt-private/private-source.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

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
	 std::cout << "  Trying to remove " << Pkg << std::endl;
      else
	 std::cout << "  Trying to install " << Pkg << std::endl;
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
// GetReleaseFileForSourceRecord - Return Suite for the given srcrecord	/*{{{*/
static pkgCache::RlsFileIterator GetReleaseFileForSourceRecord(CacheFile &CacheFile,
      pkgSourceList *SrcList, pkgSrcRecords::Parser *Parse)
{
   // try to find release
   const pkgIndexFile& CurrentIndexFile = Parse->Index();

   for (pkgSourceList::const_iterator S = SrcList->begin(); 
	 S != SrcList->end(); ++S)
   {
      std::vector<pkgIndexFile *> *Indexes = (*S)->GetIndexFiles();
      for (std::vector<pkgIndexFile *>::const_iterator IF = Indexes->begin();
	    IF != Indexes->end(); ++IF)
      {
	 if (&CurrentIndexFile == (*IF))
	    return (*S)->FindInCache(CacheFile, false);
      }
   }
   return pkgCache::RlsFileIterator(CacheFile);
}
									/*}}}*/
// FindSrc - Find a source record					/*{{{*/
static pkgSrcRecords::Parser *FindSrc(const char *Name,
			       pkgSrcRecords &SrcRecs,std::string &Src,
			       CacheFile &CacheFile)
{
   std::string VerTag, UserRequestedVerTag;
   std::string ArchTag = "";
   std::string RelTag = _config->Find("APT::Default-Release");
   std::string TmpSrc = Name;
   pkgDepCache *Cache = CacheFile.GetDepCache();

   // extract release
   size_t found = TmpSrc.find_last_of("/");
   if (found != std::string::npos)
   {
      RelTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }
   // extract the version
   found = TmpSrc.find_last_of("=");
   if (found != std::string::npos)
   {
      VerTag = UserRequestedVerTag = TmpSrc.substr(found+1);
      TmpSrc = TmpSrc.substr(0,found);
   }
   // extract arch
   found = TmpSrc.find_last_of(":");
   if (found != std::string::npos)
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
		  // the Version we have is possibly fuzzy or includes binUploads,
		  // so we use the Version of the SourcePkg (empty if same as package)
		  Src = Ver.SourcePkgName();
		  VerTag = Ver.SourceVerStr();
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
	    if (strcmp(Ver.SourcePkgName(),Ver.ParentPkg().Name()) != 0)
	       Src = Ver.SourcePkgName();
	    if (VerTag.empty() == true && strcmp(Ver.SourceVerStr(),Ver.VerStr()) != 0)
	       VerTag = Ver.SourceVerStr();
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
   std::string Version;
   pkgSourceList *SrcList = CacheFile.GetSourceList();

   /* Iterate over all of the hits, which includes the resulting
      binary packages in the search */
	       pkgSrcRecords::Parser *Parse;
	       while (true) 
	       {
		  SrcRecs.Restart();
		  while ((Parse = SrcRecs.Find(Src.c_str(), MatchSrcOnly)) != 0) 
		  {
		     const std::string Ver = Parse->Version();

		     // See if we need to look for a specific release tag
		     if (RelTag != "" && UserRequestedVerTag == "")
		     {
			pkgCache::RlsFileIterator const Rls = GetReleaseFileForSourceRecord(CacheFile, SrcList, Parse);
			if (Rls.end() == false)
			{
			   if ((Rls->Archive != 0 && RelTag == Rls.Archive()) ||
				 (Rls->Codename != 0 && RelTag == Rls.Codename()))
			   {
			      Last = Parse;
			      Offset = Parse->Offset();
			      Version = Ver;
			   }
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
// DoSource - Fetch a source archive					/*{{{*/
// ---------------------------------------------------------------------
/* Fetch souce packages */
struct DscFile
{
   std::string Package;
   std::string Version;
   std::string Dsc;
};
bool DoSource(CommandLine &CmdL)
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
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   std::unique_ptr<DscFile[]> Dsc(new DscFile[CmdL.FileSize()]);

   // insert all downloaded uris into this set to avoid downloading them
   // twice
   std::set<std::string> queued;

   // Diff only mode only fetches .diff files
   bool const diffOnly = _config->FindB("APT::Get::Diff-Only", false);
   // Tar only mode only fetches .tar files
   bool const tarOnly = _config->FindB("APT::Get::Tar-Only", false);
   // Dsc only mode only fetches .dsc files
   bool const dscOnly = _config->FindB("APT::Get::Dsc-Only", false);

   // Load the requestd sources into the fetcher
   aptAcquireWithTextStatus Fetcher;
   unsigned J = 0;
   std::vector<std::string> UntrustedList;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++, J++)
   {
      std::string Src;
      pkgSrcRecords::Parser *Last = FindSrc(*I,SrcRecs,Src,Cache);
      if (Last == 0) {
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());
      }

      if (Last->Index().IsTrusted() == false)
	 UntrustedList.push_back(Src);

      std::string srec = Last->AsStr();
      std::string::size_type pos = srec.find("\nVcs-");
      while (pos != std::string::npos)
      {
	 pos += strlen("\nVcs-");
	 std::string vcs = srec.substr(pos,srec.find(":",pos)-pos);
	 if(vcs == "Browser") 
	 {
	    pos = srec.find("\nVcs-", pos);
	    continue;
	 }
	 pos += vcs.length()+2;
	 std::string::size_type epos = srec.find("\n", pos);
	 std::string const uri = srec.substr(pos,epos-pos);
	 ioprintf(c1out, _("NOTICE: '%s' packaging is maintained in "
		  "the '%s' version control system at:\n"
		  "%s\n"),
	       Src.c_str(), vcs.c_str(), uri.c_str());
	 std::string vcscmd;
	 if (vcs == "Bzr")
	    vcscmd = "bzr branch " + uri;
	 else if (vcs == "Git")
	    vcscmd = "git clone " + uri;

	 if (vcscmd.empty() == false)
	    ioprintf(c1out,_("Please use:\n%s\n"
		     "to retrieve the latest (possibly unreleased) "
		     "updates to the package.\n"),
		  vcscmd.c_str());
	 break;
      }

      // Back track
      std::vector<pkgSrcRecords::File2> Lst;
      if (Last->Files2(Lst) == false) {
	 return false;
      }

      // Load them into the fetcher
      for (std::vector<pkgSrcRecords::File2>::const_iterator I = Lst.begin();
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
	 if (I->Hashes.usable() == false && _config->FindB("APT::Get::AllowUnauthenticated",false) == false)
	 {
	    ioprintf(c1out, "Skipping download of file '%s' as requested hashsum is not available for authentication\n",
		  localFile.c_str());
	    continue;
	 }

	 new pkgAcqFile(&Fetcher,Last->Index().ArchiveURI(I->Path),
	       I->Hashes, I->FileSize, Last->Index().SourceInfo(*Last,*I), Src);
      }
   }

   // Display statistics
   unsigned long long FetchBytes = Fetcher.FetchNeeded();
   unsigned long long FetchPBytes = Fetcher.PartialPresent();
   unsigned long long DebBytes = Fetcher.TotalNeeded();

   if (CheckFreeSpaceBeforeDownload(".", (FetchBytes - FetchPBytes)) == false)
      return false;

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
	 ioprintf(std::cout,_("Fetch source %s\n"),Dsc[I].Package.c_str());
      return true;
   }

   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 std::cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' << 
	    I->Owner->FileSize << ' ' << I->Owner->HashSum() << std::endl;
      return true;
   }

   // check authentication status of the source as well
   if (UntrustedList.empty() == false && AuthPrompt(UntrustedList, false) == false)
      return false;

   // Run it
   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true)
   {
      return _error->Error(_("Failed to fetch some archives."));
   }

   if (_config->FindB("APT::Get::Download-only",false) == true)
   {
      c1out << _("Download complete and in download only mode") << std::endl;
      return true;
   }

   // Unpack the sources
   pid_t Process = ExecFork();

   if (Process == 0)
   {
      bool const fixBroken = _config->FindB("APT::Get::Fix-Broken", false);
      for (unsigned I = 0; I != J; ++I)
      {
	 std::string Dir = Dsc[I].Package + '-' + Cache->VS().UpstreamVersion(Dsc[I].Version.c_str());

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
	    std::string buildopts = _config->Find("APT::Get::Host-Architecture");
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

   return ExecWait(Process, "dpkg-source");
}
									/*}}}*/
// DoBuildDep - Install/removes packages to satisfy build dependencies  /*{{{*/
// ---------------------------------------------------------------------
/* This function will look at the build depends list of the given source 
   package and install the necessary packages to make it true, or fail. */
bool DoBuildDep(CommandLine &CmdL)
{
   CacheFile Cache;

   _config->Set("APT::Install-Recommends", false);

   bool WantLock = _config->FindB("APT::Get::Print-URIs", false) == false;

   if (Cache.Open(WantLock) == false)
      return false;

   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to check builddeps for"));

   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();

   // Create the text record parsers
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   bool StripMultiArch;
   std::string hostArch = _config->Find("APT::Get::Host-Architecture");
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
      std::string Src;
      pkgSrcRecords::Parser *Last = 0;
      std::unique_ptr<pkgSrcRecords::Parser> LastOwner;

      // an unpacked debian source tree
      using APT::String::Startswith;
      if ((Startswith(*I, "./") || Startswith(*I, "/")) &&
	    DirectoryExists(*I))
      {
	 ioprintf(c1out, _("Note, using directory '%s' to get the build dependencies\n"), *I);
	 // FIXME: how can we make this more elegant?
	 std::string TypeName = "Debian control file";
	 pkgIndexFile::Type *Type = pkgIndexFile::Type::GetType(TypeName.c_str());
	 if(Type != NULL)
	    LastOwner.reset(Last = Type->CreateSrcPkgParser(*I));
      }
      // if its a local file (e.g. .dsc) use this
      else if (FileExists(*I))
      {
	 ioprintf(c1out, _("Note, using file '%s' to get the build dependencies\n"), *I);

	 // see if we can get a parser for this pkgIndexFile type
	 std::string TypeName = "Debian " + flExtension(*I) + " file";
	 pkgIndexFile::Type *Type = pkgIndexFile::Type::GetType(TypeName.c_str());
	 if(Type != NULL)
	    LastOwner.reset(Last = Type->CreateSrcPkgParser(*I));
      } else {
	 // normal case, search the cache for the source file
	 Last = FindSrc(*I,SrcRecs,Src,Cache);
      }

      if (Last == 0)
	 return _error->Error(_("Unable to find a source package for %s"),Src.c_str());

      // Process the build-dependencies
      std::vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;
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
      std::vector <pkgSrcRecords::Parser::BuildDepRec>::iterator D;
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
	       std::cout << "Looking for " << (*D).Package << "...\n";

	    pkgCache::PkgIterator Pkg;

	    // Cross-Building?
	    if (StripMultiArch == false && D->Type != pkgSrcRecords::Parser::BuildDependIndep)
	    {
	       size_t const colon = D->Package.find(":");
	       if (colon != std::string::npos)
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

	       std::string forbidden;
	       // We need to decide if host or build arch, so find a version we can look at
	       APT::VersionList::const_iterator Ver = verlist.begin();
	       for (; Ver != verlist.end(); ++Ver)
	       {
		  forbidden.clear();
		  if (Ver->MultiArch == pkgCache::Version::No || Ver->MultiArch == pkgCache::Version::All)
		  {
		     if (colon == std::string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
			forbidden = "Multi-Arch: no";
		     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		  }
		  else if (Ver->MultiArch == pkgCache::Version::Same)
		  {
		     if (colon == std::string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
			forbidden = "Multi-Arch: same";
		     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		  }
		  else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
		  {
		     if (colon == std::string::npos)
			Pkg = Ver.ParentPkg().Group().FindPkg("native");
		     else if (strcmp(D->Package.c_str() + colon, ":any") == 0 ||
			   strcmp(D->Package.c_str() + colon, ":native") == 0)
			forbidden = "Multi-Arch: foreign";
		  }
		  else if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
		  {
		     if (colon == std::string::npos)
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
			std::cout << D->Package.substr(colon, std::string::npos) << " is not allowed from " << forbidden << " package " << (*D).Package << " (" << Ver.VerStr() << ")" << std::endl;
		     continue;
		  }

		  //we found a good version
		  break;
	       }
	       if (Ver == verlist.end())
	       {
		  if (_config->FindB("Debug::BuildDeps",false) == true)
		     std::cout << " No multiarch info as we have no satisfying installed nor candidate for " << D->Package << " on build or host arch" << std::endl;

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
		  std::cout << " (not found)" << (*D).Package << std::endl;

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
		  std::cout << "  Is installed\n";

	       if (D->Version.empty() == true ||
		     Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
	       {
		  skipAlternatives = hasAlternatives;
		  continue;
	       }

	       if (_config->FindB("Debug::BuildDeps",false) == true)
		  std::cout << "    ...but the installed version doesn't meet the version requirement\n";

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
		     std::cout << "  Checking provider " << Prv.OwnerPkg().FullName() << std::endl;

		  if ((*Cache)[Prv.OwnerPkg()].InstVerIter(*Cache).end() == false)
		     break;
	       }

	       if (Prv.end() == false)
	       {
		  if (_config->FindB("Debug::BuildDeps",false) == true)
		     std::cout << "  Is provided by installed package " << Prv.OwnerPkg().FullName() << std::endl;
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
		  std::cout << "  Unsatisfiable, trying alternatives\n";
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
	 ShowBroken(std::cout, Cache, false);
	 return _error->Error(_("Build-dependencies for %s could not be satisfied."),*I);
      }
   }

   if (InstallPackages(Cache, false, true) == false)
      return _error->Error(_("Failed to process build dependencies"));
   return true;
}
									/*}}}*/

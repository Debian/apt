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
#include <apt-pkg/policy.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-download.h>
#include <apt-private/private-install.h>
#include <apt-private/private-source.h>

#include <apt-pkg/debindexfile.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

// GetReleaseFileForSourceRecord - Return Suite for the given srcrecord	/*{{{*/
static pkgCache::RlsFileIterator GetReleaseFileForSourceRecord(CacheFile &CacheFile,
      pkgSourceList const * const SrcList, pkgSrcRecords::Parser const * const Parse)
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
			       CacheFile &Cache)
{
   std::string VerTag, UserRequestedVerTag;
   std::string ArchTag = "";
   std::string RelTag = _config->Find("APT::Default-Release");
   std::string TmpSrc = Name;

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
      Pkg = Cache.GetPkgCache()->FindPkg(TmpSrc, ArchTag);
   else
      Pkg = Cache.GetPkgCache()->FindPkg(TmpSrc);

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
	       if(Cache.GetPkgCache()->VS->CmpVersion(VerTag, Ver.VerStr()) < 0)
		  VerTag = Ver.VerStr();

	    // We match against a concrete version (or a part of this version)
	    if (VerTag.empty() == false &&
		  (fuzzy == true || Cache.GetPkgCache()->VS->CmpVersion(VerTag, Ver.VerStr()) != 0) && // exact match
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

      if (Src.empty() == true && ArchTag.empty() == false)
      {
	 if (VerTag.empty() == false)
	    _error->Error(_("Can not find a package '%s' with version '%s'"),
		  Pkg.FullName().c_str(), VerTag.c_str());
	 if (RelTag.empty() == false)
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
	 if (Cache.BuildPolicy() == false)
	    return nullptr;
	 pkgPolicy * Policy = dynamic_cast<pkgPolicy*>(Cache.GetPolicy());
	 if (Policy == nullptr)
	 {
	    _error->Fatal("Implementation error: dynamic up-casting policy engine failed in FindSrc!");
	    return nullptr;
	 }
	 pkgCache::VerIterator const Ver = Policy->GetCandidateVer(Pkg);
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
   pkgSourceList const * const SrcList = Cache.GetSourceList();

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
		     if (RelTag.empty() == false && UserRequestedVerTag.empty() == true)
		     {
			pkgCache::RlsFileIterator const Rls = GetReleaseFileForSourceRecord(Cache, SrcList, Parse);
			if (Rls.end() == false)
			{
			   if ((Rls->Archive != 0 && RelTag != Rls.Archive()) &&
				 (Rls->Codename != 0 && RelTag != Rls.Codename()))
			      continue;
			}
		     }

		     // Ignore all versions which doesn't fit
		     if (VerTag.empty() == false &&
			   Cache.GetPkgCache()->VS->CmpVersion(VerTag, Ver) != 0) // exact match
			continue;

		     // Newer version or an exact match? Save the hit
		     if (Last == 0 || Cache.GetPkgCache()->VS->CmpVersion(Version,Ver) < 0) {
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
/* Fetch source packages */
struct DscFile
{
   std::string Package;
   std::string Version;
   std::string Dsc;
};
bool DoSource(CommandLine &CmdL)
{
   if (CmdL.FileSize() <= 1)
      return _error->Error(_("Must specify at least one package to fetch source for"));

   CacheFile Cache;
   if (Cache.BuildCaches(false) == false)
      return false;

   // Create the text record parsers
   pkgSourceList * const List = Cache.GetSourceList();
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   std::vector<DscFile> Dsc;
   Dsc.reserve(CmdL.FileSize());

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
   std::vector<std::string> UntrustedList;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
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

      DscFile curDsc;
      // Load them into the fetcher
      for (std::vector<pkgSrcRecords::File2>::const_iterator I = Lst.begin();
	    I != Lst.end(); ++I)
      {
	 // Try to guess what sort of file it is we are getting.
	 if (I->Type == "dsc")
	 {
	    curDsc.Package = Last->Package();
	    curDsc.Version = Last->Version();
	    curDsc.Dsc = flNotDir(I->Path);
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
	    curDsc.Dsc.clear();
	    continue;
	 }

	 new pkgAcqFile(&Fetcher,Last->Index().ArchiveURI(I->Path),
	       I->Hashes, I->FileSize, Last->Index().SourceInfo(*Last,*I), Src);
      }
      Dsc.push_back(std::move(curDsc));
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
      for (auto const &D: Dsc)
	 ioprintf(std::cout, _("Fetch source %s\n"), D.Package.c_str());
      return true;
   }

   // Just print out the uris an exit if the --print-uris flag was used
   if (_config->FindB("APT::Get::Print-URIs") == true)
   {
      pkgAcquire::UriIterator I = Fetcher.UriBegin();
      for (; I != Fetcher.UriEnd(); ++I)
	 std::cout << '\'' << I->URI << "' " << flNotDir(I->Owner->DestFile) << ' ' <<
	    std::to_string(I->Owner->FileSize) << ' ' << I->Owner->HashSum() << std::endl;
      return true;
   }

   // check authentication status of the source as well
   if (UntrustedList.empty() == false && AuthPrompt(UntrustedList, false) == false)
      return false;

   // Run it
   bool Failed = false;
   if (AcquireRun(Fetcher, 0, &Failed, NULL) == false || Failed == true)
      return _error->Error(_("Failed to fetch some archives."));

   if (diffOnly || tarOnly || dscOnly || _config->FindB("APT::Get::Download-only",false) == true)
   {
      c1out << _("Download complete and in download only mode") << std::endl;
      return true;
   }

   bool const fixBroken = _config->FindB("APT::Get::Fix-Broken", false);
   bool SaidCheckIfDpkgDev = false;
   for (auto const &D: Dsc)
   {
      if (unlikely(D.Dsc.empty() == true))
	 continue;
      std::string const Dir = D.Package + '-' + Cache.GetPkgCache()->VS->UpstreamVersion(D.Version.c_str());

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
	 std::string const sourceopts = _config->Find("DPkg::Source-Options", "--no-check -x");
	 std::string S;
	 strprintf(S, "%s %s %s",
	       _config->Find("Dir::Bin::dpkg-source","dpkg-source").c_str(),
	       sourceopts.c_str(), D.Dsc.c_str());
	 if (system(S.c_str()) != 0)
	 {
	    _error->Error(_("Unpack command '%s' failed.\n"), S.c_str());
	    if (SaidCheckIfDpkgDev == false)
	    {
	       _error->Notice(_("Check if the 'dpkg-dev' package is installed.\n"));
	       SaidCheckIfDpkgDev = true;
	    }
	    continue;
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
	    _error->Error(_("Build command '%s' failed.\n"), S.c_str());
	    continue;
	 }
      }
   }
   return true;
}
									/*}}}*/
// DoBuildDep - Install/removes packages to satisfy build dependencies  /*{{{*/
// ---------------------------------------------------------------------
/* This function will look at the build depends list of the given source 
   package and install the necessary packages to make it true, or fail. */
static std::vector<pkgSrcRecords::Parser::BuildDepRec> GetBuildDeps(pkgSrcRecords::Parser * const Last,
      char const * const Src, bool const StripMultiArch, std::string const &hostArch)
{
   std::vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;
   // FIXME: Can't specify architecture to use for [wildcard] matching, so switch default arch temporary
   if (hostArch.empty() == false)
   {
      std::string nativeArch = _config->Find("APT::Architecture");
      _config->Set("APT::Architecture", hostArch);
      bool Success = Last->BuildDepends(BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch);
      _config->Set("APT::Architecture", nativeArch);
      if (Success == false)
      {
	 _error->Error(_("Unable to get build-dependency information for %s"), Src);
	 return {};
      }
   }
   else if (Last->BuildDepends(BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch) == false)
   {
      _error->Error(_("Unable to get build-dependency information for %s"), Src);
      return {};
   }

   if (BuildDeps.empty() == true)
      ioprintf(c1out,_("%s has no build depends.\n"), Src);

   return BuildDeps;
}
static void WriteBuildDependencyPackage(std::ostringstream &buildDepsPkgFile,
      std::string const &PkgName, std::string const &Arch,
      std::vector<pkgSrcRecords::Parser::BuildDepRec> const &Dependencies)
{
   buildDepsPkgFile << "Package: " << PkgName << "\n"
      << "Architecture: " << Arch << "\n"
      << "Version: 1\n";

   bool const IndepOnly = _config->FindB("APT::Get::Indep-Only", false);
   std::string depends, conflicts;
   for (auto const &dep: Dependencies)
   {
      // ArchOnly is handled while parsing the dependencies on input
      if (IndepOnly && (dep.Type == pkgSrcRecords::Parser::BuildDependArch ||
	       dep.Type == pkgSrcRecords::Parser::BuildConflictArch))
	 continue;
      std::string * type;
      if (dep.Type == pkgSrcRecords::Parser::BuildConflict ||
		  dep.Type == pkgSrcRecords::Parser::BuildConflictIndep ||
		  dep.Type == pkgSrcRecords::Parser::BuildConflictArch)
	 type = &conflicts;
      else
	 type = &depends;

      type->append(" ").append(dep.Package);
      if (dep.Version.empty() ==  false)
	 type->append(" (").append(pkgCache::CompTypeDeb(dep.Op)).append(" ").append(dep.Version).append(")");
      if ((dep.Op & pkgCache::Dep::Or) == pkgCache::Dep::Or)
      {
	 type->append("\n  |");
      }
      else
	 type->append(",\n");
   }
   if (depends.empty() == false)
      buildDepsPkgFile << "Depends:\n" << depends;
   if (conflicts.empty() == false)
      buildDepsPkgFile << "Conflicts:\n" << conflicts;
   buildDepsPkgFile << "\n";
}
bool DoBuildDep(CommandLine &CmdL)
{
   CacheFile Cache;
   std::vector<std::string> VolatileCmdL;
   Cache.GetSourceList()->AddVolatileFiles(CmdL, &VolatileCmdL);

   _config->Set("APT::Install-Recommends", false);

   if (CmdL.FileSize() <= 1 && VolatileCmdL.empty())
      return _error->Error(_("Must specify at least one package to check builddeps for"));

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

   std::ostringstream buildDepsPkgFile;
   std::vector<std::pair<std::string,std::string>> pseudoPkgs;
   // deal with the build essentials first
   {
      std::vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;
      for (auto && opt: _config->FindVector("APT::Build-Essential"))
      {
	 if (opt.empty())
	    continue;
	 pkgSrcRecords::Parser::BuildDepRec rec;
	 rec.Package = std::move(opt);
	 rec.Type = pkgSrcRecords::Parser::BuildDependIndep;
	 rec.Op = 0;
	 BuildDeps.push_back(rec);
      }
      std::string const pseudo = "builddeps:essentials";
      std::string const nativeArch = _config->Find("APT::Architecture");
      WriteBuildDependencyPackage(buildDepsPkgFile, pseudo, nativeArch, BuildDeps);
      pseudoPkgs.emplace_back(pseudo, nativeArch);
   }

   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;
   pkgSourceList *List = Cache.GetSourceList();
   std::string const pseudoArch = hostArch.empty() ? _config->Find("APT::Architecture") : hostArch;

   // FIXME: Avoid volatile sources == cmdline assumption
   {
      auto const VolatileSources = List->GetVolatileFiles();
      if (VolatileSources.size() == VolatileCmdL.size())
      {
	 for (size_t i = 0; i < VolatileSources.size(); ++i)
	 {
	    auto const Src = VolatileCmdL[i];
	    if (DirectoryExists(Src))
	       ioprintf(c1out, _("Note, using directory '%s' to get the build dependencies\n"), Src.c_str());
	    else
	       ioprintf(c1out, _("Note, using file '%s' to get the build dependencies\n"), Src.c_str());
	    std::unique_ptr<pkgSrcRecords::Parser> Last(VolatileSources[i]->CreateSrcParser());
	    if (Last == nullptr)
	       return _error->Error(_("Unable to find a source package for %s"), Src.c_str());

	    std::string const pseudo = std::string("builddeps:") + Src;
	    WriteBuildDependencyPackage(buildDepsPkgFile, pseudo, pseudoArch,
		  GetBuildDeps(Last.get(), Src.c_str(), StripMultiArch, hostArch));
	    pseudoPkgs.emplace_back(pseudo, pseudoArch);
	 }
      }
      else
	 return _error->Error("Implementation error: Volatile sources (%lu) and"
	       "commandline elements (%lu) do not match!", VolatileSources.size(),
	       VolatileCmdL.size());
   }

   bool const WantLock = _config->FindB("APT::Get::Print-URIs", false) == false;
   if (CmdL.FileList[1] != 0)
   {
      if (Cache.BuildCaches(WantLock) == false)
	 return false;
      // Create the text record parsers
      pkgSrcRecords SrcRecs(*List);
      if (_error->PendingError() == true)
	 return false;
      for (const char **I = CmdL.FileList + 1; *I != 0; ++I)
      {
	 std::string Src;
	 pkgSrcRecords::Parser * const Last = FindSrc(*I,SrcRecs,Src,Cache);
	 if (Last == nullptr)
	    return _error->Error(_("Unable to find a source package for %s"), *I);

	 std::string const pseudo = std::string("builddeps:") + Src;
	 WriteBuildDependencyPackage(buildDepsPkgFile, pseudo, pseudoArch,
	       GetBuildDeps(Last, Src.c_str(), StripMultiArch, hostArch));
	 pseudoPkgs.emplace_back(pseudo, pseudoArch);
      }
   }

   Cache.AddIndexFile(new debStringPackageIndex(buildDepsPkgFile.str()));

   if (Cache.Open(WantLock) == false)
      return false;
   pkgProblemResolver Fix(Cache.GetDepCache());

   APT::PackageVector removeAgain;
   {
      pkgDepCache::ActionGroup group(Cache);
      TryToInstall InstallAction(Cache, &Fix, false);
      for (auto const &pkg: pseudoPkgs)
      {
	 pkgCache::PkgIterator const Pkg = Cache->FindPkg(pkg.first, pkg.second);
	 if (Pkg.end())
	    continue;
	 Cache->SetCandidateVersion(Pkg.VersionList());
	 InstallAction(Cache[Pkg].CandidateVerIter(Cache));
	 removeAgain.push_back(Pkg);
      }
      InstallAction.doAutoInstall();

      OpTextProgress Progress(*_config);
      bool const resolver_fail = Fix.Resolve(true, &Progress);
      if (resolver_fail == false && Cache->BrokenCount() == 0)
	 return false;
      if (CheckNothingBroken(Cache) == false)
	 return false;
   }
   if (DoAutomaticRemove(Cache) == false)
      return false;

   {
      pkgDepCache::ActionGroup group(Cache);
      if (_config->FindB("APT::Get::Build-Dep-Automatic", false) == false)
      {
	 for (auto const &pkg: removeAgain)
	 {
	    auto const instVer = Cache[pkg].InstVerIter(Cache);
	    if (unlikely(instVer.end() == true))
	       continue;
	    for (auto D = instVer.DependsList(); D.end() != true; ++D)
	    {
	       if (D->Type != pkgCache::Dep::Depends || D.IsMultiArchImplicit())
		  continue;
	       APT::VersionList verlist = APT::VersionList::FromDependency(Cache, D, APT::CacheSetHelper::CANDIDATE);
	       for (auto const &V : verlist)
	       {
		  auto const P = V.ParentPkg();
		  if (Cache[P].InstallVer != V)
		     continue;
		  Cache->MarkAuto(P, false);
	       }
	    }
	 }
      }
      for (auto const &pkg: removeAgain)
	 Cache->MarkDelete(pkg, false, 0, true);
   }

   pseudoPkgs.clear();
   if (_error->PendingError() || InstallPackages(Cache, false, true) == false)
      return _error->Error(_("Failed to process build dependencies"));
   return true;
}
									/*}}}*/

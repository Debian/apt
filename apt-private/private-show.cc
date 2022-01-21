// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-output.h>
#include <apt-private/private-install.h>
#include <apt-private/private-show.h>

#include <ostream>
#include <string>
#include <stdio.h>
#include <unistd.h>

#include <apti18n.h>
									/*}}}*/

pkgRecords::Parser &LookupParser(pkgRecords &Recs, pkgCache::VerIterator const &V, pkgCache::VerFileIterator &Vf) /*{{{*/
{
   Vf = V.FileList();
   for (; Vf.end() == false; ++Vf)
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) == 0)
	 break;
   if (Vf.end() == true)
      Vf = V.FileList();
   return Recs.Lookup(Vf);
}
									/*}}}*/
static APT_PURE char const *skipDescription(char const *DescP, size_t const Length, bool fields) /*{{{*/
{
   auto const backup = DescP;
   char const * const TagName = "\nDescription";
   size_t const TagLen = strlen(TagName);
   while ((DescP = static_cast<char const *>(memchr(DescP, '\n', Length - (DescP - backup)))) != nullptr)
   {
      if (DescP[1] == ' ')
	 DescP += 2;
      else if (fields && strncmp((char *)DescP, TagName, TagLen) == 0)
	 DescP += TagLen;
      else
	 break;
   }
   if (DescP != NULL)
      ++DescP;
   return DescP;
}
									/*}}}*/
static APT_PURE char const *findDescriptionField(char const *DescP, size_t const Length) /*{{{*/
{
   auto const backup = DescP;
   char const * const TagName = "\nDescription";
   size_t const TagLen = strlen(TagName);
   while ((DescP = static_cast<char const *>(memchr(DescP, '\n', Length - (DescP - backup)))) != nullptr)
   {
      if (strncmp(DescP, TagName, TagLen) == 0)
	 break;
      else
	 ++DescP;
   }
   if (DescP != nullptr)
      ++DescP;
   return DescP;
}
									/*}}}*/
static APT_PURE char const *skipColonSpaces(char const *Buffer, size_t const Length) /*{{{*/
{
   // skipping withspace before and after the field-value separating colon
   char const *const Start = Buffer;
   for (; isspace(*Buffer) != 0 && Length - (Buffer - Start) > 0; ++Buffer)
      ;
   if (*Buffer != ':')
      return nullptr;
   ++Buffer;
   for (; isspace(*Buffer) != 0 && Length - (Buffer - Start) > 0; ++Buffer)
      ;
   if (Length < static_cast<size_t>(Buffer - Start))
      return nullptr;
   return Buffer;
}
									/*}}}*/

bool DisplayRecordV1(pkgCacheFile &, pkgRecords &Recs, /*{{{*/
		     pkgCache::VerIterator const &V, pkgCache::VerFileIterator const &Vf,
		     char const *Buffer, size_t Length, std::ostream &out)
{
   if (unlikely(Length < 4))
      return false;

   auto const Desc = V.TranslatedDescription();
   if (Desc.end())
   {
      /* This handles the unusual case that we have no description whatsoever.
	 The slightly more common case of only having a short-description embedded
	 in the record could be handled here, but apt supports also having multiple
	 descriptions embedded in the record, so we deal with that case later */
      if (FileFd::Write(STDOUT_FILENO, Buffer, Length) == false)
	 return false;
      if (strncmp((Buffer + Length - 4), "\r\n\r\n", 4) != 0 &&
	  strncmp((Buffer + Length - 2), "\n\n", 2) != 0)
	 out << std::endl;
      return true;
   }

   // Get a pointer to start of Description field
   char const *DescP = findDescriptionField(Buffer, Length);
   if (DescP == nullptr)
      DescP = Buffer + Length;

   // Write all but Description
   size_t const untilDesc = DescP - Buffer;
   if (untilDesc != 0 && FileFd::Write(STDOUT_FILENO, Buffer, untilDesc) == false)
      return false;

   // Show the right description
   char desctag[50];
   auto const langcode = Desc.LanguageCode();
   if (strcmp(langcode, "") == 0)
      strcpy(desctag, "\nDescription");
   else
      snprintf(desctag, sizeof(desctag), "\nDescription-%s", langcode);

   out << desctag + 1 << ": " << std::flush;
   auto const Df = Desc.FileList();
   if (Df.end() == false)
   {
      if (Desc.FileList()->File == Vf->File)
      {
	 /* If we have the file already open look in the buffer for the
	    description we want to display. Note that this might not be the
	    only one we can encounter in this record */
	 char const *Start = DescP;
	 do
	 {
	    if (strncmp(Start, desctag + 1, strlen(desctag) - 1) != 0)
	       continue;
	    Start += strlen(desctag) - 1;
	    Start = skipColonSpaces(Start, Length - (Start - Buffer));
	    if (Start == nullptr)
	       continue;
	    char const *End = skipDescription(Start, Length - (Start - Buffer), false);
	    if (likely(End != nullptr))
	       FileFd::Write(STDOUT_FILENO, Start, End - (Start + 1));
	    break;
	 } while ((Start = findDescriptionField(Start, Length - (Start - Buffer))) != nullptr);
      }
      else
      {
	 pkgRecords::Parser &P = Recs.Lookup(Df);
	 out << P.LongDesc();
      }
   }

   out << std::endl << "Description-md5: " << Desc.md5() << std::endl;

   // Find the first field after the description (if there is any)
   DescP = skipDescription(DescP, Length - (DescP - Buffer), true);

   // write the rest of the buffer, but skip mixed in Descriptions* fields
   while (DescP != nullptr)
   {
      char const *const Start = DescP;
      char const *End = findDescriptionField(DescP, Length - (DescP - Buffer));
      if (End == nullptr)
      {
	 DescP = nullptr;
	 End = Buffer + Length - 1;
	 size_t endings = 0;
	 while (*End == '\n')
	 {
	    --End;
	    if (*End == '\r')
	       --End;
	    ++endings;
	 }
	 if (endings >= 1)
	 {
	    ++End;
	    if (*End == '\r')
	       ++End;
	 }
	 ++End;
      }
      else
	 DescP = skipDescription(End + strlen("Description"), Length - (End - Buffer), true);

      size_t const length = End - Start;
      if (length != 0 && FileFd::Write(STDOUT_FILENO, Start, length) == false)
	 return false;
   }
   // write a final newline after the last field
   out << std::endl;

   return true;
}
									/*}}}*/
static bool DisplayRecordV2(pkgCacheFile &CacheFile, pkgRecords &Recs, /*{{{*/
			    pkgCache::VerIterator const &V, pkgCache::VerFileIterator const &Vf,
			    char const *Buffer, size_t const Length, std::ostream &out)
{
   // Check and load the package list file
   pkgCache::PkgFileIterator I = Vf.File();

   // find matching sources.list metaindex
   pkgSourceList *SrcList = CacheFile.GetSourceList();
   pkgIndexFile *Index;
   if (SrcList->FindIndex(I, Index) == false &&
       _system->FindIndex(I, Index) == false)
      return _error->Error("Can not find indexfile for Package %s (%s)",
			   V.ParentPkg().Name(), V.VerStr());
   std::string source_index_file = Index->Describe(true);

   // Read the record
   pkgTagSection Tags;
   if (Tags.Scan(Buffer, Length, true) == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // make size nice
   std::string installed_size;
   if (Tags.FindULL("Installed-Size") > 0)
      strprintf(installed_size, "%sB", SizeToStr(Tags.FindULL("Installed-Size") * 1024).c_str());
   else
      installed_size = _("unknown");
   std::string package_size;
   if (Tags.FindULL("Size") > 0)
      strprintf(package_size, "%sB", SizeToStr(Tags.FindULL("Size")).c_str());
   else
      package_size = _("unknown");

   const char *manual_installed = nullptr;
   if (V.ParentPkg().CurrentVer() == V)
   {
      pkgDepCache *depCache = CacheFile.GetDepCache();
      if (unlikely(depCache == nullptr))
	 return false;
      pkgDepCache::StateCache &state = (*depCache)[V.ParentPkg()];
      manual_installed = !(state.Flags & pkgCache::Flag::Auto) ? "yes" : "no";
   }

   std::vector<pkgTagSection::Tag> RW;
   // delete, apt-cache show has this info and most users do not care
   if (not _config->FindB("APT::Cache::ShowFull", false))
   {
      RW.push_back(pkgTagSection::Tag::Remove("MD5sum"));
      RW.push_back(pkgTagSection::Tag::Remove("SHA1"));
      RW.push_back(pkgTagSection::Tag::Remove("SHA256"));
      RW.push_back(pkgTagSection::Tag::Remove("SHA512"));
      RW.push_back(pkgTagSection::Tag::Remove("Filename"));
      RW.push_back(pkgTagSection::Tag::Remove("Multi-Arch"));
      RW.push_back(pkgTagSection::Tag::Remove("Conffiles"));
   }
   RW.push_back(pkgTagSection::Tag::Remove("Architecture"));
   // we use the translated description
   RW.push_back(pkgTagSection::Tag::Remove("Description"));
   RW.push_back(pkgTagSection::Tag::Remove("Description-md5"));
   // improve
   RW.push_back(pkgTagSection::Tag::Rewrite("Package", V.ParentPkg().FullName(true)));
   RW.push_back(pkgTagSection::Tag::Rewrite("Installed-Size", installed_size));
   RW.push_back(pkgTagSection::Tag::Remove("Size"));
   RW.push_back(pkgTagSection::Tag::Rewrite("Download-Size", package_size));
   // add
   if (manual_installed != nullptr)
      RW.push_back(pkgTagSection::Tag::Rewrite("APT-Manual-Installed", manual_installed));
   RW.push_back(pkgTagSection::Tag::Rewrite("APT-Sources", source_index_file));

   FileFd stdoutfd;
   if (stdoutfd.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly, false) == false ||
	 Tags.Write(stdoutfd, TFRewritePackageOrder, RW) == false || stdoutfd.Close() == false)
      return _error->Error("Internal Error, Unable to parse a package record");

   // write the description
   // FIXME: show (optionally) all available translations(?)
   pkgCache::DescIterator Desc = V.TranslatedDescription();
   if (Desc.end() == false)
   {
      pkgRecords::Parser &P = Recs.Lookup(Desc.FileList());
      out << "Description: " << P.LongDesc();
   }

   // write a final newline (after the description)
   out << std::endl << std::endl;

   return true;
}
									/*}}}*/
bool ShowPackage(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   CacheFile.InhibitActionGroups(true);
   auto VolatileCmdL = GetAllPackagesAsPseudo(CacheFile.GetSourceList(), CmdL, AddVolatileBinaryFile, "");

   if (unlikely(CacheFile.GetPkgCache() == nullptr))
      return false;
   CacheSetHelperVirtuals helper(true, GlobalError::NOTICE);
   APT::CacheSetHelper::VerSelector const select = _config->FindB("APT::Cache::AllVersions", true) ?
			APT::CacheSetHelper::ALL : APT::CacheSetHelper::CANDIDATE;
   if (select == APT::CacheSetHelper::CANDIDATE && CacheFile.GetDepCache() == nullptr)
      return false;

   APT::VersionList verset;
   size_t normalPackages = 0;
   for (auto const &I: VolatileCmdL)
   {
      if (I.index == -1)
      {
	 APT::VersionContainerInterface::FromString(&verset, CacheFile, I.name, select, helper);
	 ++normalPackages;
      }
      else
      {
	 if (select != APT::CacheSetHelper::CANDIDATE && unlikely(CacheFile.GetDepCache() == nullptr))
	    return false;
	 pkgCache::PkgIterator const P = CacheFile->FindPkg(I.name);
	 if (unlikely(P.end()))
	    continue;

	 // Set any version providing the .deb as the candidate.
	 for (auto Prv = P.ProvidesList(); Prv.end() == false; ++Prv)
	 {
	    if (I.release.empty())
	       CacheFile->SetCandidateVersion(Prv.OwnerVer());
	    else
	       CacheFile->SetCandidateRelease(Prv.OwnerVer(), I.release);

	    // via cacheset to have our usual handling
	    APT::VersionContainerInterface::FromPackage(&verset, CacheFile, Prv.OwnerPkg(), APT::CacheSetHelper::CANDIDATE, helper);
	 }
      }
   }

   int const ShowVersion = _config->FindI("APT::Cache::Show::Version", 1);
   pkgRecords Recs(CacheFile);
   for (APT::VersionList::const_iterator Ver = verset.begin(); Ver != verset.end(); ++Ver)
   {
      pkgCache::VerFileIterator Vf;
      auto &Parser = LookupParser(Recs, Ver, Vf);
      char const *Start, *Stop;
      Parser.GetRec(Start, Stop);
      size_t const Length = Stop - Start;

      if (ShowVersion <= 1)
      {
	 if (DisplayRecordV1(CacheFile, Recs, Ver, Vf, Start, Length, std::cout) == false)
	    return false;
      }
      else if (DisplayRecordV2(CacheFile, Recs, Ver, Vf, Start, Length + 1, c1out) == false)
	 return false;
   }

   if (select == APT::CacheSetHelper::CANDIDATE && normalPackages != 0)
   {
      APT::VersionList verset_all;
      for (auto const &I: VolatileCmdL)
      {
	 if (I.index == -1)
	    APT::VersionContainerInterface::FromString(&verset_all, CacheFile, I.name, APT::CacheSetHelper::ALL, helper);
	 else
	 {
	    pkgCache::PkgIterator const P = CacheFile->FindPkg(I.name);
	    if (unlikely(P.end()))
	       continue;

	    // Set any version providing the .deb as the candidate.
	    for (auto Prv = P.ProvidesList(); Prv.end() == false; ++Prv)
	    {
	       if (I.release.empty())
		  CacheFile->SetCandidateVersion(Prv.OwnerVer());
	       else
		  CacheFile->SetCandidateRelease(Prv.OwnerVer(), I.release);

	       // via cacheset to have our usual virtual handling
	       APT::VersionContainerInterface::FromPackage(&verset_all, CacheFile, Prv.OwnerPkg(), APT::CacheSetHelper::CANDIDATE, helper);
	    }
	 }
      }

      int const records = verset_all.size() - verset.size();
      if (records > 0)
         _error->Notice(P_("There is %i additional record. Please use the '-a' switch to see it", "There are %i additional records. Please use the '-a' switch to see them.", records), records);
   }

   if (_config->FindB("APT::Cache::ShowVirtuals", false) == true)
      for (APT::PackageSet::const_iterator Pkg = helper.virtualPkgs.begin();
	    Pkg != helper.virtualPkgs.end(); ++Pkg)
      {
	 c1out << "Package: " << Pkg.FullName(true) << std::endl;
	 c1out << "State: " << _("not a real package (virtual)") << std::endl;
	 // FIXME: show providers, see private-cacheset.h
	 //        CacheSetHelperAPTGet::showVirtualPackageErrors()
      }

   if (verset.empty() == true)
   {
      if (helper.virtualPkgs.empty() == true)
        return _error->Error(_("No packages found"));
      else
        _error->Notice(_("No packages found"));
   }

   return true;
}
									/*}}}*/
static std::string Sha1FromString(std::string const &input)		/*{{{*/
{
   // XXX: move to hashes.h: HashString::FromString() ?
   Hashes sha1(Hashes::SHA1SUM);
   sha1.Add(input.c_str(), input.length());
   return sha1.GetHashString(Hashes::SHA1SUM).HashValue();
}
									/*}}}*/
bool ShowSrcPackage(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   pkgSourceList *List = CacheFile.GetSourceList();
   if (unlikely(List == NULL))
      return false;

   // Create the text record parsers
   pkgSrcRecords SrcRecs(*List);
   if (_error->PendingError() == true)
      return false;

   bool found = false;
   // avoid showing identical records
   std::set<std::string> seen;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      SrcRecs.Restart();

      pkgSrcRecords::Parser *Parse;
      bool found_this = false;
      while ((Parse = SrcRecs.Find(*I,false)) != 0) {
	 // SrcRecs.Find() will find both binary and source names
	 if (_config->FindB("APT::Cache::Only-Source", false) == true)
	    if (Parse->Package() != *I)
	       continue;
         std::string sha1str = Sha1FromString(Parse->AsStr());
         if (std::find(seen.begin(), seen.end(), sha1str) == seen.end())
         {
            std::cout << Parse->AsStr() << std::endl;;
            found = true;
            found_this = true;
            seen.insert(sha1str);
         }
      }
      if (found_this == false) {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
   }
   if (found == false)
      _error->Notice(_("No packages found"));
   return true;
}
									/*}}}*/
// Policy - Show the results of the preferences file			/*{{{*/
bool Policy(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgSourceList const * const SrcList = CacheFile.GetSourceList();
   if (unlikely(SrcList == nullptr))
      return false;
   pkgCache * const Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == nullptr))
      return false;
   pkgPolicy * const Plcy = CacheFile.GetPolicy();
   if (unlikely(Plcy == nullptr))
      return false;

   // Print out all of the package files
   if (CmdL.FileList[1] == 0)
   {
      std::cout << _("Package files:") << std::endl;
      for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F.end() == false; ++F)
      {
	 if (F.Flagged(pkgCache::Flag::NoPackages))
	    continue;
	 // Locate the associated index files so we can derive a description
	 pkgIndexFile *Indx;
	 if (SrcList->FindIndex(F,Indx) == false &&
	       _system->FindIndex(F,Indx) == false)
	    return _error->Error(_("Cache is out of sync, can't x-ref a package file"));

	 printf("%4i %s\n",
	       Plcy->GetPriority(F),Indx->Describe(true).c_str());

	 // Print the reference information for the package
	 std::string Str = F.RelStr();
	 if (Str.empty() == false)
	    printf("     release %s\n",F.RelStr().c_str());
	 if (F.Site() != 0 && F.Site()[0] != 0)
	    printf("     origin %s\n",F.Site());
      }

      // Show any packages have explicit pins
      std::cout << _("Pinned packages:") << std::endl;
      pkgCache::PkgIterator I = Cache->PkgBegin();
      for (;I.end() != true; ++I)
      {
	 for (pkgCache::VerIterator V = I.VersionList(); !V.end(); ++V) {
	    auto Prio = Plcy->GetPriority(V, false);
	    if (Prio == 0)
	       continue;

	    std::cout << "     ";
	    // Print the package name and the version we are forcing to
	    ioprintf(std::cout, _("%s -> %s with priority %d\n"), I.FullName(true).c_str(), V.VerStr(), Prio);
	 }
      }
      return true;
   }

   char const * const msgInstalled = _("  Installed: ");
   char const * const msgCandidate = _("  Candidate: ");
   short const InstalledLessCandidate =
      mbstowcs(NULL, msgInstalled, 0) - mbstowcs(NULL, msgCandidate, 0);
   short const deepInstalled =
      (InstalledLessCandidate < 0 ? (InstalledLessCandidate*-1) : 0) - 1;
   short const deepCandidate =
      (InstalledLessCandidate > 0 ? (InstalledLessCandidate) : 0) - 1;

   // Print out detailed information for each package
   APT::CacheSetHelper helper(true, GlobalError::NOTICE);
   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1, helper);
   for (APT::PackageList::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      std::cout << Pkg.FullName(true) << ":" << std::endl;

      // Installed version
      std::cout << msgInstalled << OutputInDepth(deepInstalled, " ");
      if (Pkg->CurrentVer == 0)
	 std::cout << _("(none)") << std::endl;
      else
	 std::cout << Pkg.CurrentVer().VerStr() << std::endl;

      // Candidate Version 
      std::cout << msgCandidate << OutputInDepth(deepCandidate, " ");
      pkgCache::VerIterator V = Plcy->GetCandidateVer(Pkg);
      if (V.end() == true)
	 std::cout << _("(none)") << std::endl;
      else
	 std::cout << V.VerStr() << std::endl;

      // Show the priority tables
      std::cout << _("  Version table:") << std::endl;
      for (V = Pkg.VersionList(); V.end() == false; ++V)
      {
	 if (Pkg.CurrentVer() == V)
	    std::cout << " *** " << V.VerStr();
	 else
	    std::cout << "     " << V.VerStr();

	 std::cout << " " << Plcy->GetPriority(V);

	 if (V.PhasedUpdatePercentage() != 100)
	    std::cout << " "
		      << "(" << _("phased") << " " << V.PhasedUpdatePercentage() << "%)";

	 std::cout << std::endl;
	 for (pkgCache::VerFileIterator VF = V.FileList(); VF.end() == false; ++VF)
	 {
	    // Locate the associated index files so we can derive a description
	    pkgIndexFile *Indx;
	    if (SrcList->FindIndex(VF.File(),Indx) == false &&
		  _system->FindIndex(VF.File(),Indx) == false)
	       return _error->Error(_("Cache is out of sync, can't x-ref a package file"));
	    printf("       %4i %s\n",Plcy->GetPriority(VF.File()),
		  Indx->Describe(true).c_str());
	 }
      }
   }
   return true;
}
									/*}}}*/

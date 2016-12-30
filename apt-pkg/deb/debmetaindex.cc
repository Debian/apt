#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/macros.h>

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <sstream>

#include <sys/stat.h>
#include <string.h>

#include <apti18n.h>

class APT_HIDDEN debReleaseIndexPrivate					/*{{{*/
{
   public:
   struct APT_HIDDEN debSectionEntry
   {
      std::string sourcesEntry;
      std::string Name;
      std::vector<std::string> Targets;
      std::vector<std::string> Architectures;
      std::vector<std::string> Languages;
      bool UsePDiffs;
      std::string UseByHash;
   };

   std::vector<debSectionEntry> DebEntries;
   std::vector<debSectionEntry> DebSrcEntries;

   metaIndex::TriState CheckValidUntil;
   time_t ValidUntilMin;
   time_t ValidUntilMax;

   std::vector<std::string> Architectures;
   std::vector<std::string> NoSupportForAll;

   debReleaseIndexPrivate() : CheckValidUntil(metaIndex::TRI_UNSET), ValidUntilMin(0), ValidUntilMax(0) {}
};
									/*}}}*/
// ReleaseIndex::MetaIndex* - display helpers				/*{{{*/
std::string debReleaseIndex::MetaIndexInfo(const char *Type) const
{
   std::string Info = ::URI::ArchiveOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Info += Dist;
   }
   else
      Info += Dist;
   Info += " ";
   Info += Type;
   return Info;
}
std::string debReleaseIndex::Describe() const
{
   return MetaIndexInfo("Release");
}

std::string debReleaseIndex::MetaIndexFile(const char *Type) const
{
   return _config->FindDir("Dir::State::lists") +
      URItoFileName(MetaIndexURI(Type));
}

std::string debReleaseIndex::MetaIndexURI(const char *Type) const
{
   std::string Res;

   if (Dist == "/")
      Res = URI;
   else if (Dist[Dist.size()-1] == '/')
      Res = URI + Dist;
   else
      Res = URI + "dists/" + Dist + "/";
   
   Res += Type;
   return Res;
}
									/*}}}*/
// ReleaseIndex Con- and Destructors					/*{{{*/
debReleaseIndex::debReleaseIndex(std::string const &URI, std::string const &Dist) :
					metaIndex(URI, Dist, "deb"), d(new debReleaseIndexPrivate())
{}
debReleaseIndex::debReleaseIndex(std::string const &URI, std::string const &Dist, bool const pTrusted) :
					metaIndex(URI, Dist, "deb"), d(new debReleaseIndexPrivate())
{
   Trusted = pTrusted ? TRI_YES : TRI_NO;
}
debReleaseIndex::~debReleaseIndex() {
   if (d != NULL)
      delete d;
}
									/*}}}*/
// ReleaseIndex::GetIndexTargets					/*{{{*/
static void GetIndexTargetsFor(char const * const Type, std::string const &URI, std::string const &Dist,
      std::vector<debReleaseIndexPrivate::debSectionEntry> const &entries,
      std::vector<IndexTarget> &IndexTargets)
{
   bool const flatArchive = (Dist[Dist.length() - 1] == '/');
   std::string baseURI = URI;
   if (flatArchive)
   {
      if (Dist != "/")
         baseURI += Dist;
   }
   else
      baseURI += "dists/" + Dist + "/";
   std::string const Release = (Dist == "/") ? "" : Dist;
   std::string const Site = ::URI::ArchiveOnly(URI);

   std::string DefCompressionTypes;
   {
      std::vector<std::string> types = APT::Configuration::getCompressionTypes();
      if (types.empty() == false)
      {
	 std::ostringstream os;
	 std::copy(types.begin(), types.end()-1, std::ostream_iterator<std::string>(os, " "));
	 os << *types.rbegin();
	 DefCompressionTypes = os.str();
      }
   }
   std::string DefKeepCompressedAs;
   {
      std::vector<APT::Configuration::Compressor> comps = APT::Configuration::getCompressors();
      if (comps.empty() == false)
      {
	 std::sort(comps.begin(), comps.end(),
	       [](APT::Configuration::Compressor const &a, APT::Configuration::Compressor const &b) { return a.Cost < b.Cost; });
	 std::ostringstream os;
	 for (auto const &c : comps)
	    if (c.Cost != 0)
	       os << c.Extension.substr(1) << ' ';
	 DefKeepCompressedAs = os.str();
      }
      DefKeepCompressedAs += "uncompressed";
   }
   std::string const NativeArch = _config->Find("APT::Architecture");
   bool const GzipIndex = _config->FindB("Acquire::GzipIndexes", false);
   for (std::vector<debReleaseIndexPrivate::debSectionEntry>::const_iterator E = entries.begin(); E != entries.end(); ++E)
   {
      for (std::vector<std::string>::const_iterator T = E->Targets.begin(); T != E->Targets.end(); ++T)
      {
#define APT_T_CONFIG_STR(X, Y) _config->Find(std::string("Acquire::IndexTargets::") + Type  + "::" + *T + "::" + (X), (Y))
#define APT_T_CONFIG_BOOL(X, Y) _config->FindB(std::string("Acquire::IndexTargets::") + Type  + "::" + *T + "::" + (X), (Y))
	 std::string const tplMetaKey = APT_T_CONFIG_STR(flatArchive ? "flatMetaKey" : "MetaKey", "");
	 std::string const tplShortDesc = APT_T_CONFIG_STR("ShortDescription", "");
	 std::string const tplLongDesc = "$(SITE) " + APT_T_CONFIG_STR(flatArchive ? "flatDescription" : "Description", "");
	 bool const IsOptional = APT_T_CONFIG_BOOL("Optional", true);
	 bool const KeepCompressed = APT_T_CONFIG_BOOL("KeepCompressed", GzipIndex);
	 bool const DefaultEnabled = APT_T_CONFIG_BOOL("DefaultEnabled", true);
	 bool const UsePDiffs = APT_T_CONFIG_BOOL("PDiffs", E->UsePDiffs);
	 std::string const UseByHash = APT_T_CONFIG_STR("By-Hash", E->UseByHash);
	 std::string const CompressionTypes = APT_T_CONFIG_STR("CompressionTypes", DefCompressionTypes);
	 std::string KeepCompressedAs = APT_T_CONFIG_STR("KeepCompressedAs", "");
#undef APT_T_CONFIG_BOOL
#undef APT_T_CONFIG_STR
	 if (tplMetaKey.empty())
	    continue;

	 if (KeepCompressedAs.empty())
	    KeepCompressedAs = DefKeepCompressedAs;
	 else
	 {
	    std::vector<std::string> const defKeep = VectorizeString(DefKeepCompressedAs, ' ');
	    std::vector<std::string> const valKeep = VectorizeString(KeepCompressedAs, ' ');
	    std::vector<std::string> keep;
	    for (auto const &val : valKeep)
	    {
	       if (val.empty())
		  continue;
	       if (std::find(defKeep.begin(), defKeep.end(), val) == defKeep.end())
		  continue;
	       keep.push_back(val);
	    }
	    if (std::find(keep.begin(), keep.end(), "uncompressed") == keep.end())
	       keep.push_back("uncompressed");
	    std::ostringstream os;
	    std::copy(keep.begin(), keep.end()-1, std::ostream_iterator<std::string>(os, " "));
	    os << *keep.rbegin();
	    KeepCompressedAs = os.str();
	 }

	 for (std::vector<std::string>::const_iterator L = E->Languages.begin(); L != E->Languages.end(); ++L)
	 {
	    if (*L == "none" && tplMetaKey.find("$(LANGUAGE)") != std::string::npos)
	       continue;

	    for (std::vector<std::string>::const_iterator A = E->Architectures.begin(); A != E->Architectures.end(); ++A)
	    {
	       // available in templates
	       std::map<std::string, std::string> Options;
	       Options.insert(std::make_pair("SITE", Site));
	       Options.insert(std::make_pair("RELEASE", Release));
	       if (tplMetaKey.find("$(COMPONENT)") != std::string::npos)
		  Options.insert(std::make_pair("COMPONENT", E->Name));
	       if (tplMetaKey.find("$(LANGUAGE)") != std::string::npos)
		  Options.insert(std::make_pair("LANGUAGE", *L));
	       if (tplMetaKey.find("$(ARCHITECTURE)") != std::string::npos)
		  Options.insert(std::make_pair("ARCHITECTURE", *A));
	       else if (tplMetaKey.find("$(NATIVE_ARCHITECTURE)") != std::string::npos)
		  Options.insert(std::make_pair("ARCHITECTURE", NativeArch));
	       if (tplMetaKey.find("$(NATIVE_ARCHITECTURE)") != std::string::npos)
		  Options.insert(std::make_pair("NATIVE_ARCHITECTURE", NativeArch));

	       std::string MetaKey = tplMetaKey;
	       std::string ShortDesc = tplShortDesc;
	       std::string LongDesc = tplLongDesc;
	       for (std::map<std::string, std::string>::const_iterator O = Options.begin(); O != Options.end(); ++O)
	       {
		  MetaKey = SubstVar(MetaKey, std::string("$(") + O->first + ")", O->second);
		  ShortDesc = SubstVar(ShortDesc, std::string("$(") + O->first + ")", O->second);
		  LongDesc = SubstVar(LongDesc, std::string("$(") + O->first + ")", O->second);
	       }

	       {
		  auto const dup = std::find_if(IndexTargets.begin(), IndexTargets.end(), [&](IndexTarget const &IT) {
		     return MetaKey == IT.MetaKey && baseURI == IT.Option(IndexTarget::BASE_URI) &&
			E->sourcesEntry == IT.Option(IndexTarget::SOURCESENTRY) && *T == IT.Option(IndexTarget::CREATED_BY);
		  });
		  if (dup != IndexTargets.end())
		  {
		     if (tplMetaKey.find("$(ARCHITECTURE)") == std::string::npos)
			break;
		     continue;
		  }
	       }

	       {
		  auto const dup = std::find_if(IndexTargets.begin(), IndexTargets.end(), [&](IndexTarget const &IT) {
		     return MetaKey == IT.MetaKey && baseURI == IT.Option(IndexTarget::BASE_URI) &&
			E->sourcesEntry == IT.Option(IndexTarget::SOURCESENTRY) && *T != IT.Option(IndexTarget::CREATED_BY);
		  });
		  if (dup != IndexTargets.end())
		  {
		     std::string const dupT = dup->Option(IndexTarget::CREATED_BY);
		     std::string const dupEntry = dup->Option(IndexTarget::SOURCESENTRY);
		     //TRANSLATOR: an identifier like Packages; Releasefile key indicating
		     // a file like main/binary-amd64/Packages; another identifier like Contents;
		     // filename and linenumber of the sources.list entry currently parsed
		     _error->Warning(_("Target %s wants to acquire the same file (%s) as %s from source %s"),
			   T->c_str(), MetaKey.c_str(), dupT.c_str(), dupEntry.c_str());
		     if (tplMetaKey.find("$(ARCHITECTURE)") == std::string::npos)
			break;
		     continue;
		  }
	       }

	       {
		  auto const dup = std::find_if(IndexTargets.begin(), IndexTargets.end(), [&](IndexTarget const &T) {
		     return MetaKey == T.MetaKey && baseURI == T.Option(IndexTarget::BASE_URI) &&
			E->sourcesEntry != T.Option(IndexTarget::SOURCESENTRY);
		  });
		  if (dup != IndexTargets.end())
		  {
		     std::string const dupEntry = dup->Option(IndexTarget::SOURCESENTRY);
		     //TRANSLATOR: an identifier like Packages; Releasefile key indicating
		     // a file like main/binary-amd64/Packages; filename and linenumber of
		     // two sources.list entries
		     _error->Warning(_("Target %s (%s) is configured multiple times in %s and %s"),
			   T->c_str(), MetaKey.c_str(), dupEntry.c_str(), E->sourcesEntry.c_str());
		     if (tplMetaKey.find("$(ARCHITECTURE)") == std::string::npos)
			break;
		     continue;
		  }
	       }

	       // not available in templates, but in the indextarget
	       Options.insert(std::make_pair("BASE_URI", baseURI));
	       Options.insert(std::make_pair("REPO_URI", URI));
	       Options.insert(std::make_pair("TARGET_OF", Type));
	       Options.insert(std::make_pair("CREATED_BY", *T));
	       Options.insert(std::make_pair("PDIFFS", UsePDiffs ? "yes" : "no"));
	       Options.insert(std::make_pair("BY_HASH", UseByHash));
	       Options.insert(std::make_pair("DEFAULTENABLED", DefaultEnabled ? "yes" : "no"));
	       Options.insert(std::make_pair("COMPRESSIONTYPES", CompressionTypes));
	       Options.insert(std::make_pair("KEEPCOMPRESSEDAS", KeepCompressedAs));
	       Options.insert(std::make_pair("SOURCESENTRY", E->sourcesEntry));

	       bool IsOpt = IsOptional;
	       if (IsOpt == false)
	       {
		  auto const arch = Options.find("ARCHITECTURE");
		  if (arch != Options.end() && arch->second == "all")
		     IsOpt = true;
	       }

	       IndexTarget Target(
		     MetaKey,
		     ShortDesc,
		     LongDesc,
		     Options.find("BASE_URI")->second + MetaKey,
		     IsOpt,
		     KeepCompressed,
		     Options
		     );
	       IndexTargets.push_back(Target);

	       if (tplMetaKey.find("$(ARCHITECTURE)") == std::string::npos)
		  break;

	    }

	    if (tplMetaKey.find("$(LANGUAGE)") == std::string::npos)
	       break;

	 }

      }
   }
}
std::vector<IndexTarget> debReleaseIndex::GetIndexTargets() const
{
   std::vector<IndexTarget> IndexTargets;
   GetIndexTargetsFor("deb-src", URI, Dist, d->DebSrcEntries, IndexTargets);
   GetIndexTargetsFor("deb", URI, Dist, d->DebEntries, IndexTargets);
   return IndexTargets;
}
									/*}}}*/
void debReleaseIndex::AddComponent(std::string const &sourcesEntry,	/*{{{*/
	 bool const isSrc, std::string const &Name,
	 std::vector<std::string> const &Targets,
	 std::vector<std::string> const &Architectures,
	 std::vector<std::string> Languages,
	 bool const usePDiffs, std::string const &useByHash)
{
   if (Languages.empty() == true)
      Languages.push_back("none");
   debReleaseIndexPrivate::debSectionEntry const entry = {
      sourcesEntry, Name, Targets, Architectures, Languages, usePDiffs, useByHash
   };
   if (isSrc)
      d->DebSrcEntries.push_back(entry);
   else
      d->DebEntries.push_back(entry);
}
									/*}}}*/

bool debReleaseIndex::Load(std::string const &Filename, std::string * const ErrorText)/*{{{*/
{
   LoadedSuccessfully = TRI_NO;
   FileFd Fd;
   if (OpenMaybeClearSignedFile(Filename, Fd) == false)
      return false;

   pkgTagFile TagFile(&Fd, Fd.Size());
   if (Fd.IsOpen() == false || Fd.Failed())
   {
      if (ErrorText != NULL)
	 strprintf(*ErrorText, _("Unable to parse Release file %s"),Filename.c_str());
      return false;
   }

   pkgTagSection Section;
   const char *Start, *End;
   if (TagFile.Step(Section) == false)
   {
      if (ErrorText != NULL)
	 strprintf(*ErrorText, _("No sections in Release file %s"), Filename.c_str());
      return false;
   }
   // FIXME: find better tag name
   SupportsAcquireByHash = Section.FindB("Acquire-By-Hash", false);

   Suite = Section.FindS("Suite");
   Codename = Section.FindS("Codename");
   {
      std::string const archs = Section.FindS("Architectures");
      if (archs.empty() == false)
	 d->Architectures = VectorizeString(archs, ' ');
   }
   {
      std::string const targets = Section.FindS("No-Support-for-Architecture-all");
      if (targets.empty() == false)
	 d->NoSupportForAll = VectorizeString(targets, ' ');
   }

   bool FoundHashSum = false;
   bool FoundStrongHashSum = false;
   auto const SupportedHashes = HashString::SupportedHashes();
   for (int i=0; SupportedHashes[i] != NULL; i++)
   {
      if (!Section.Find(SupportedHashes[i], Start, End))
	 continue;

      std::string Name;
      std::string Hash;
      unsigned long long Size;
      while (Start < End)
      {
	 if (!parseSumData(Start, End, Name, Hash, Size))
	    return false;

	 HashString const hs(SupportedHashes[i], Hash);
         if (Entries.find(Name) == Entries.end())
         {
            metaIndex::checkSum *Sum = new metaIndex::checkSum;
            Sum->MetaKeyFilename = Name;
            Sum->Size = Size;
	    Sum->Hashes.FileSize(Size);
            APT_IGNORE_DEPRECATED(Sum->Hash = hs;)
            Entries[Name] = Sum;
         }
         Entries[Name]->Hashes.push_back(hs);
         FoundHashSum = true;
	 if (FoundStrongHashSum == false && hs.usable() == true)
	    FoundStrongHashSum = true;
      }
   }

   if(FoundHashSum == false)
   {
      if (ErrorText != NULL)
	 strprintf(*ErrorText, _("No Hash entry in Release file %s"), Filename.c_str());
      return false;
   }
   if(FoundStrongHashSum == false)
   {
      if (ErrorText != NULL)
	 strprintf(*ErrorText, _("No Hash entry in Release file %s which is considered strong enough for security purposes"), Filename.c_str());
      return false;
   }

   std::string const StrDate = Section.FindS("Date");
   if (RFC1123StrToTime(StrDate.c_str(), Date) == false)
   {
      _error->Warning( _("Invalid 'Date' entry in Release file %s"), Filename.c_str());
      Date = 0;
   }

   bool CheckValidUntil = _config->FindB("Acquire::Check-Valid-Until", true);
   if (d->CheckValidUntil == metaIndex::TRI_NO)
      CheckValidUntil = false;
   else if (d->CheckValidUntil == metaIndex::TRI_YES)
      CheckValidUntil = true;

   if (CheckValidUntil == true)
   {
      std::string const Label = Section.FindS("Label");
      std::string const StrValidUntil = Section.FindS("Valid-Until");

      // if we have a Valid-Until header in the Release file, use it as default
      if (StrValidUntil.empty() == false)
      {
	 if(RFC1123StrToTime(StrValidUntil.c_str(), ValidUntil) == false)
	 {
	    if (ErrorText != NULL)
	       strprintf(*ErrorText, _("Invalid 'Valid-Until' entry in Release file %s"), Filename.c_str());
	    return false;
	 }
      }
      // get the user settings for this archive and use what expires earlier
      time_t MaxAge = d->ValidUntilMax;
      if (MaxAge == 0)
      {
	 MaxAge = _config->FindI("Acquire::Max-ValidTime", 0);
	 if (Label.empty() == false)
	    MaxAge = _config->FindI(("Acquire::Max-ValidTime::" + Label).c_str(), MaxAge);
      }
      time_t MinAge = d->ValidUntilMin;
      if (MinAge == 0)
      {
	 MinAge = _config->FindI("Acquire::Min-ValidTime", 0);
	 if (Label.empty() == false)
	    MinAge = _config->FindI(("Acquire::Min-ValidTime::" + Label).c_str(), MinAge);
      }

      if (MinAge != 0 || ValidUntil != 0 || MaxAge != 0)
      {
	 if (MinAge != 0 && ValidUntil != 0) {
	    time_t const min_date = Date + MinAge;
	    if (ValidUntil < min_date)
	       ValidUntil = min_date;
	 }
	 if (MaxAge != 0 && Date != 0) {
	    time_t const max_date = Date + MaxAge;
	    if (ValidUntil == 0 || ValidUntil > max_date)
	       ValidUntil = max_date;
	 }
      }
   }

   LoadedSuccessfully = TRI_YES;
   return true;
}
									/*}}}*/
metaIndex * debReleaseIndex::UnloadedClone() const			/*{{{*/
{
   if (Trusted == TRI_NO)
      return new debReleaseIndex(URI, Dist, false);
   else if (Trusted == TRI_YES)
      return new debReleaseIndex(URI, Dist, true);
   else
      return new debReleaseIndex(URI, Dist);
}
									/*}}}*/
bool debReleaseIndex::parseSumData(const char *&Start, const char *End,	/*{{{*/
				   std::string &Name, std::string &Hash, unsigned long long &Size)
{
   Name = "";
   Hash = "";
   Size = 0;
   /* Skip over the first blank */
   while ((*Start == '\t' || *Start == ' ' || *Start == '\n' || *Start == '\r')
	  && Start < End)
      Start++;
   if (Start >= End)
      return false;

   /* Move EntryEnd to the end of the first entry (the hash) */
   const char *EntryEnd = Start;
   while ((*EntryEnd != '\t' && *EntryEnd != ' ')
	  && EntryEnd < End)
      EntryEnd++;
   if (EntryEnd == End)
      return false;

   Hash.append(Start, EntryEnd-Start);

   /* Skip over intermediate blanks */
   Start = EntryEnd;
   while (*Start == '\t' || *Start == ' ')
      Start++;
   if (Start >= End)
      return false;
   
   EntryEnd = Start;
   /* Find the end of the second entry (the size) */
   while ((*EntryEnd != '\t' && *EntryEnd != ' ' )
	  && EntryEnd < End)
      EntryEnd++;
   if (EntryEnd == End)
      return false;
   
   Size = strtoull (Start, NULL, 10);
      
   /* Skip over intermediate blanks */
   Start = EntryEnd;
   while (*Start == '\t' || *Start == ' ')
      Start++;
   if (Start >= End)
      return false;
   
   EntryEnd = Start;
   /* Find the end of the third entry (the filename) */
   while ((*EntryEnd != '\t' && *EntryEnd != ' ' && 
           *EntryEnd != '\n' && *EntryEnd != '\r')
	  && EntryEnd < End)
      EntryEnd++;

   Name.append(Start, EntryEnd-Start);
   Start = EntryEnd; //prepare for the next round
   return true;
}
									/*}}}*/

bool debReleaseIndex::GetIndexes(pkgAcquire *Owner, bool const &GetAll)/*{{{*/
{
   std::vector<IndexTarget> const targets = GetIndexTargets();
#define APT_TARGET(X) IndexTarget("", X, MetaIndexInfo(X), MetaIndexURI(X), false, false, std::map<std::string,std::string>())
   pkgAcqMetaClearSig * const TransactionManager = new pkgAcqMetaClearSig(Owner,
	 APT_TARGET("InRelease"), APT_TARGET("Release"), APT_TARGET("Release.gpg"),
	 targets, this);
#undef APT_TARGET
   // special case for --print-uris
   if (GetAll)
      for (auto const &Target: targets)
	 new pkgAcqIndex(Owner, TransactionManager, Target);

   return true;
}
									/*}}}*/
// ReleaseIndex::Set* TriState options					/*{{{*/
bool debReleaseIndex::SetTrusted(TriState const pTrusted)
{
   if (Trusted == TRI_UNSET)
      Trusted = pTrusted;
   else if (Trusted != pTrusted)
      // TRANSLATOR: The first is an option name from sources.list manpage, the other two URI and Suite
      return _error->Error(_("Conflicting values set for option %s regarding source %s %s"), "Trusted", URI.c_str(), Dist.c_str());
   return true;
}
bool debReleaseIndex::SetCheckValidUntil(TriState const pCheckValidUntil)
{
   if (d->CheckValidUntil == TRI_UNSET)
      d->CheckValidUntil = pCheckValidUntil;
   else if (d->CheckValidUntil != pCheckValidUntil)
      return _error->Error(_("Conflicting values set for option %s regarding source %s %s"), "Check-Valid-Until", URI.c_str(), Dist.c_str());
   return true;
}
bool debReleaseIndex::SetValidUntilMin(time_t const Valid)
{
   if (d->ValidUntilMin == 0)
      d->ValidUntilMin = Valid;
   else if (d->ValidUntilMin != Valid)
      return _error->Error(_("Conflicting values set for option %s regarding source %s %s"), "Min-ValidTime", URI.c_str(), Dist.c_str());
   return true;
}
bool debReleaseIndex::SetValidUntilMax(time_t const Valid)
{
   if (d->ValidUntilMax == 0)
      d->ValidUntilMax = Valid;
   else if (d->ValidUntilMax != Valid)
      return _error->Error(_("Conflicting values set for option %s regarding source %s %s"), "Max-ValidTime", URI.c_str(), Dist.c_str());
   return true;
}
bool debReleaseIndex::SetSignedBy(std::string const &pSignedBy)
{
   if (SignedBy.empty() == true && pSignedBy.empty() == false)
   {
      if (pSignedBy[0] == '/') // no check for existence as we could be chrooting later or such things
	 ; // absolute path to a keyring file
      else
      {
	 // we could go all fancy and allow short/long/string matches as gpgv/apt-key does,
	 // but fingerprints are harder to fake than the others and this option is set once,
	 // not interactively all the time so easy to type is not really a concern.
	 std::string finger = pSignedBy;
	 finger.erase(std::remove(finger.begin(), finger.end(), ' '), finger.end());
	 std::transform(finger.begin(), finger.end(), finger.begin(), ::toupper);
	 if (finger.length() != 40 || finger.find_first_not_of("0123456789ABCDEF") != std::string::npos)
	    return _error->Error(_("Invalid value set for option %s regarding source %s %s (%s)"), "Signed-By", URI.c_str(), Dist.c_str(), "not a fingerprint");
      }
      SignedBy = pSignedBy;
   }
   else if (SignedBy != pSignedBy)
      return _error->Error(_("Conflicting values set for option %s regarding source %s %s"), "Signed-By", URI.c_str(), Dist.c_str());
   return true;
}
									/*}}}*/
// ReleaseIndex::IsTrusted						/*{{{*/
bool debReleaseIndex::IsTrusted() const
{
   if (Trusted == TRI_YES)
      return true;
   else if (Trusted == TRI_NO)
      return false;


   if(_config->FindB("APT::Authentication::TrustCDROM", false))
      if(URI.substr(0,strlen("cdrom:")) == "cdrom:")
	 return true;

   if (FileExists(MetaIndexFile("Release.gpg")))
      return true;

   return FileExists(MetaIndexFile("InRelease"));
}
									/*}}}*/
bool debReleaseIndex::IsArchitectureSupported(std::string const &arch) const/*{{{*/
{
   if (d->Architectures.empty())
      return true;
   return std::find(d->Architectures.begin(), d->Architectures.end(), arch) != d->Architectures.end();
}
									/*}}}*/
bool debReleaseIndex::IsArchitectureAllSupportedFor(IndexTarget const &target) const/*{{{*/
{
   if (d->NoSupportForAll.empty())
      return true;
   return std::find(d->NoSupportForAll.begin(), d->NoSupportForAll.end(), target.Option(IndexTarget::CREATED_BY)) == d->NoSupportForAll.end();
}
									/*}}}*/
std::vector <pkgIndexFile *> *debReleaseIndex::GetIndexFiles()		/*{{{*/
{
   if (Indexes != NULL)
      return Indexes;

   Indexes = new std::vector<pkgIndexFile*>();
   bool const istrusted = IsTrusted();
   for (auto const &T: GetIndexTargets())
   {
      std::string const TargetName = T.Option(IndexTarget::CREATED_BY);
      if (TargetName == "Packages")
	 Indexes->push_back(new debPackagesIndex(T, istrusted));
      else if (TargetName == "Sources")
	 Indexes->push_back(new debSourcesIndex(T, istrusted));
      else if (TargetName == "Translations")
	 Indexes->push_back(new debTranslationsIndex(T));
   }
   return Indexes;
}
									/*}}}*/

static bool ReleaseFileName(debReleaseIndex const * const That, std::string &ReleaseFile)/*{{{*/
{
   ReleaseFile = That->MetaIndexFile("InRelease");
   bool releaseExists = false;
   if (FileExists(ReleaseFile) == true)
      releaseExists = true;
   else
   {
      ReleaseFile = That->MetaIndexFile("Release");
      if (FileExists(ReleaseFile))
	 releaseExists = true;
   }
   return releaseExists;
}
									/*}}}*/
bool debReleaseIndex::Merge(pkgCacheGenerator &Gen,OpProgress * /*Prog*/) const/*{{{*/
{
   std::string ReleaseFile;
   bool const releaseExists = ReleaseFileName(this, ReleaseFile);

   ::URI Tmp(URI);
   if (Gen.SelectReleaseFile(ReleaseFile, Tmp.Host) == false)
      return _error->Error("Problem with SelectReleaseFile %s", ReleaseFile.c_str());

   if (releaseExists == false)
      return true;

   FileFd Rel;
   // Beware: The 'Release' file might be clearsigned in case the
   // signature for an 'InRelease' file couldn't be checked
   if (OpenMaybeClearSignedFile(ReleaseFile, Rel) == false)
      return false;

   // Store the IMS information
   pkgCache::RlsFileIterator File = Gen.GetCurRlsFile();
   pkgCacheGenerator::Dynamic<pkgCache::RlsFileIterator> DynFile(File);
   // Rel can't be used as this is potentially a temporary file
   struct stat Buf;
   if (stat(ReleaseFile.c_str(), &Buf) != 0)
      return _error->Errno("fstat", "Unable to stat file %s", ReleaseFile.c_str());
   File->Size = Buf.st_size;
   File->mtime = Buf.st_mtime;

   pkgTagFile TagFile(&Rel, Rel.Size());
   pkgTagSection Section;
   if (Rel.IsOpen() == false || Rel.Failed() || TagFile.Step(Section) == false)
      return false;

   std::string data;
   #define APT_INRELEASE(TYPE, TAG, STORE) \
   data = Section.FindS(TAG); \
   if (data.empty() == false) \
   { \
      map_stringitem_t const storage = Gen.StoreString(pkgCacheGenerator::TYPE, data); \
      if (storage == 0) return false; \
      STORE = storage; \
   }
   APT_INRELEASE(MIXED, "Suite", File->Archive)
   APT_INRELEASE(VERSIONNUMBER, "Version", File->Version)
   APT_INRELEASE(MIXED, "Origin", File->Origin)
   APT_INRELEASE(MIXED, "Codename", File->Codename)
   APT_INRELEASE(MIXED, "Label", File->Label)
   #undef APT_INRELEASE
   Section.FindFlag("NotAutomatic", File->Flags, pkgCache::Flag::NotAutomatic);
   Section.FindFlag("ButAutomaticUpgrades", File->Flags, pkgCache::Flag::ButAutomaticUpgrades);

   return true;
}
									/*}}}*/
// ReleaseIndex::FindInCache - Find this index				/*{{{*/
pkgCache::RlsFileIterator debReleaseIndex::FindInCache(pkgCache &Cache, bool const ModifyCheck) const
{
   std::string ReleaseFile;
   bool const releaseExists = ReleaseFileName(this, ReleaseFile);

   pkgCache::RlsFileIterator File = Cache.RlsFileBegin();
   for (; File.end() == false; ++File)
   {
       if (File->FileName == 0 || ReleaseFile != File.FileName())
	 continue;

       // empty means the file does not exist by "design"
       if (ModifyCheck == false || (releaseExists == false && File->Size == 0))
	  return File;

      struct stat St;
      if (stat(File.FileName(),&St) != 0)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "ReleaseIndex::FindInCache - stat failed on " << File.FileName() << std::endl;
	 return pkgCache::RlsFileIterator(Cache);
      }
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "ReleaseIndex::FindInCache - size (" << St.st_size << " <> " << File->Size
			<< ") or mtime (" << St.st_mtime << " <> " << File->mtime
			<< ") doesn't match for " << File.FileName() << std::endl;
	 return pkgCache::RlsFileIterator(Cache);
      }
      return File;
   }

   return File;
}
									/*}}}*/

static std::vector<std::string> parsePlusMinusOptions(std::string const &Name, /*{{{*/
      std::map<std::string, std::string> const &Options, std::vector<std::string> const &defaultValues)
{
   std::map<std::string, std::string>::const_iterator val = Options.find(Name);
   std::vector<std::string> Values;
   if (val != Options.end())
      Values = VectorizeString(val->second, ',');
   else
      Values = defaultValues;

   // all is a very special architecture users shouldn't be concerned with explicitly
   if (Name == "arch" && std::find(Values.begin(), Values.end(), "all") == Values.end())
      Values.push_back("all");

   if ((val = Options.find(Name + "+")) != Options.end())
   {
      std::vector<std::string> const plus = VectorizeString(val->second, ',');
      std::copy_if(plus.begin(), plus.end(), std::back_inserter(Values), [&Values](std::string const &v) {
	 return std::find(Values.begin(), Values.end(), v) == Values.end();
      });
   }
   if ((val = Options.find(Name + "-")) != Options.end())
   {
      std::vector<std::string> const minus = VectorizeString(val->second, ',');
      Values.erase(std::remove_if(Values.begin(), Values.end(), [&minus](std::string const &v) {
	 return std::find(minus.begin(), minus.end(), v) != minus.end();
      }), Values.end());
   }
   return Values;
}
									/*}}}*/
class APT_HIDDEN debSLTypeDebian : public pkgSourceList::Type		/*{{{*/
{
   metaIndex::TriState GetTriStateOption(std::map<std::string, std::string>const &Options, char const * const name) const
   {
      std::map<std::string, std::string>::const_iterator const opt = Options.find(name);
      if (opt != Options.end())
	 return StringToBool(opt->second, false) ? metaIndex::TRI_YES : metaIndex::TRI_NO;
      return metaIndex::TRI_DONTCARE;
   }

   time_t GetTimeOption(std::map<std::string, std::string>const &Options, char const * const name) const
   {
      std::map<std::string, std::string>::const_iterator const opt = Options.find(name);
      if (opt == Options.end())
	 return 0;
      return strtoull(opt->second.c_str(), NULL, 10);
   }

   protected:

   bool CreateItemInternal(std::vector<metaIndex *> &List, std::string const &URI,
			   std::string const &Dist, std::string const &Section,
			   bool const &IsSrc, std::map<std::string, std::string> const &Options) const
   {
      debReleaseIndex *Deb = NULL;
      for (std::vector<metaIndex *>::const_iterator I = List.begin();
	   I != List.end(); ++I)
      {
	 // We only worry about debian entries here
	 if (strcmp((*I)->GetType(), "deb") != 0)
	    continue;

	 /* This check insures that there will be only one Release file
	    queued for all the Packages files and Sources files it
	    corresponds to. */
	 if ((*I)->GetURI() == URI && (*I)->GetDist() == Dist)
	 {
	    Deb = dynamic_cast<debReleaseIndex*>(*I);
	    if (Deb != NULL)
	       break;
	 }
      }

      // No currently created Release file indexes this entry, so we create a new one.
      if (Deb == NULL)
      {
	 Deb = new debReleaseIndex(URI, Dist);
	 List.push_back(Deb);
      }

      std::vector<std::string> const alltargets = _config->FindVector(std::string("Acquire::IndexTargets::") + Name, "", true);
      std::vector<std::string> deftargets;
      deftargets.reserve(alltargets.size());
      std::copy_if(alltargets.begin(), alltargets.end(), std::back_inserter(deftargets), [&](std::string const &t) {
	 std::string c = "Acquire::IndexTargets::";
	 c.append(Name).append("::").append(t).append("::DefaultEnabled");
	 return _config->FindB(c, true);
      });
      std::vector<std::string> mytargets = parsePlusMinusOptions("target", Options, deftargets);
      for (auto const &target : alltargets)
      {
	 std::map<std::string, std::string>::const_iterator const opt = Options.find(target);
	 if (opt == Options.end())
	    continue;
	 auto const tarItr = std::find(mytargets.begin(), mytargets.end(), target);
	 bool const optValue = StringToBool(opt->second);
	 if (optValue == true && tarItr == mytargets.end())
	    mytargets.push_back(target);
	 else if (optValue == false && tarItr != mytargets.end())
	    mytargets.erase(std::remove(mytargets.begin(), mytargets.end(), target), mytargets.end());
      }

      bool UsePDiffs = _config->FindB("Acquire::PDiffs", true);
      {
	 std::map<std::string, std::string>::const_iterator const opt = Options.find("pdiffs");
	 if (opt != Options.end())
	    UsePDiffs = StringToBool(opt->second);
      }

      std::string UseByHash = _config->Find("APT::Acquire::By-Hash", "yes");
      UseByHash = _config->Find("Acquire::By-Hash", UseByHash);
      {
	 std::string const host = ::URI(URI).Host;
	 if (host.empty() == false)
	 {
	    UseByHash = _config->Find("APT::Acquire::" + host + "::By-Hash", UseByHash);
	    UseByHash = _config->Find("Acquire::" + host + "::By-Hash", UseByHash);
	 }
	 std::map<std::string, std::string>::const_iterator const opt = Options.find("by-hash");
	 if (opt != Options.end())
	    UseByHash = opt->second;
      }

      auto const entry = Options.find("sourceslist-entry");
      Deb->AddComponent(
	    entry->second,
	    IsSrc,
	    Section,
	    mytargets,
	    parsePlusMinusOptions("arch", Options, APT::Configuration::getArchitectures()),
	    parsePlusMinusOptions("lang", Options, APT::Configuration::getLanguages(true)),
	    UsePDiffs,
	    UseByHash
	    );

      if (Deb->SetTrusted(GetTriStateOption(Options, "trusted")) == false ||
	 Deb->SetCheckValidUntil(GetTriStateOption(Options, "check-valid-until")) == false ||
	 Deb->SetValidUntilMax(GetTimeOption(Options, "valid-until-max")) == false ||
	 Deb->SetValidUntilMin(GetTimeOption(Options, "valid-until-min")) == false)
	 return false;

      std::map<std::string, std::string>::const_iterator const signedby = Options.find("signed-by");
      if (signedby == Options.end())
      {
	 if (Deb->SetSignedBy("") == false)
	    return false;
      }
      else
      {
	 if (Deb->SetSignedBy(signedby->second) == false)
	    return false;
      }

      return true;
   }

   debSLTypeDebian(char const * const Name, char const * const Label) : Type(Name, Label)
   {
   }
};
									/*}}}*/
class APT_HIDDEN debSLTypeDeb : public debSLTypeDebian			/*{{{*/
{
   public:

   bool CreateItem(std::vector<metaIndex *> &List, std::string const &URI,
		   std::string const &Dist, std::string const &Section,
		   std::map<std::string, std::string> const &Options) const APT_OVERRIDE
   {
      return CreateItemInternal(List, URI, Dist, Section, false, Options);
   }

   debSLTypeDeb() : debSLTypeDebian("deb", "Debian binary tree")
   {
   }
};
									/*}}}*/
class APT_HIDDEN debSLTypeDebSrc : public debSLTypeDebian		/*{{{*/
{
   public:

   bool CreateItem(std::vector<metaIndex *> &List, std::string const &URI,
		   std::string const &Dist, std::string const &Section,
		   std::map<std::string, std::string> const &Options) const APT_OVERRIDE
   {
      return CreateItemInternal(List, URI, Dist, Section, true, Options);
   }

   debSLTypeDebSrc() : debSLTypeDebian("deb-src", "Debian source tree")
   {
   }
};
									/*}}}*/

APT_HIDDEN debSLTypeDeb _apt_DebType;
APT_HIDDEN debSLTypeDebSrc _apt_DebSrcType;

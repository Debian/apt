#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/indexrecords.h>
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
#include <set>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <apti18n.h>

class APT_HIDDEN debReleaseIndexPrivate					/*{{{*/
{
   public:
   struct APT_HIDDEN debSectionEntry
   {
      std::string Name;
      std::vector<std::string> Targets;
      std::vector<std::string> Architectures;
      std::vector<std::string> Languages;
   };

   std::vector<debSectionEntry> DebEntries;
   std::vector<debSectionEntry> DebSrcEntries;

   debReleaseIndex::TriState Trusted;

   debReleaseIndexPrivate() : Trusted(debReleaseIndex::TRI_UNSET) {}
   debReleaseIndexPrivate(bool const pTrusted) : Trusted(pTrusted ? debReleaseIndex::TRI_YES : debReleaseIndex::TRI_NO) {}
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
std::string debReleaseIndex::LocalFileName() const			/*{{{*/
{
   // see if we have a InRelease file
   std::string PathInRelease =  MetaIndexFile("InRelease");
   if (FileExists(PathInRelease))
      return PathInRelease;

   // and if not return the normal one
   if (FileExists(PathInRelease))
      return MetaIndexFile("Release");

   return "";
}
									/*}}}*/
// ReleaseIndex Con- and Destructors					/*{{{*/
debReleaseIndex::debReleaseIndex(std::string const &URI, std::string const &Dist) :
					metaIndex(URI, Dist, "deb"), d(new debReleaseIndexPrivate())
{}
debReleaseIndex::debReleaseIndex(std::string const &URI, std::string const &Dist, bool const Trusted) :
					metaIndex(URI, Dist, "deb"), d(new debReleaseIndexPrivate(Trusted))
{}
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

   for (std::vector<debReleaseIndexPrivate::debSectionEntry>::const_iterator E = entries.begin(); E != entries.end(); ++E)
   {
      for (std::vector<std::string>::const_iterator T = E->Targets.begin(); T != E->Targets.end(); ++T)
      {
#define APT_T_CONFIG(X) _config->Find(std::string("APT::Acquire::Targets::") + Type  + "::" + *T + "::" + (X))
	 std::string const tplMetaKey = APT_T_CONFIG(flatArchive ? "flatMetaKey" : "MetaKey");
	 std::string const tplShortDesc = APT_T_CONFIG("ShortDescription");
	 std::string const tplLongDesc = APT_T_CONFIG(flatArchive ? "flatDescription" : "Description");
	 bool const IsOptional = _config->FindB(std::string("APT::Acquire::Targets::deb-src::") + *T + "::Optional", true);
#undef APT_T_CONFIG
	 if (tplMetaKey.empty())
	    continue;

	 for (std::vector<std::string>::const_iterator L = E->Languages.begin(); L != E->Languages.end(); ++L)
	 {
	    if (*L == "none" && tplMetaKey.find("$(LANGUAGE)") != std::string::npos)
	       continue;

	    for (std::vector<std::string>::const_iterator A = E->Architectures.begin(); A != E->Architectures.end(); ++A)
	    {

	       std::map<std::string, std::string> Options;
	       Options.insert(std::make_pair("SITE", Site));
	       Options.insert(std::make_pair("RELEASE", Release));
	       if (tplMetaKey.find("$(COMPONENT)") != std::string::npos)
		  Options.insert(std::make_pair("COMPONENT", E->Name));
	       if (tplMetaKey.find("$(LANGUAGE)") != std::string::npos)
		  Options.insert(std::make_pair("LANGUAGE", *L));
	       if (tplMetaKey.find("$(ARCHITECTURE)") != std::string::npos)
		  Options.insert(std::make_pair("ARCHITECTURE", *A));
	       Options.insert(std::make_pair("BASE_URI", baseURI));
	       Options.insert(std::make_pair("REPO_URI", URI));
	       Options.insert(std::make_pair("TARGET_OF", "deb-src"));
	       Options.insert(std::make_pair("CREATED_BY", *T));

	       std::string MetaKey = tplMetaKey;
	       std::string ShortDesc = tplShortDesc;
	       std::string LongDesc = tplLongDesc;
	       for (std::map<std::string, std::string>::const_iterator O = Options.begin(); O != Options.end(); ++O)
	       {
		  MetaKey = SubstVar(MetaKey, std::string("$(") + O->first + ")", O->second);
		  ShortDesc = SubstVar(ShortDesc, std::string("$(") + O->first + ")", O->second);
		  LongDesc = SubstVar(LongDesc, std::string("$(") + O->first + ")", O->second);
	       }
	       IndexTarget Target(
		     MetaKey,
		     ShortDesc,
		     LongDesc,
		     Options.find("BASE_URI")->second + MetaKey,
		     IsOptional,
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
void debReleaseIndex::AddComponent(bool const isSrc, std::string const &Name,/*{{{*/
	 std::vector<std::string> const &Targets,
	 std::vector<std::string> const &Architectures,
	 std::vector<std::string> Languages)
{
   if (Languages.empty() == true)
      Languages.push_back("none");
   debReleaseIndexPrivate::debSectionEntry const entry = {
      Name, Targets, Architectures, Languages
   };
   if (isSrc)
      d->DebSrcEntries.push_back(entry);
   else
      d->DebEntries.push_back(entry);
}
									/*}}}*/


bool debReleaseIndex::GetIndexes(pkgAcquire *Owner, bool const &GetAll) const/*{{{*/
{
   indexRecords * const iR = new indexRecords(Dist);
   if (d->Trusted == TRI_YES)
      iR->SetTrusted(true);
   else if (d->Trusted == TRI_NO)
      iR->SetTrusted(false);

   // special case for --print-uris
   std::vector<IndexTarget> const targets = GetIndexTargets();
#define APT_TARGET(X) IndexTarget("", X, MetaIndexInfo(X), MetaIndexURI(X), false, std::map<std::string,std::string>())
   pkgAcqMetaClearSig * const TransactionManager = new pkgAcqMetaClearSig(Owner,
	 APT_TARGET("InRelease"), APT_TARGET("Release"), APT_TARGET("Release.gpg"),
	 targets, iR);
#undef APT_TARGET
   if (GetAll)
   {
      for (std::vector<IndexTarget>::const_iterator Target = targets.begin(); Target != targets.end(); ++Target)
	 new pkgAcqIndex(Owner, TransactionManager, *Target);
   }

   return true;
}
									/*}}}*/
// ReleaseIndex::IsTrusted						/*{{{*/
bool debReleaseIndex::SetTrusted(TriState const Trusted)
{
   if (d->Trusted == TRI_UNSET)
      d->Trusted = Trusted;
   else if (d->Trusted != Trusted)
      // TRANSLATOR: The first is an option name from sources.list manpage, the other two URI and Suite
      return _error->Error(_("Conflicting values set for option %s concerning source %s %s"), "Trusted", URI.c_str(), Dist.c_str());
   return true;
}
bool debReleaseIndex::IsTrusted() const
{
   if (d->Trusted == TRI_YES)
      return true;
   else if (d->Trusted == TRI_NO)
      return false;


   if(_config->FindB("APT::Authentication::TrustCDROM", false))
      if(URI.substr(0,strlen("cdrom:")) == "cdrom:")
	 return true;

   if (FileExists(MetaIndexFile("Release.gpg")))
      return true;

   return FileExists(MetaIndexFile("InRelease"));
}
									/*}}}*/
std::vector <pkgIndexFile *> *debReleaseIndex::GetIndexFiles()		/*{{{*/
{
   if (Indexes != NULL)
      return Indexes;

   Indexes = new std::vector<pkgIndexFile*>();
   std::vector<IndexTarget> const Targets = GetIndexTargets();
   bool const istrusted = IsTrusted();
   for (std::vector<IndexTarget>::const_iterator T = Targets.begin(); T != Targets.end(); ++T)
   {
      std::string const TargetName = T->Option(IndexTarget::CREATED_BY);
      if (TargetName == "Packages")
	 Indexes->push_back(new debPackagesIndex(*T, istrusted));
      else if (TargetName == "Sources")
	 Indexes->push_back(new debSourcesIndex(*T, istrusted));
      else if (TargetName == "Translations")
	 Indexes->push_back(new debTranslationsIndex(*T));
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
   if (_error->PendingError() == true)
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
   if (_error->PendingError() == true || TagFile.Step(Section) == false)
      return false;

   std::string data;
   #define APT_INRELEASE(TYPE, TAG, STORE) \
   data = Section.FindS(TAG); \
   if (data.empty() == false) \
   { \
      map_stringitem_t const storage = Gen.StoreString(pkgCacheGenerator::TYPE, data); \
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

   return !_error->PendingError();
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

   if ((val = Options.find(Name + "+")) != Options.end())
   {
      std::vector<std::string> const plusArch = VectorizeString(val->second, ',');
      for (std::vector<std::string>::const_iterator plus = plusArch.begin(); plus != plusArch.end(); ++plus)
	 if (std::find(Values.begin(), Values.end(), *plus) == Values.end())
	    Values.push_back(*plus);
   }
   if ((val = Options.find(Name + "-")) != Options.end())
   {
      std::vector<std::string> const minusArch = VectorizeString(val->second, ',');
      for (std::vector<std::string>::const_iterator minus = minusArch.begin(); minus != minusArch.end(); ++minus)
      {
	 std::vector<std::string>::iterator kill = std::find(Values.begin(), Values.end(), *minus);
	 if (kill != Values.end())
	    Values.erase(kill);
      }
   }
   return Values;
}
									/*}}}*/
class APT_HIDDEN debSLTypeDebian : public pkgSourceList::Type		/*{{{*/
{
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

      Deb->AddComponent(
	    IsSrc,
	    Section,
	    parsePlusMinusOptions("target", Options, _config->FindVector(std::string("APT::Acquire::Targets::") + Name, "", true)),
	    parsePlusMinusOptions("arch", Options, APT::Configuration::getArchitectures()),
	    parsePlusMinusOptions("lang", Options, APT::Configuration::getLanguages(true))
	    );

      std::map<std::string, std::string>::const_iterator const trusted = Options.find("trusted");
      if (trusted != Options.end())
      {
	 if (Deb->SetTrusted(StringToBool(trusted->second, false) ? debReleaseIndex::TRI_YES : debReleaseIndex::TRI_NO) == false)
	    return false;
      }
      else if (Deb->SetTrusted(debReleaseIndex::TRI_DONTCARE) == false)
	 return false;

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
		   std::map<std::string, std::string> const &Options) const
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
		   std::map<std::string, std::string> const &Options) const
   {
      return CreateItemInternal(List, URI, Dist, Section, true, Options);
   }

   debSLTypeDebSrc() : debSLTypeDebian("deb-src", "Debian source tree")
   {
   }
};
									/*}}}*/

debDebFileMetaIndex::debDebFileMetaIndex(std::string const &DebFile)	/*{{{*/
   : metaIndex(DebFile, "local-uri", "deb-dist"), d(NULL), DebFile(DebFile)
{
   DebIndex = new debDebPkgFileIndex(DebFile);
   Indexes = new std::vector<pkgIndexFile *>();
   Indexes->push_back(DebIndex);
}
debDebFileMetaIndex::~debDebFileMetaIndex() {}
									/*}}}*/
class APT_HIDDEN debSLTypeDebFile : public pkgSourceList::Type		/*{{{*/
{
   public:

   bool CreateItem(std::vector<metaIndex *> &List, std::string const &URI,
		   std::string const &/*Dist*/, std::string const &/*Section*/,
		   std::map<std::string, std::string> const &/*Options*/) const
   {
      metaIndex *mi = new debDebFileMetaIndex(URI);
      List.push_back(mi);
      return true;
   }

   debSLTypeDebFile() : Type("deb-file", "Debian local deb file")
   {
   }
};
									/*}}}*/

APT_HIDDEN debSLTypeDeb _apt_DebType;
APT_HIDDEN debSLTypeDebSrc _apt_DebSrcType;
APT_HIDDEN debSLTypeDebFile _apt_DebFileType;

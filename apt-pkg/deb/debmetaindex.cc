#include <config.h>

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
#include <apt-pkg/macros.h>
#include <apt-pkg/metaindex.h>

#include <string.h>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include <algorithm>

using namespace std;

string debReleaseIndex::MetaIndexInfo(const char *Type) const
{
   string Info = ::URI::SiteOnly(URI) + ' ';
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

string debReleaseIndex::MetaIndexFile(const char *Type) const
{
   return _config->FindDir("Dir::State::lists") +
      URItoFileName(MetaIndexURI(Type));
}

string debReleaseIndex::MetaIndexURI(const char *Type) const
{
   string Res;

   if (Dist == "/")
      Res = URI;
   else if (Dist[Dist.size()-1] == '/')
      Res = URI + Dist;
   else
      Res = URI + "dists/" + Dist + "/";
   
   Res += Type;
   return Res;
}

std::string debReleaseIndex::LocalFileName() const
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

debReleaseIndex::debReleaseIndex(string const &URI, string const &Dist) :
					metaIndex(URI, Dist, "deb"), Trusted(CHECK_TRUST)
{}

debReleaseIndex::debReleaseIndex(string const &URI, string const &Dist, bool const Trusted) :
					metaIndex(URI, Dist, "deb") {
	SetTrusted(Trusted);
}

debReleaseIndex::~debReleaseIndex() {
	for (map<string, vector<debSectionEntry const*> >::const_iterator A = ArchEntries.begin();
	     A != ArchEntries.end(); ++A)
		for (vector<const debSectionEntry *>::const_iterator S = A->second.begin();
		     S != A->second.end(); ++S)
			delete *S;
}

template<typename CallC>
void foreachTarget(std::string const URI, std::string const Dist,
      std::map<std::string, std::vector<debReleaseIndex::debSectionEntry const *> > const &ArchEntries,
      CallC Call)
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
   std::string const Site = ::URI::SiteOnly(URI);
   std::vector<std::string> lang = APT::Configuration::getLanguages(true);
   if (lang.empty())
      lang.push_back("none");
   map<string, vector<debReleaseIndex::debSectionEntry const*> >::const_iterator const src = ArchEntries.find("source");
   if (src != ArchEntries.end())
   {
      std::vector<std::string> const targets = _config->FindVector("APT::Acquire::Targets::deb-src", "", true);
      for (std::vector<std::string>::const_iterator T = targets.begin(); T != targets.end(); ++T)
      {
#define APT_T_CONFIG(X) _config->Find(std::string("APT::Acquire::Targets::deb-src::") + *T + "::" + (X))
	 std::string const MetaKey = APT_T_CONFIG(flatArchive ? "flatMetaKey" : "MetaKey");
	 std::string const ShortDesc = APT_T_CONFIG("ShortDescription");
	 std::string const LongDesc = APT_T_CONFIG(flatArchive ? "flatDescription" : "Description");
	 bool const IsOptional = _config->FindB(std::string("APT::Acquire::Targets::deb-src::") + *T + "::Optional", true);
#undef APT_T_CONFIG
	 if (MetaKey.empty())
	    continue;

	 vector<debReleaseIndex::debSectionEntry const*> const SectionEntries = src->second;
	 for (vector<debReleaseIndex::debSectionEntry const*>::const_iterator I = SectionEntries.begin();
	       I != SectionEntries.end(); ++I)
	 {
	    for (vector<std::string>::const_iterator l = lang.begin(); l != lang.end(); ++l)
	    {
	       if (*l == "none" && MetaKey.find("$(LANGUAGE)") != std::string::npos)
		  continue;

	       std::map<std::string, std::string> Options;
	       Options.insert(std::make_pair("SITE", Site));
	       Options.insert(std::make_pair("RELEASE", Release));
	       Options.insert(std::make_pair("COMPONENT", (*I)->Section));
	       Options.insert(std::make_pair("LANGUAGE", *l));
	       Options.insert(std::make_pair("ARCHITECTURE", "source"));
	       Options.insert(std::make_pair("BASE_URI", baseURI));
	       Options.insert(std::make_pair("CREATED_BY", *T));
	       Call(MetaKey, ShortDesc, LongDesc, IsOptional, Options);

	       if (MetaKey.find("$(LANGUAGE)") == std::string::npos)
		  break;
	    }

	    if (MetaKey.find("$(COMPONENT)") == std::string::npos)
	       break;
	 }
      }
   }

   std::vector<std::string> const targets = _config->FindVector("APT::Acquire::Targets::deb", "", true);
   for (std::vector<std::string>::const_iterator T = targets.begin(); T != targets.end(); ++T)
   {
#define APT_T_CONFIG(X) _config->Find(std::string("APT::Acquire::Targets::deb::") + *T + "::" + (X))
      std::string const MetaKey = APT_T_CONFIG(flatArchive ? "flatMetaKey" : "MetaKey");
      std::string const ShortDesc = APT_T_CONFIG("ShortDescription");
      std::string const LongDesc = APT_T_CONFIG(flatArchive ? "flatDescription" : "Description");
      bool const IsOptional = _config->FindB(std::string("APT::Acquire::Targets::deb::") + *T + "::Optional", true);
#undef APT_T_CONFIG
      if (MetaKey.empty())
	 continue;

      for (map<string, vector<debReleaseIndex::debSectionEntry const*> >::const_iterator a = ArchEntries.begin();
	    a != ArchEntries.end(); ++a)
      {
	 if (a->first == "source")
	    continue;

	 for (vector <const debReleaseIndex::debSectionEntry *>::const_iterator I = a->second.begin();
	       I != a->second.end(); ++I) {

	    for (vector<std::string>::const_iterator l = lang.begin(); l != lang.end(); ++l)
	    {
	       if (*l == "none" && MetaKey.find("$(LANGUAGE)") != std::string::npos)
		  continue;

	       std::map<std::string, std::string> Options;
	       Options.insert(std::make_pair("SITE", Site));
	       Options.insert(std::make_pair("RELEASE", Release));
	       Options.insert(std::make_pair("COMPONENT", (*I)->Section));
	       Options.insert(std::make_pair("LANGUAGE", *l));
	       Options.insert(std::make_pair("ARCHITECTURE", a->first));
	       Options.insert(std::make_pair("BASE_URI", baseURI));
	       Options.insert(std::make_pair("CREATED_BY", *T));
	       Call(MetaKey, ShortDesc, LongDesc, IsOptional, Options);

	       if (MetaKey.find("$(LANGUAGE)") == std::string::npos)
		  break;
	    }

	    if (MetaKey.find("$(COMPONENT)") == std::string::npos)
	       break;
	 }

	 if (MetaKey.find("$(ARCHITECTURE)") == std::string::npos)
	    break;
      }
   }
}


struct ComputeIndexTargetsClass
{
   vector <IndexTarget *> * const IndexTargets;

   void operator()(std::string MetaKey, std::string ShortDesc, std::string LongDesc,
	 bool const IsOptional, std::map<std::string, std::string> Options)
   {
      for (std::map<std::string, std::string>::const_iterator O = Options.begin(); O != Options.end(); ++O)
      {
	 MetaKey = SubstVar(MetaKey, std::string("$(") + O->first + ")", O->second);
	 ShortDesc = SubstVar(ShortDesc, std::string("$(") + O->first + ")", O->second);
	 LongDesc = SubstVar(LongDesc, std::string("$(") + O->first + ")", O->second);
      }
      IndexTarget * Target = new IndexTarget(
	    MetaKey,
	    ShortDesc,
	    LongDesc,
	    Options.find("BASE_URI")->second + MetaKey,
	    IsOptional,
	    Options
	    );
      IndexTargets->push_back(Target);
   }

   ComputeIndexTargetsClass() : IndexTargets(new vector <IndexTarget *>) {}
};

vector <IndexTarget *>* debReleaseIndex::ComputeIndexTargets() const
{
   ComputeIndexTargetsClass comp;
   foreachTarget(URI, Dist, ArchEntries, comp);
   return comp.IndexTargets;
}


									/*}}}*/
bool debReleaseIndex::GetIndexes(pkgAcquire *Owner, bool const &GetAll) const
{
   indexRecords * const iR = new indexRecords(Dist);
   if (Trusted == ALWAYS_TRUSTED)
      iR->SetTrusted(true);
   else if (Trusted == NEVER_TRUSTED)
      iR->SetTrusted(false);

   // special case for --print-uris
   vector <IndexTarget *> const * const targets = ComputeIndexTargets();
#define APT_TARGET(X) IndexTarget("", X, MetaIndexInfo(X), MetaIndexURI(X), false, std::map<std::string,std::string>())
   pkgAcqMetaBase * const TransactionManager = new pkgAcqMetaClearSig(Owner,
	 APT_TARGET("InRelease"), APT_TARGET("Release"), APT_TARGET("Release.gpg"),
	 targets, iR);
#undef APT_TARGET
   if (GetAll)
   {
      for (vector <IndexTarget*>::const_iterator Target = targets->begin(); Target != targets->end(); ++Target)
	 new pkgAcqIndex(Owner, TransactionManager, *Target);
   }

   return true;
}

void debReleaseIndex::SetTrusted(bool const Trusted)
{
	if (Trusted == true)
		this->Trusted = ALWAYS_TRUSTED;
	else
		this->Trusted = NEVER_TRUSTED;
}

bool debReleaseIndex::IsTrusted() const
{
   if (Trusted == ALWAYS_TRUSTED)
      return true;
   else if (Trusted == NEVER_TRUSTED)
      return false;


   if(_config->FindB("APT::Authentication::TrustCDROM", false))
      if(URI.substr(0,strlen("cdrom:")) == "cdrom:")
	 return true;

   string VerifiedSigFile = _config->FindDir("Dir::State::lists") +
      URItoFileName(MetaIndexURI("Release")) + ".gpg";

   if (FileExists(VerifiedSigFile))
      return true;

   VerifiedSigFile = _config->FindDir("Dir::State::lists") +
      URItoFileName(MetaIndexURI("InRelease"));

   return FileExists(VerifiedSigFile);
}

struct GetIndexFilesClass
{
   vector <pkgIndexFile *> * const Indexes;
   std::string const URI;
   std::string const Release;
   bool const IsTrusted;

   void operator()(std::string const &/*URI*/, std::string const &/*ShortDesc*/, std::string const &/*LongDesc*/,
	 bool const /*IsOptional*/, std::map<std::string, std::string> Options)
   {
      std::string const TargetName = Options.find("CREATED_BY")->second;
      if (TargetName == "Packages")
      {
	 Indexes->push_back(new debPackagesIndex(
		  URI,
		  Release,
		  Options.find("COMPONENT")->second,
		  IsTrusted,
		  Options.find("ARCHITECTURE")->second
		  ));
      }
      else if (TargetName == "Sources")
	 Indexes->push_back(new debSourcesIndex(
		  URI,
		  Release,
		  Options.find("COMPONENT")->second,
		  IsTrusted
		  ));
      else if (TargetName == "Translations")
	 Indexes->push_back(new debTranslationsIndex(
		  URI,
		  Release,
		  Options.find("COMPONENT")->second,
		  Options.find("LANGUAGE")->second
		  ));
   }

   GetIndexFilesClass(std::string const &URI, std::string const &Release, bool const IsTrusted) :
      Indexes(new vector <pkgIndexFile*>), URI(URI), Release(Release), IsTrusted(IsTrusted) {}
};

std::vector <pkgIndexFile *> *debReleaseIndex::GetIndexFiles()
{
   if (Indexes != NULL)
      return Indexes;

   GetIndexFilesClass comp(URI, Dist, IsTrusted());
   foreachTarget(URI, Dist, ArchEntries, comp);
   return Indexes = comp.Indexes;
}

void debReleaseIndex::PushSectionEntry(vector<string> const &Archs, const debSectionEntry *Entry) {
	for (vector<string>::const_iterator a = Archs.begin();
	     a != Archs.end(); ++a)
		ArchEntries[*a].push_back(new debSectionEntry(Entry->Section, Entry->IsSrc));
	delete Entry;
}

void debReleaseIndex::PushSectionEntry(string const &Arch, const debSectionEntry *Entry) {
	ArchEntries[Arch].push_back(Entry);
}

debReleaseIndex::debSectionEntry::debSectionEntry (string const &Section,
		bool const &IsSrc): Section(Section), IsSrc(IsSrc)
{}

class APT_HIDDEN debSLTypeDebian : public pkgSourceList::Type
{
   protected:

   bool CreateItemInternal(vector<metaIndex *> &List, string const &URI,
			   string const &Dist, string const &Section,
			   bool const &IsSrc, map<string, string> const &Options) const
   {
      // parse arch=, arch+= and arch-= settings
      map<string, string>::const_iterator arch = Options.find("arch");
      vector<string> Archs;
      if (arch != Options.end())
	 Archs = VectorizeString(arch->second, ',');
      else
	 Archs = APT::Configuration::getArchitectures();

      if ((arch = Options.find("arch+")) != Options.end())
      {
	 std::vector<std::string> const plusArch = VectorizeString(arch->second, ',');
	 for (std::vector<std::string>::const_iterator plus = plusArch.begin(); plus != plusArch.end(); ++plus)
	    if (std::find(Archs.begin(), Archs.end(), *plus) == Archs.end())
	       Archs.push_back(*plus);
      }
      if ((arch = Options.find("arch-")) != Options.end())
      {
	 std::vector<std::string> const minusArch = VectorizeString(arch->second, ',');
	 for (std::vector<std::string>::const_iterator minus = minusArch.begin(); minus != minusArch.end(); ++minus)
	 {
	    std::vector<std::string>::iterator kill = std::find(Archs.begin(), Archs.end(), *minus);
	    if (kill != Archs.end())
	       Archs.erase(kill);
	 }
      }

      map<string, string>::const_iterator const trusted = Options.find("trusted");

      for (vector<metaIndex *>::const_iterator I = List.begin();
	   I != List.end(); ++I)
      {
	 // We only worry about debian entries here
	 if (strcmp((*I)->GetType(), "deb") != 0)
	    continue;

	 debReleaseIndex *Deb = (debReleaseIndex *) (*I);
	 if (trusted != Options.end())
	    Deb->SetTrusted(StringToBool(trusted->second, false));

	 /* This check insures that there will be only one Release file
	    queued for all the Packages files and Sources files it
	    corresponds to. */
	 if (Deb->GetURI() == URI && Deb->GetDist() == Dist)
	 {
	    if (IsSrc == true)
	       Deb->PushSectionEntry("source", new debReleaseIndex::debSectionEntry(Section, IsSrc));
	    else
	    {
	       if (Dist[Dist.size() - 1] == '/')
		  Deb->PushSectionEntry("any", new debReleaseIndex::debSectionEntry(Section, IsSrc));
	       else
		  Deb->PushSectionEntry(Archs, new debReleaseIndex::debSectionEntry(Section, IsSrc));
	    }
	    return true;
	 }
      }

      // No currently created Release file indexes this entry, so we create a new one.
      debReleaseIndex *Deb;
      if (trusted != Options.end())
	 Deb = new debReleaseIndex(URI, Dist, StringToBool(trusted->second, false));
      else
	 Deb = new debReleaseIndex(URI, Dist);

      if (IsSrc == true)
	 Deb->PushSectionEntry ("source", new debReleaseIndex::debSectionEntry(Section, IsSrc));
      else
      {
	 if (Dist[Dist.size() - 1] == '/')
	    Deb->PushSectionEntry ("any", new debReleaseIndex::debSectionEntry(Section, IsSrc));
	 else
	    Deb->PushSectionEntry (Archs, new debReleaseIndex::debSectionEntry(Section, IsSrc));
      }
      List.push_back(Deb);
      return true;
   }
};

debDebFileMetaIndex::debDebFileMetaIndex(std::string const &DebFile)
   : metaIndex(DebFile, "local-uri", "deb-dist"), DebFile(DebFile)
{
   DebIndex = new debDebPkgFileIndex(DebFile);
   Indexes = new vector<pkgIndexFile *>();
   Indexes->push_back(DebIndex);
}


class APT_HIDDEN debSLTypeDeb : public debSLTypeDebian
{
   public:

   bool CreateItem(vector<metaIndex *> &List, string const &URI,
		   string const &Dist, string const &Section,
		   std::map<string, string> const &Options) const
   {
      return CreateItemInternal(List, URI, Dist, Section, false, Options);
   }

   debSLTypeDeb()
   {
      Name = "deb";
      Label = "Standard Debian binary tree";
   }   
};

class APT_HIDDEN debSLTypeDebSrc : public debSLTypeDebian
{
   public:

   bool CreateItem(vector<metaIndex *> &List, string const &URI,
		   string const &Dist, string const &Section,
		   std::map<string, string> const &Options) const
   {
      return CreateItemInternal(List, URI, Dist, Section, true, Options);
   }
   
   debSLTypeDebSrc()
   {
      Name = "deb-src";
      Label = "Standard Debian source tree";
   }   
};

class APT_HIDDEN debSLTypeDebFile : public pkgSourceList::Type
{
   public:

   bool CreateItem(vector<metaIndex *> &List, string const &URI,
		   string const &/*Dist*/, string const &/*Section*/,
		   std::map<string, string> const &/*Options*/) const
   {
      metaIndex *mi = new debDebFileMetaIndex(URI);
      List.push_back(mi);
      return true;
   }
   
   debSLTypeDebFile()
   {
      Name = "deb-file";
      Label = "Debian Deb File";
   }   
};

APT_HIDDEN debSLTypeDeb _apt_DebType;
APT_HIDDEN debSLTypeDebSrc _apt_DebSrcType;
APT_HIDDEN debSLTypeDebFile _apt_DebFileType;

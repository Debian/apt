// ijones, walters

#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/error.h>

#include <set>

using namespace std;

string debReleaseIndex::Info(const char *Type, string const &Section, string const &Arch) const
{
   string Info = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
         Info += Dist;
   }
   else
   {
      Info += Dist + '/' + Section;
      if (Arch.empty() != true)
	 Info += " " + Arch;
   }
   Info += " ";
   Info += Type;
   return Info;
}

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

string debReleaseIndex::IndexURISuffix(const char *Type, string const &Section, string const &Arch) const
{
   string Res ="";
   if (Dist[Dist.size() - 1] != '/')
   {
      if (Arch == "native")
	 Res += Section + "/binary-" + _config->Find("APT::Architecture") + '/';
      else
	 Res += Section + "/binary-" + Arch + '/';
   }
   return Res + Type;
}
   

string debReleaseIndex::IndexURI(const char *Type, string const &Section, string const &Arch) const
{
   if (Dist[Dist.size() - 1] == '/')
   {
      string Res;
      if (Dist != "/")
         Res = URI + Dist;
      else 
         Res = URI;
      return Res + Type;
   }
   else
      return URI + "dists/" + Dist + '/' + IndexURISuffix(Type, Section, Arch);
 }

string debReleaseIndex::SourceIndexURISuffix(const char *Type, const string &Section) const
{
   string Res ="";
   if (Dist[Dist.size() - 1] != '/')
      Res += Section + "/source/";
   return Res + Type;
}

string debReleaseIndex::SourceIndexURI(const char *Type, const string &Section) const
{
   string Res;
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
         Res = URI + Dist;
      else 
         Res = URI;
      return Res + Type;
   }
   else
      return URI + "dists/" + Dist + "/" + SourceIndexURISuffix(Type, Section);
}

debReleaseIndex::debReleaseIndex(string const &URI, string const &Dist) {
	this->URI = URI;
	this->Dist = Dist;
	this->Indexes = NULL;
	this->Type = "deb";
}

debReleaseIndex::~debReleaseIndex() {
	for (map<string, vector<debSectionEntry const*> >::const_iterator A = ArchEntries.begin();
	     A != ArchEntries.end(); ++A)
		for (vector<const debSectionEntry *>::const_iterator S = A->second.begin();
		     S != A->second.end(); ++S)
			delete *S;
}

vector <struct IndexTarget *>* debReleaseIndex::ComputeIndexTargets() const {
	vector <struct IndexTarget *>* IndexTargets = new vector <IndexTarget *>;

	map<string, vector<debSectionEntry const*> >::const_iterator const src = ArchEntries.find("source");
	if (src != ArchEntries.end()) {
		vector<debSectionEntry const*> const SectionEntries = src->second;
		for (vector<debSectionEntry const*>::const_iterator I = SectionEntries.begin();
		     I != SectionEntries.end(); ++I) {
			IndexTarget * Target = new IndexTarget();
			Target->ShortDesc = "Sources";
			Target->MetaKey = SourceIndexURISuffix(Target->ShortDesc.c_str(), (*I)->Section);
			Target->URI = SourceIndexURI(Target->ShortDesc.c_str(), (*I)->Section);
			Target->Description = Info (Target->ShortDesc.c_str(), (*I)->Section);
			IndexTargets->push_back (Target);
		}
	}

	// Only source release
	if (IndexTargets->empty() == false && ArchEntries.size() == 1)
		return IndexTargets;

	for (map<string, vector<debSectionEntry const*> >::const_iterator a = ArchEntries.begin();
	     a != ArchEntries.end(); ++a) {
		if (a->first == "source")
			continue;
		for (vector <const debSectionEntry *>::const_iterator I = a->second.begin();
		     I != a->second.end(); ++I) {
			IndexTarget * Target = new IndexTarget();
			Target->ShortDesc = "Packages";
			Target->MetaKey = IndexURISuffix(Target->ShortDesc.c_str(), (*I)->Section, a->first);
			Target->URI = IndexURI(Target->ShortDesc.c_str(), (*I)->Section, a->first);
			Target->Description = Info (Target->ShortDesc.c_str(), (*I)->Section, a->first);
			IndexTargets->push_back (Target);
		}
	}

	return IndexTargets;
}
									/*}}}*/
bool debReleaseIndex::GetIndexes(pkgAcquire *Owner, bool const &GetAll) const
{
   // special case for --print-uris
   if (GetAll) {
      vector <struct IndexTarget *> *targets = ComputeIndexTargets();
      for (vector <struct IndexTarget*>::const_iterator Target = targets->begin(); Target != targets->end(); Target++) {
	 new pkgAcqIndex(Owner, (*Target)->URI, (*Target)->Description,
			 (*Target)->ShortDesc, HashString());
      }
      // this is normally created in pkgAcqMetaSig, but if we run
      // in --print-uris mode, we add it here
      new pkgAcqMetaIndex(Owner, MetaIndexURI("Release"),
		     MetaIndexInfo("Release"), "Release",
		     MetaIndexURI("Release.gpg"), 
		     ComputeIndexTargets(),
		     new indexRecords (Dist));

   }

   new pkgAcqMetaSig(Owner, MetaIndexURI("Release.gpg"),
		     MetaIndexInfo("Release.gpg"), "Release.gpg",
		     MetaIndexURI("Release"), MetaIndexInfo("Release"), "Release",
		     ComputeIndexTargets(),
		     new indexRecords (Dist));

	// Queue the translations
	std::vector<std::string> const lang = APT::Configuration::getLanguages(true);
	map<string, set<string> > sections;
	for (map<string, vector<debSectionEntry const*> >::const_iterator a = ArchEntries.begin();
	     a != ArchEntries.end(); ++a) {
		if (a->first == "source")
			continue;
		for (vector<debSectionEntry const*>::const_iterator I = a->second.begin();
		     I != a->second.end(); I++)
			sections[(*I)->Section].insert(lang.begin(), lang.end());
	}

	for (map<string, set<string> >::const_iterator s = sections.begin();
	     s != sections.end(); ++s)
		for (set<string>::const_iterator l = s->second.begin();
		     l != s->second.end(); l++) {
			if (*l == "none") continue;
			debTranslationsIndex i = debTranslationsIndex(URI,Dist,s->first,(*l).c_str());
			i.GetIndexes(Owner);
		}

	return true;
}

bool debReleaseIndex::IsTrusted() const
{
   string VerifiedSigFile = _config->FindDir("Dir::State::lists") +
      URItoFileName(MetaIndexURI("Release")) + ".gpg";
   
   if(_config->FindB("APT::Authentication::TrustCDROM", false))
      if(URI.substr(0,strlen("cdrom:")) == "cdrom:")
	 return true;
   
   if (FileExists(VerifiedSigFile))
      return true;
   return false;
}

vector <pkgIndexFile *> *debReleaseIndex::GetIndexFiles() {
	if (Indexes != NULL)
		return Indexes;

	Indexes = new vector <pkgIndexFile*>;
	map<string, vector<debSectionEntry const*> >::const_iterator const src = ArchEntries.find("source");
	if (src != ArchEntries.end()) {
		vector<debSectionEntry const*> const SectionEntries = src->second;
		for (vector<debSectionEntry const*>::const_iterator I = SectionEntries.begin();
		     I != SectionEntries.end(); I++)
			Indexes->push_back(new debSourcesIndex (URI, Dist, (*I)->Section, IsTrusted()));
	}

	// Only source release
	if (Indexes->empty() == false && ArchEntries.size() == 1)
		return Indexes;

	std::vector<std::string> const lang = APT::Configuration::getLanguages(true);
	map<string, set<string> > sections;
	for (map<string, vector<debSectionEntry const*> >::const_iterator a = ArchEntries.begin();
	     a != ArchEntries.end(); ++a) {
		if (a->first == "source")
			continue;
		for (vector<debSectionEntry const*>::const_iterator I = a->second.begin();
		     I != a->second.end(); I++) {
			Indexes->push_back(new debPackagesIndex (URI, Dist, (*I)->Section, IsTrusted(), a->first));
			sections[(*I)->Section].insert(lang.begin(), lang.end());
		}
	}

	for (map<string, set<string> >::const_iterator s = sections.begin();
	     s != sections.end(); ++s)
		for (set<string>::const_iterator l = s->second.begin();
		     l != s->second.end(); l++) {
			if (*l == "none") continue;
			Indexes->push_back(new debTranslationsIndex(URI,Dist,s->first,(*l).c_str()));
		}

	return Indexes;
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

void debReleaseIndex::PushSectionEntry(const debSectionEntry *Entry) {
	if (Entry->IsSrc == true)
		PushSectionEntry("source", Entry);
	else {
		for (map<string, vector<const debSectionEntry *> >::iterator a = ArchEntries.begin();
		     a != ArchEntries.end(); ++a) {
			a->second.push_back(Entry);
		}
	}
}

debReleaseIndex::debSectionEntry::debSectionEntry (string const &Section,
		bool const &IsSrc): Section(Section), IsSrc(IsSrc)
{}

class debSLTypeDebian : public pkgSourceList::Type
{
   protected:

   bool CreateItemInternal(vector<metaIndex *> &List, string const &URI,
			   string const &Dist, string const &Section,
			   bool const &IsSrc, map<string, string> const &Options) const
   {
      map<string, string>::const_iterator const arch = Options.find("arch");
      vector<string> const Archs =
		(arch != Options.end()) ? VectorizeString(arch->second, ',') :
				APT::Configuration::getArchitectures();

      for (vector<metaIndex *>::const_iterator I = List.begin();
	   I != List.end(); I++)
      {
	 // We only worry about debian entries here
	 if (strcmp((*I)->GetType(), "deb") != 0)
	    continue;

	 debReleaseIndex *Deb = (debReleaseIndex *) (*I);
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
      // XXX determine whether this release is trusted or not
      debReleaseIndex *Deb = new debReleaseIndex(URI, Dist);
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

class debSLTypeDeb : public debSLTypeDebian
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

class debSLTypeDebSrc : public debSLTypeDebian
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

debSLTypeDeb _apt_DebType;
debSLTypeDebSrc _apt_DebSrcType;

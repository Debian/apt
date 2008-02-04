// ijones, walters

#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>

using namespace std;

string debReleaseIndex::Info(const char *Type, const string Section) const
{
   string Info = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
         Info += Dist;
   }
   else
      Info += Dist + '/' + Section;   
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

string debReleaseIndex::IndexURISuffix(const char *Type, const string Section) const
{
   string Res ="";
   if (Dist[Dist.size() - 1] != '/')
      Res += Section + "/binary-" + _config->Find("APT::Architecture") + '/';
   return Res + Type;
}
   

string debReleaseIndex::IndexURI(const char *Type, const string Section) const
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
      return URI + "dists/" + Dist + '/' + IndexURISuffix(Type, Section);
 }

string debReleaseIndex::SourceIndexURISuffix(const char *Type, const string Section) const
{
   string Res ="";
   if (Dist[Dist.size() - 1] != '/')
      Res += Section + "/source/";
   return Res + Type;
}

string debReleaseIndex::SourceIndexURI(const char *Type, const string Section) const
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

debReleaseIndex::debReleaseIndex(string URI,string Dist)
{
   this->URI = URI;
   this->Dist = Dist;
   this->Indexes = NULL;
   this->Type = "deb";
}

vector <struct IndexTarget *>* debReleaseIndex::ComputeIndexTargets() const
{
   vector <struct IndexTarget *>* IndexTargets = new vector <IndexTarget *>;
   for (vector <const debSectionEntry *>::const_iterator I = SectionEntries.begin();
	I != SectionEntries.end();
	I++)
   {
      IndexTarget * Target = new IndexTarget();
      Target->ShortDesc = (*I)->IsSrc ? "Sources" : "Packages";
      Target->MetaKey
	= (*I)->IsSrc ? SourceIndexURISuffix(Target->ShortDesc.c_str(), (*I)->Section)
	              : IndexURISuffix(Target->ShortDesc.c_str(), (*I)->Section);
      Target->URI 
	= (*I)->IsSrc ? SourceIndexURI(Target->ShortDesc.c_str(), (*I)->Section)
	              : IndexURI(Target->ShortDesc.c_str(), (*I)->Section);
      
      Target->Description = Info (Target->ShortDesc.c_str(), (*I)->Section);
      IndexTargets->push_back (Target);
   }
   return IndexTargets;
}
									/*}}}*/
bool debReleaseIndex::GetIndexes(pkgAcquire *Owner, bool GetAll) const
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
   for (vector<const debSectionEntry *>::const_iterator I = SectionEntries.begin(); 
	I != SectionEntries.end(); I++) {

      if((*I)->IsSrc)
	 continue;
      debTranslationsIndex i = debTranslationsIndex(URI,Dist,(*I)->Section);
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

vector <pkgIndexFile *> *debReleaseIndex::GetIndexFiles()
{
   if (Indexes != NULL)
      return Indexes;

   Indexes = new vector <pkgIndexFile*>;
   for (vector<const debSectionEntry *>::const_iterator I = SectionEntries.begin(); 
	I != SectionEntries.end(); I++) {
      if ((*I)->IsSrc)
         Indexes->push_back(new debSourcesIndex (URI, Dist, (*I)->Section, IsTrusted()));
      else 
      {
         Indexes->push_back(new debPackagesIndex (URI, Dist, (*I)->Section, IsTrusted()));
	 Indexes->push_back(new debTranslationsIndex(URI, Dist, (*I)->Section));
      }
   }

   return Indexes;
}

void debReleaseIndex::PushSectionEntry(const debSectionEntry *Entry)
{
   SectionEntries.push_back(Entry);
}

debReleaseIndex::debSectionEntry::debSectionEntry (string Section, bool IsSrc): Section(Section)
{
   this->IsSrc = IsSrc;
}

class debSLTypeDebian : public pkgSourceList::Type
{
   protected:

   bool CreateItemInternal(vector<metaIndex *> &List,string URI,
			   string Dist,string Section,
			   bool IsSrc) const
   {
      for (vector<metaIndex *>::const_iterator I = List.begin(); 
	   I != List.end(); I++)
      {
	 // This check insures that there will be only one Release file
	 // queued for all the Packages files and Sources files it
	 // corresponds to.
		if (strcmp((*I)->GetType(), "deb") == 0)
	 {
	    debReleaseIndex *Deb = (debReleaseIndex *) (*I);
	    // This check insures that there will be only one Release file
	    // queued for all the Packages files and Sources files it
	    // corresponds to.
	    if (Deb->GetURI() == URI && Deb->GetDist() == Dist)
	    {
	       Deb->PushSectionEntry(new debReleaseIndex::debSectionEntry(Section, IsSrc));
	       return true;
	    }
	 }
      }
      // No currently created Release file indexes this entry, so we create a new one.
      // XXX determine whether this release is trusted or not
      debReleaseIndex *Deb = new debReleaseIndex(URI,Dist);
      Deb->PushSectionEntry (new debReleaseIndex::debSectionEntry(Section, IsSrc));
      List.push_back(Deb);
      return true;
   }
};

class debSLTypeDeb : public debSLTypeDebian
{
   public:

   bool CreateItem(vector<metaIndex *> &List,string URI,
		   string Dist,string Section) const
   {
      return CreateItemInternal(List, URI, Dist, Section, false);
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

   bool CreateItem(vector<metaIndex *> &List,string URI,
		   string Dist,string Section) const 
   {
      return CreateItemInternal(List, URI, Dist, Section, true);
   }
   
   debSLTypeDebSrc()
   {
      Name = "deb-src";
      Label = "Standard Debian source tree";
   }   
};

debSLTypeDeb _apt_DebType;
debSLTypeDebSrc _apt_DebSrcType;

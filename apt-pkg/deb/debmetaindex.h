// ijones, walters
#ifndef PKGLIB_DEBMETAINDEX_H
#define PKGLIB_DEBMETAINDEX_H

#include <apt-pkg/metaindex.h>
#include <apt-pkg/sourcelist.h>

class debReleaseIndex : public metaIndex {
   public:

   class debSectionEntry
   {
      public:
      debSectionEntry (string Section, bool IsSrc);
      bool IsSrc;
      string Section;
   };

   private:
   vector <const debSectionEntry *> SectionEntries;

   public:

   debReleaseIndex(string URI, string Dist);

   virtual string ArchiveURI(string File) const {return URI + File;};
   virtual bool GetIndexes(pkgAcquire *Owner, bool GetAll=false) const;
   vector <struct IndexTarget *>* ComputeIndexTargets() const;
   string Info(const char *Type, const string Section) const;
   string MetaIndexInfo(const char *Type) const;
   string MetaIndexFile(const char *Types) const;
   string MetaIndexURI(const char *Type) const;
   string IndexURI(const char *Type, const string Section) const;
   string IndexURISuffix(const char *Type, const string Section) const;
   string SourceIndexURI(const char *Type, const string Section) const;
   string SourceIndexURISuffix(const char *Type, const string Section) const;
   virtual vector <pkgIndexFile *> *GetIndexFiles();

   virtual bool IsTrusted() const;

   void PushSectionEntry(const debSectionEntry *Entry);
};

#endif

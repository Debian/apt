// ijones, walters
#ifndef PKGLIB_DEBMETAINDEX_H
#define PKGLIB_DEBMETAINDEX_H

#include <apt-pkg/metaindex.h>

#include <map>
#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/sourcelist.h>
#endif

class debReleaseIndex : public metaIndex {
   public:

   class debSectionEntry
   {
      public:
      debSectionEntry (std::string const &Section, bool const &IsSrc);
      std::string const Section;
      bool const IsSrc;
   };

   private:
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;
   std::map<std::string, std::vector<debSectionEntry const*> > ArchEntries;
   enum { ALWAYS_TRUSTED, NEVER_TRUSTED, CHECK_TRUST } Trusted;

   public:

   debReleaseIndex(std::string const &URI, std::string const &Dist);
   debReleaseIndex(std::string const &URI, std::string const &Dist, bool const Trusted);
   virtual ~debReleaseIndex();

   virtual std::string ArchiveURI(std::string const &File) const {return URI + File;};
   virtual bool GetIndexes(pkgAcquire *Owner, bool const &GetAll=false) const;
   std::vector <struct IndexTarget *>* ComputeIndexTargets() const;
   std::string Info(const char *Type, std::string const &Section, std::string const &Arch="") const;
   std::string MetaIndexInfo(const char *Type) const;
   std::string MetaIndexFile(const char *Types) const;
   std::string MetaIndexURI(const char *Type) const;
   std::string IndexURI(const char *Type, std::string const &Section, std::string const &Arch="native") const;
   std::string IndexURISuffix(const char *Type, std::string const &Section, std::string const &Arch="native") const;
   std::string SourceIndexURI(const char *Type, const std::string &Section) const;
   std::string SourceIndexURISuffix(const char *Type, const std::string &Section) const;
   std::string TranslationIndexURI(const char *Type, const std::string &Section) const;
   std::string TranslationIndexURISuffix(const char *Type, const std::string &Section) const;
   virtual std::vector <pkgIndexFile *> *GetIndexFiles();

   void SetTrusted(bool const Trusted);
   virtual bool IsTrusted() const;

   void PushSectionEntry(std::vector<std::string> const &Archs, const debSectionEntry *Entry);
   void PushSectionEntry(std::string const &Arch, const debSectionEntry *Entry);
   void PushSectionEntry(const debSectionEntry *Entry);
};

#endif

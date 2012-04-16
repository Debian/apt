// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debindexfile.h,v 1.3.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Debian Index Files
   
   There are three sorts currently
   
   Package files that have File: tags
   Package files that don't (/var/lib/dpkg/status)
   Source files
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBINDEXFILE_H
#define PKGLIB_DEBINDEXFILE_H



#include <apt-pkg/indexfile.h>

class debStatusIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   protected:
   std::string File;

   public:

   virtual const Type *GetType() const;
   
   // Interface for acquire
   virtual std::string Describe(bool Short) const {return File;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return true;};
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog, unsigned long const Flag) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debStatusIndex(std::string File);
   virtual ~debStatusIndex() {};
};
    
class debPackagesIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;
   std::string Architecture;

   std::string Info(const char *Type) const;
   std::string IndexFile(const char *Type) const;
   std::string IndexURI(const char *Type) const;
   
   public:
   
   virtual const Type *GetType() const;

   // Stuff for accessing files on remote items
   virtual std::string ArchiveInfo(pkgCache::VerIterator Ver) const;
   virtual std::string ArchiveURI(std::string File) const {return URI + File;};
   
   // Interface for acquire
   virtual std::string Describe(bool Short) const;   
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return true;};
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debPackagesIndex(std::string const &URI, std::string const &Dist, std::string const &Section,
			bool const &Trusted, std::string const &Arch = "native");
   virtual ~debPackagesIndex() {};
};

class debTranslationsIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;
   const char * const Language;
   
   std::string Info(const char *Type) const;
   std::string IndexFile(const char *Type) const;
   std::string IndexURI(const char *Type) const;

   inline std::string TranslationFile() const {return std::string("Translation-").append(Language);};

   public:
   
   virtual const Type *GetType() const;

   // Interface for acquire
   virtual std::string Describe(bool Short) const;   
   virtual bool GetIndexes(pkgAcquire *Owner) const;
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const;
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debTranslationsIndex(std::string URI,std::string Dist,std::string Section, char const * const Language);
   virtual ~debTranslationsIndex() {};
};

class debSourcesIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;

   std::string Info(const char *Type) const;
   std::string IndexFile(const char *Type) const;
   std::string IndexURI(const char *Type) const;
   
   public:

   virtual const Type *GetType() const;

   // Stuff for accessing files on remote items
   virtual std::string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const;
   virtual std::string ArchiveURI(std::string File) const {return URI + File;};
   
   // Interface for acquire
   virtual std::string Describe(bool Short) const;   

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const;
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return false;};
   virtual unsigned long Size() const;
   
   debSourcesIndex(std::string URI,std::string Dist,std::string Section,bool Trusted);
   virtual ~debSourcesIndex() {};
};

#endif

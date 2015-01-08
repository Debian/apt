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
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/srcrecords.h>

#include <string>

class OpProgress;
class pkgAcquire;
class pkgCacheGenerator;


class APT_HIDDEN debStatusIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   protected:
   std::string File;

   public:

   virtual const Type *GetType() const APT_CONST;
   
   // Interface for acquire
   virtual std::string Describe(bool /*Short*/) const {return File;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return true;};
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog, unsigned long const Flag) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debStatusIndex(std::string File);
   virtual ~debStatusIndex();
};
    
class APT_HIDDEN debPackagesIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;
   std::string Architecture;

   APT_HIDDEN std::string Info(const char *Type) const;
   APT_HIDDEN std::string IndexFile(const char *Type) const;
   APT_HIDDEN std::string IndexURI(const char *Type) const;

   public:
   
   virtual const Type *GetType() const APT_CONST;

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
   virtual ~debPackagesIndex();
};

class APT_HIDDEN debTranslationsIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;
   const char * const Language;
   
   APT_HIDDEN std::string Info(const char *Type) const;
   APT_HIDDEN std::string IndexFile(const char *Type) const;
   APT_HIDDEN std::string IndexURI(const char *Type) const;

   APT_HIDDEN std::string TranslationFile() const {return std::string("Translation-").append(Language);};

   public:
   
   virtual const Type *GetType() const APT_CONST;

   // Interface for acquire
   virtual std::string Describe(bool Short) const;   
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const;
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debTranslationsIndex(std::string URI,std::string Dist,std::string Section, char const * const Language);
   virtual ~debTranslationsIndex();
};

class APT_HIDDEN debSourcesIndex : public pkgIndexFile
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   std::string URI;
   std::string Dist;
   std::string Section;

   APT_HIDDEN std::string Info(const char *Type) const;
   APT_HIDDEN std::string IndexFile(const char *Type) const;
   APT_HIDDEN std::string IndexURI(const char *Type) const;

   public:

   virtual const Type *GetType() const APT_CONST;

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
   virtual ~debSourcesIndex();
};

class APT_HIDDEN debDebPkgFileIndex : public pkgIndexFile
{
 private:
   void *d;
   std::string DebFile;
   std::string DebFileFullPath;

 public:
   virtual const Type *GetType() const APT_CONST;

   virtual std::string Describe(bool /*Short*/) const {
      return DebFile;
   }

   /** get the control (file) content of the deb file
    *
    * @param[out] content of the control file
    * @param debfile is the filename of the .deb-file
    * @return \b true if successful, otherwise \b false.
    */
   static bool GetContent(std::ostream &content, std::string const &debfile);

   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {
      return true;
   };
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   // Interface for acquire
   virtual std::string ArchiveURI(std::string /*File*/) const;

   debDebPkgFileIndex(std::string DebFile);
   virtual ~debDebPkgFileIndex();
};

class APT_HIDDEN debDscFileIndex : public pkgIndexFile
{
 private:
   std::string DscFile;
 public:
   virtual const Type *GetType() const APT_CONST;
   virtual pkgSrcRecords::Parser *CreateSrcParser() const;
   virtual bool Exists() const;
   virtual bool HasPackages() const {return false;};
   virtual unsigned long Size() const;
   virtual std::string Describe(bool /*Short*/) const {
      return DscFile;
   };

   debDscFileIndex(std::string &DscFile);
   virtual ~debDscFileIndex() {};
};

class APT_HIDDEN debDebianSourceDirIndex : public debDscFileIndex
{
 public:
   virtual const Type *GetType() const APT_CONST;
};

#endif

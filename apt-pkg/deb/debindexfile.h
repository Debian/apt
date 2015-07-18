// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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


class debStatusIndex : public pkgIndexFile
{
   void * const d;
   protected:
   std::string File;

   public:

   virtual const Type *GetType() const APT_CONST;

   // Interface for acquire
   virtual std::string Describe(bool /*Short*/) const APT_OVERRIDE {return File;};

   // Interface for the Cache Generator
   virtual bool Exists() const APT_OVERRIDE;
   virtual bool HasPackages() const APT_OVERRIDE {return true;};
   virtual unsigned long Size() const APT_OVERRIDE;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const APT_OVERRIDE;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const APT_OVERRIDE;

   debStatusIndex(std::string File);
   virtual ~debStatusIndex();
};

class debPackagesIndex : public pkgIndexTargetFile
{
   void * const d;
   public:

   virtual const Type *GetType() const APT_CONST;

   // Stuff for accessing files on remote items
   virtual std::string ArchiveInfo(pkgCache::VerIterator Ver) const APT_OVERRIDE;

   // Interface for the Cache Generator
   virtual bool HasPackages() const APT_OVERRIDE {return true;};
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const APT_OVERRIDE;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const APT_OVERRIDE;

   debPackagesIndex(IndexTarget const &Target, bool const Trusted);
   virtual ~debPackagesIndex();
};

class debTranslationsIndex : public pkgIndexTargetFile
{
   void * const d;
   public:

   virtual const Type *GetType() const APT_CONST;

   // Interface for the Cache Generator
   virtual bool HasPackages() const APT_OVERRIDE;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const APT_OVERRIDE;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const APT_OVERRIDE;

   debTranslationsIndex(IndexTarget const &Target);
   virtual ~debTranslationsIndex();
};

class debSourcesIndex : public pkgIndexTargetFile
{
   void * const d;
   public:

   virtual const Type *GetType() const APT_CONST;

   // Stuff for accessing files on remote items
   virtual std::string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const APT_OVERRIDE;

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const APT_OVERRIDE;

   // Interface for the Cache Generator
   virtual bool HasPackages() const APT_OVERRIDE {return false;};

   debSourcesIndex(IndexTarget const &Target, bool const Trusted);
   virtual ~debSourcesIndex();
};

class debDebPkgFileIndex : public pkgIndexFile
{
 private:
   void * const d;
   std::string DebFile;
   std::string DebFileFullPath;

 public:
   virtual const Type *GetType() const APT_CONST;

   virtual std::string Describe(bool /*Short*/) const APT_OVERRIDE {
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
   virtual bool Exists() const APT_OVERRIDE;
   virtual bool HasPackages() const APT_OVERRIDE {
      return true;
   };
   virtual unsigned long Size() const APT_OVERRIDE;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const APT_OVERRIDE;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const APT_OVERRIDE;

   // Interface for acquire
   virtual std::string ArchiveURI(std::string /*File*/) const APT_OVERRIDE;

   debDebPkgFileIndex(std::string const &DebFile);
   virtual ~debDebPkgFileIndex();
};

class debDscFileIndex : public pkgIndexFile
{
 private:
   void * const d;
   std::string DscFile;
 public:
   virtual const Type *GetType() const APT_CONST;
   virtual pkgSrcRecords::Parser *CreateSrcParser() const APT_OVERRIDE;
   virtual bool Exists() const APT_OVERRIDE;
   virtual bool HasPackages() const APT_OVERRIDE {return false;};
   virtual unsigned long Size() const APT_OVERRIDE;
   virtual std::string Describe(bool /*Short*/) const APT_OVERRIDE {
      return DscFile;
   };

   debDscFileIndex(std::string const &DscFile);
   virtual ~debDscFileIndex();
};

class debDebianSourceDirIndex : public debDscFileIndex
{
 public:
   virtual const Type *GetType() const APT_CONST;
};

#endif

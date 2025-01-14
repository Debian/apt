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
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/srcrecords.h>

#include <string>

class OpProgress;
class pkgAcquire;
class pkgCacheGenerator;

class debStatusIndex : public pkgDebianIndexRealFile
{
   void * const d;
protected:
   [[nodiscard]] std::string GetArchitecture() const override;
   [[nodiscard]] std::string GetComponent() const override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;

public:

   [[nodiscard]] const Type *GetType() const override APT_PURE;

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override { return true; };
   // Abort if the file does not exist.
   [[nodiscard]] bool Exists() const override { return true; };

   pkgCacheListParser *CreateListParser(FileFd &Pkg) override;

   explicit debStatusIndex(std::string const &File);
   ~debStatusIndex() override;
};

class debPackagesIndex : public pkgDebianIndexTargetFile
{
   void * const d;
protected:
   [[nodiscard]] uint8_t GetIndexFlags() const override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   // Stuff for accessing files on remote items
   [[nodiscard]] std::string ArchiveInfo(pkgCache::VerIterator const &Ver) const override;

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override {return true;};

   debPackagesIndex(IndexTarget const &Target, bool const Trusted);
   ~debPackagesIndex() override;
};

class debTranslationsIndex : public pkgDebianIndexTargetFile
{
   void * const d;
protected:
   [[nodiscard]] std::string GetArchitecture() const override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;
   bool OpenListFile(FileFd &Pkg, std::string const &FileName) override;
   APT_HIDDEN pkgCacheListParser *CreateListParser(FileFd &Pkg) override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override;

   explicit debTranslationsIndex(IndexTarget const &Target);
   ~debTranslationsIndex() override;
};

class debSourcesIndex : public pkgDebianIndexTargetFile
{
   void * const d;
   [[nodiscard]] uint8_t GetIndexFlags() const override;
   bool OpenListFile(FileFd &Pkg, std::string const &FileName) override;
   APT_HIDDEN pkgCacheListParser *CreateListParser(FileFd &Pkg) override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   // Stuff for accessing files on remote items
   [[nodiscard]] std::string SourceInfo(pkgSrcRecords::Parser const &Record,
					pkgSrcRecords::File const &File) const override;

   // Interface for the record parsers
   [[nodiscard]] pkgSrcRecords::Parser *CreateSrcParser() const override;

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override { return false; };

   debSourcesIndex(IndexTarget const &Target, bool const Trusted);
   ~debSourcesIndex() override;
};

class debDebPkgFileIndex : public pkgDebianIndexRealFile
{
   void * const d;
   std::string DebFile;

protected:
   [[nodiscard]] std::string GetComponent() const override;
   [[nodiscard]] std::string GetArchitecture() const override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;
   bool OpenListFile(FileFd &Pkg, std::string const &FileName) override;
   APT_HIDDEN pkgCacheListParser *CreateListParser(FileFd &Pkg) override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   /** get the control (file) content of the deb file
    *
    * @param[out] content of the control file
    * @param debfile is the filename of the .deb-file
    * @return \b true if successful, otherwise \b false.
    */
   static bool GetContent(std::ostream &content, std::string const &debfile);

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override { return true; }
   pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const override;

   // Interface for acquire

   explicit debDebPkgFileIndex(std::string const &DebFile);
   ~debDebPkgFileIndex() override;

   [[nodiscard]] std::string ArchiveInfo(pkgCache::VerIterator const &Ver) const override;
};

class APT_PUBLIC debDscFileIndex : public pkgDebianIndexRealFile
{
   void * const d;

protected:
   [[nodiscard]] std::string GetComponent() const override;
   [[nodiscard]] std::string GetArchitecture() const override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;

public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;
   [[nodiscard]] pkgSrcRecords::Parser *CreateSrcParser() const override;
   [[nodiscard]] bool HasPackages() const override { return false; };

   explicit debDscFileIndex(std::string const &DscFile);
   virtual ~debDscFileIndex();
};

class debDebianSourceDirIndex : public debDscFileIndex
{
protected:
   [[nodiscard]] std::string GetComponent() const override;

public:
[[nodiscard]] const Type *GetType() const override APT_PURE;
};

class APT_PUBLIC debStringPackageIndex : public pkgDebianIndexRealFile
{
   void * const d;
protected:
   [[nodiscard]] std::string GetArchitecture() const override;
   [[nodiscard]] std::string GetComponent() const override;
   [[nodiscard]] uint8_t GetIndexFlags() const override;

   public:
   [[nodiscard]] const Type *GetType() const override APT_PURE;

   // Interface for the Cache Generator
   [[nodiscard]] bool HasPackages() const override { return true; };
   // Abort if the file does not exist.
   [[nodiscard]] bool Exists() const override { return true; };

   explicit debStringPackageIndex(std::string const &content);
   ~debStringPackageIndex() override;
};
#endif

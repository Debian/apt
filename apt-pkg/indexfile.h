// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Index File - Abstraction for an index of archive/source file.
   
   There are 4 primary sorts of index files, all represented by this 
   class:
   
   Binary index files 
   Binary translation files 
   Binary index files describing the local system
   Source index files
   
   They are all bundled together here, and the interfaces for 
   sources.list, acquire, cache gen and record parsing all use this class
   to access the underlying representation.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_INDEXFILE_H
#define PKGLIB_INDEXFILE_H

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>

#include <map>
#include <string>


class pkgCacheGenerator;
class pkgCacheListParser;
class OpProgress;

class APT_PUBLIC IndexTarget							/*{{{*/
/** \brief Information about an index file. */
{
   public:
   /** \brief A URI from which the index file can be downloaded. */
   std::string URI;

   /** \brief A description of the index file. */
   std::string Description;

   /** \brief A shorter description of the index file. */
   std::string ShortDesc;

   /** \brief The key by which this index file should be
       looked up within the meta index file. */
   std::string MetaKey;

   /** \brief Is it okay if the file isn't found in the meta index */
   bool IsOptional;

   /** \brief If the file is downloaded compressed, do not unpack it */
   bool KeepCompressed;

   /** \brief options with which this target was created
       Prefer the usage of #Option if at all possible.
       Beware: Not all of these options are intended for public use */
   std::map<std::string, std::string> Options;

   IndexTarget(std::string const &MetaKey, std::string const &ShortDesc,
	 std::string const &LongDesc, std::string const &URI, bool const IsOptional,
	 bool const KeepCompressed, std::map<std::string, std::string> const &Options);

   enum OptionKeys
   {
      SITE,
      RELEASE,
      COMPONENT,
      LANGUAGE,
      ARCHITECTURE,
      BASE_URI,
      REPO_URI,
      CREATED_BY,
      TARGET_OF,
      FILENAME,
      EXISTING_FILENAME,
      PDIFFS,
      COMPRESSIONTYPES,
      DEFAULTENABLED,
      SOURCESENTRY,
      BY_HASH,
      KEEPCOMPRESSEDAS,
      FALLBACK_OF,
      IDENTIFIER,
      ALLOW_INSECURE,
      ALLOW_WEAK,
      ALLOW_DOWNGRADE_TO_INSECURE,
      INRELEASE_PATH,
      SHADOWED,
   };
   std::string Option(OptionKeys const Key) const;
   bool OptionBool(OptionKeys const Key) const;
   std::string Format(std::string format) const;
};
									/*}}}*/

class APT_PUBLIC pkgIndexFile
{
   void * const d;
   protected:
   bool Trusted;

   public:

   class APT_PUBLIC Type
   {
      public:

      // Global list of Items supported
      static Type **GlobalList;
      static unsigned long GlobalListLen;
      static Type *GetType(const char * const Type) APT_PURE;

      const char *Label;

      virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &/*File*/) const {return 0;};
      virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string const &/*File*/) const {return 0;};
      Type();
      virtual ~Type() {};
   };

   virtual const Type *GetType() const = 0;
   
   // Return descriptive strings of various sorts
   virtual std::string ArchiveInfo(pkgCache::VerIterator const &Ver) const;
   virtual std::string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const;
   virtual std::string Describe(bool const Short = false) const = 0;

   // Interface for acquire
   virtual std::string ArchiveURI(std::string const &/*File*/) const {return std::string();};

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const {return 0;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const = 0;
   virtual bool HasPackages() const = 0;
   virtual unsigned long Size() const = 0;
   virtual bool Merge(pkgCacheGenerator &/*Gen*/, OpProgress* const /*Prog*/) { return true; };
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   bool IsTrusted() const { return Trusted; };

   explicit pkgIndexFile(bool const Trusted);
   virtual ~pkgIndexFile();
};

class APT_PUBLIC pkgDebianIndexFile : public pkgIndexFile
{
protected:
   virtual std::string IndexFileName() const = 0;
   virtual std::string GetComponent() const = 0;
   virtual std::string GetArchitecture() const = 0;
   virtual std::string GetProgressDescription() const = 0;
   virtual uint8_t GetIndexFlags() const = 0;
   virtual bool OpenListFile(FileFd &Pkg, std::string const &FileName) = 0;
   APT_HIDDEN virtual pkgCacheListParser * CreateListParser(FileFd &Pkg);

public:
   virtual bool Merge(pkgCacheGenerator &Gen, OpProgress* const Prog) APT_OVERRIDE;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const APT_OVERRIDE;

   explicit pkgDebianIndexFile(bool const Trusted);
   virtual ~pkgDebianIndexFile();
};

class APT_PUBLIC pkgDebianIndexTargetFile : public pkgDebianIndexFile
{
   void * const d;
protected:
   IndexTarget const Target;

   virtual std::string IndexFileName() const APT_OVERRIDE;
   virtual std::string GetComponent() const APT_OVERRIDE;
   virtual std::string GetArchitecture() const APT_OVERRIDE;
   virtual std::string GetProgressDescription() const APT_OVERRIDE;
   virtual bool OpenListFile(FileFd &Pkg, std::string const &FileName) APT_OVERRIDE;

public:
   virtual std::string ArchiveURI(std::string const &File) const APT_OVERRIDE;
   virtual std::string Describe(bool const Short = false) const APT_OVERRIDE;
   virtual bool Exists() const APT_OVERRIDE;
   virtual unsigned long Size() const APT_OVERRIDE;
   IndexTarget GetIndexTarget() const APT_HIDDEN;

   pkgDebianIndexTargetFile(IndexTarget const &Target, bool const Trusted);
   virtual ~pkgDebianIndexTargetFile();
};

class APT_PUBLIC pkgDebianIndexRealFile : public pkgDebianIndexFile
{
   void * const d;
protected:
   std::string File;

   virtual std::string IndexFileName() const APT_OVERRIDE;
   virtual std::string GetProgressDescription() const APT_OVERRIDE;
   virtual bool OpenListFile(FileFd &Pkg, std::string const &FileName) APT_OVERRIDE;
public:
   virtual std::string Describe(bool const /*Short*/ = false) const APT_OVERRIDE;
   virtual bool Exists() const APT_OVERRIDE;
   virtual unsigned long Size() const APT_OVERRIDE;
   virtual std::string ArchiveURI(std::string const &/*File*/) const APT_OVERRIDE;

   pkgDebianIndexRealFile(std::string const &File, bool const Trusted);
   virtual ~pkgDebianIndexRealFile();
};

#endif

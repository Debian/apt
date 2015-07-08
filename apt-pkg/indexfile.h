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

#include <apt-pkg/srcrecords.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/macros.h>

#include <map>
#include <string>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
#endif
#ifndef APT_10_CLEANER_HEADERS
class pkgAcquire;
#endif

class pkgCacheGenerator;
class OpProgress;

class IndexTarget							/*{{{*/
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

   enum OptionKeys {
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
   };
   std::string Option(OptionKeys const Key) const;
   std::string Format(std::string format) const;
};
									/*}}}*/

class pkgIndexFile
{
   void * const d;
   protected:
   bool Trusted;

   public:

   class Type
   {
      public:

      // Global list of Items supported
      static Type **GlobalList;
      static unsigned long GlobalListLen;
      static Type *GetType(const char *Type) APT_PURE;

      const char *Label;

      virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator /*File*/) const {return 0;};
      virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string /*File*/) const {return 0;};
      Type();
      virtual ~Type() {};
   };

   virtual const Type *GetType() const = 0;
   
   // Return descriptive strings of various sorts
   virtual std::string ArchiveInfo(pkgCache::VerIterator Ver) const;
   virtual std::string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const;
   virtual std::string Describe(bool Short = false) const = 0;

   // Interface for acquire
   virtual std::string ArchiveURI(std::string /*File*/) const {return std::string();};

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const {return 0;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const = 0;
   virtual bool HasPackages() const = 0;
   virtual unsigned long Size() const = 0;
   virtual bool Merge(pkgCacheGenerator &/*Gen*/, OpProgress* /*Prog*/) const { return false; };
   APT_DEPRECATED virtual bool Merge(pkgCacheGenerator &Gen, OpProgress &Prog) const
      { return Merge(Gen, &Prog); };
   virtual bool MergeFileProvides(pkgCacheGenerator &/*Gen*/,OpProgress* /*Prog*/) const {return true;};
   APT_DEPRECATED virtual bool MergeFileProvides(pkgCacheGenerator &Gen, OpProgress &Prog) const
      {return MergeFileProvides(Gen, &Prog);};
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   static bool TranslationsAvailable();
   static bool CheckLanguageCode(const char *Lang);
   static std::string LanguageCode();

   bool IsTrusted() const { return Trusted; };

   explicit pkgIndexFile(bool Trusted);
   virtual ~pkgIndexFile();
};

class pkgIndexTargetFile : public pkgIndexFile
{
   void * const d;
protected:
   IndexTarget const Target;

   std::string IndexFileName() const;

public:
   virtual std::string ArchiveURI(std::string File) const APT_OVERRIDE;
   virtual std::string Describe(bool Short = false) const APT_OVERRIDE;
   virtual bool Exists() const APT_OVERRIDE;
   virtual unsigned long Size() const APT_OVERRIDE;

   pkgIndexTargetFile(IndexTarget const &Target, bool const Trusted);
   virtual ~pkgIndexTargetFile();
};

#endif

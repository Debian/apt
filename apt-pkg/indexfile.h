// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexfile.h,v 1.6 2002/07/08 03:13:30 jgg Exp $
/* ######################################################################

   Index File - Abstraction for an index of archive/source file.
   
   There are 3 primary sorts of index files, all represented by this 
   class:
   
   Binary index files 
   Bianry index files decribing the local system
   Source index files
   
   They are all bundled together here, and the interfaces for 
   sources.list, acquire, cache gen and record parsing all use this class
   to acess the underlying representation.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_INDEXFILE_H
#define PKGLIB_INDEXFILE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/indexfile.h"
#endif

#include <string>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/pkgrecords.h>
    
using std::string;

class pkgAcquire;
class pkgCacheGenerator;
class OpProgress;
class pkgIndexFile
{
   public:

   class Type
   {
      public:
      
      // Global list of Items supported
      static Type **GlobalList;
      static unsigned long GlobalListLen;
      static Type *GetType(const char *Type);

      const char *Label;

      virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator /*File*/) const {return 0;};
      Type();
   };

   virtual const Type *GetType() const = 0;
   
   // Return descriptive strings of various sorts
   virtual string ArchiveInfo(pkgCache::VerIterator Ver) const;
   virtual string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const;
   virtual string Describe(bool Short = false) const = 0;   

   // Interface for acquire
   virtual string ArchiveURI(string /*File*/) const {return string();};
   virtual bool GetIndexes(pkgAcquire *Owner) const;

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const {return 0;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const = 0;
   virtual bool HasPackages() const = 0;
   virtual unsigned long Size() const = 0;
   virtual bool Merge(pkgCacheGenerator &/*Gen*/,OpProgress &/*Prog*/) const {return false;};
   virtual bool MergeFileProvides(pkgCacheGenerator &/*Gen*/,OpProgress &/*Prog*/) const {return true;};
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;
   
   virtual ~pkgIndexFile() {};
};

#endif

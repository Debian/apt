// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debindexfile.h,v 1.2 2001/02/20 07:03:17 jgg Exp $
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

#ifdef __GNUG__
#pragma interface "apt-pkg/debindexfile.h"
#endif

#include <apt-pkg/indexfile.h>

class debStatusIndex : public pkgIndexFile
{
   string File;
   
   public:

   virtual const Type *GetType() const;
   
   // Interface for acquire
   virtual string Describe() const {return File;};
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return true;};
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress &Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;

   debStatusIndex(string File);
};
    
class debPackagesIndex : public pkgIndexFile
{
   string URI;
   string Dist;
   string Section;
   
   string Info(const char *Type) const;
   string IndexFile(const char *Type) const;
   string IndexURI(const char *Type) const;   
   
   public:
   
   virtual const Type *GetType() const;

   // Stuff for accessing files on remote items
   virtual string ArchiveInfo(pkgCache::VerIterator Ver) const;
   virtual string ArchiveURI(string File) const {return URI + File;};
   
   // Interface for acquire
   virtual string Describe() const;   
   virtual bool GetIndexes(pkgAcquire *Owner) const;
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return true;};
   virtual unsigned long Size() const;
   virtual bool Merge(pkgCacheGenerator &Gen,OpProgress &Prog) const;
   virtual pkgCache::PkgFileIterator FindInCache(pkgCache &Cache) const;
   
   debPackagesIndex(string URI,string Dist,string Section);
};

class debSourcesIndex : public pkgIndexFile
{
   string URI;
   string Dist;
   string Section;

   string Info(const char *Type) const;
   string IndexFile(const char *Type) const;
   string IndexURI(const char *Type) const;   
   
   public:

   virtual const Type *GetType() const;

   // Stuff for accessing files on remote items
   virtual string SourceInfo(pkgSrcRecords::Parser const &Record,
			     pkgSrcRecords::File const &File) const;
   virtual string ArchiveURI(string File) const {return URI + File;};
   
   // Interface for acquire
   virtual string Describe() const;   
   virtual bool GetIndexes(pkgAcquire *Owner) const;

   // Interface for the record parsers
   virtual pkgSrcRecords::Parser *CreateSrcParser() const;
   
   // Interface for the Cache Generator
   virtual bool Exists() const;
   virtual bool HasPackages() const {return false;};
   virtual unsigned long Size() const;
   
   debSourcesIndex(string URI,string Dist,string Section);
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.h,v 1.7 1998/07/19 21:24:14 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   Each archive source has it's own list parser that is instantiated by
   the caller to provide data for the generator. 
   
   Parts of the cache are created by this generator class while other
   parts are created by the list parser. The list parser is responsible
   for creating version, depends and provides structures, and some of
   their contents
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_PKGCACHEGEN_H
#define PKGLIB_PKGCACHEGEN_H

#ifdef __GNUG__
#pragma interface "apt-pkg/pkgcachegen.h"
#endif 

#include <apt-pkg/pkgcache.h>

class pkgCacheGenerator
{
   public:
   
   class ListParser;
   
   protected:
   
   DynamicMMap &Map;
   pkgCache Cache;

   string PkgFileName;
   pkgCache::PackageFile *CurrentFile;
   
   bool NewPackage(pkgCache::PkgIterator &Pkg,string Pkg);
   bool NewFileVer(pkgCache::VerIterator &Ver,ListParser &List);
   unsigned long NewVersion(pkgCache::VerIterator &Ver,string VerStr,unsigned long Next);

   unsigned long WriteUniqString(const char *S,unsigned int Size);
   inline unsigned long WriteUniqString(string S) {return WriteUniqString(S);};
   
   public:
   
   // This is the abstract package list parser class.
   class ListParser
   {
      pkgCacheGenerator *Owner;
      friend pkgCacheGenerator;
      
      protected:
      
      inline unsigned long WriteUniqString(string S) {return Owner->WriteUniqString(S);};
      inline unsigned long WriteUniqString(const char *S,unsigned int Size) {return Owner->WriteUniqString(S,Size);};
      inline unsigned long WriteString(string S) {return Owner->Map.WriteString(S);};
      inline unsigned long WriteString(const char *S,unsigned int Size) {return Owner->Map.WriteString(S,Size);};
      bool NewDepends(pkgCache::VerIterator Ver,string Package,
			 string Version,unsigned int Op,
			 unsigned int Type);
      bool NewProvides(pkgCache::VerIterator Ver,string Package,string Version);
      
      public:
      
      // These all operate against the current section
      virtual string Package() = 0;
      virtual string Version() = 0;
      virtual bool NewVersion(pkgCache::VerIterator Ver) = 0;
      virtual bool UsePackage(pkgCache::PkgIterator Pkg,
			      pkgCache::VerIterator Ver) = 0;
      virtual unsigned long Offset() = 0;
      virtual unsigned long Size() = 0;
      
      virtual bool Step() = 0;
      
      virtual ~ListParser() {};
   };
   friend ListParser;

   bool SelectFile(string File,unsigned long Flags = 0);
   bool MergeList(ListParser &List);
   inline pkgCache &GetCache() {return Cache;};
      
   pkgCacheGenerator(DynamicMMap &Map);
   ~pkgCacheGenerator();
};

#endif

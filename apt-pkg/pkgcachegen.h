// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.h,v 1.2 1998/07/04 05:57:38 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_PKGCACHEGEN_H
#define PKGLIB_PKGCACHEGEN_H

#include <pkglib/pkgcache.h>

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
      
      public:
      
      // These all operate against the current section
      virtual string Package() = 0;
      virtual string Version() = 0;
      virtual bool NewVersion(pkgCache::VerIterator Ver) = 0;
      virtual bool NewPackage(pkgCache::PkgIterator Pkg) = 0;
      virtual bool UsePackage(pkgCache::PkgIterator Pkg,
			      pkgCache::VerIterator Ver) = 0;
				   
      virtual bool Step() = 0;
      virtual ~ListParser() {};
   };
   friend ListParser;

   bool SelectFile(string File,unsigned long Flags = 0);
   bool MergeList(ListParser &List);
   
   pkgCacheGenerator(DynamicMMap &Map);
   ~pkgCacheGenerator();
};

#endif

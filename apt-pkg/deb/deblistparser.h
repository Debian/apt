// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: deblistparser.h,v 1.1 1998/07/04 05:58:08 jgg Exp $
/* ######################################################################
   
   Debian Package List Parser - This implements the abstract parser 
   interface for Debian package files
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_DEBLISTPARSER_H
#define PKGLIB_DEBLISTPARSER_H

#include <pkglib/pkgcachegen.h>
#include <pkglib/tagfile.h>

class debListParser : public pkgCacheGenerator::ListParser
{
   pkgTagFile Tags;
   pkgTagSection Section;
   
   // Parser Helper
   struct WordList
   {
      char *Str;
      unsigned char Val;
   };
   
   string FindTag(const char *Tag);
   unsigned long UniqFindTagWrite(const char *Tag);
   bool HandleFlag(const char *Tag,unsigned long &Flags,unsigned long Flag);
   bool ParseStatus(pkgCache::PkgIterator Pkg,pkgCache::VerIterator Ver);
   bool GrabWord(string Word,WordList *List,int Count,unsigned char &Out);
   
   public:
   
   // These all operate against the current section
   virtual string Package();
   virtual string Version();
   virtual bool NewVersion(pkgCache::VerIterator Ver);
   virtual bool NewPackage(pkgCache::PkgIterator Pkg);
   virtual bool UsePackage(pkgCache::PkgIterator Pkg,
			   pkgCache::VerIterator Ver);

   virtual bool Step();
   
   debListParser(File &File);
};

#endif

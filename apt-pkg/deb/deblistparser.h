// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: deblistparser.h,v 1.4 1998/07/12 23:58:54 jgg Exp $
/* ######################################################################
   
   Debian Package List Parser - This implements the abstract parser 
   interface for Debian package files
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_DEBLISTPARSER_H
#define PKGLIB_DEBLISTPARSER_H

#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/tagfile.h>

class debListParser : public pkgCacheGenerator::ListParser
{
   pkgTagFile Tags;
   pkgTagSection Section;
   unsigned long iOffset;
   
   // Parser Helper
   struct WordList
   {
      char *Str;
      unsigned char Val;
   };
   
   string FindTag(const char *Tag);
   signed long FindTagI(const char *Tag,signed long Default = 0);
   unsigned long UniqFindTagWrite(const char *Tag);
   bool HandleFlag(const char *Tag,unsigned long &Flags,unsigned long Flag);
   bool ParseStatus(pkgCache::PkgIterator Pkg,pkgCache::VerIterator Ver);
   const char *ParseDepends(const char *Start,const char *Stop,
			    string &Package,string &Ver,unsigned int &Op);
   bool ParseDepends(pkgCache::VerIterator Ver,const char *Tag,
		     unsigned int Type);
   bool ParseProvides(pkgCache::VerIterator Ver);
   bool GrabWord(string Word,WordList *List,int Count,unsigned char &Out);
   
   public:
   
   // These all operate against the current section
   virtual string Package();
   virtual string Version();
   virtual bool NewVersion(pkgCache::VerIterator Ver);
   virtual bool UsePackage(pkgCache::PkgIterator Pkg,
			   pkgCache::VerIterator Ver);
   virtual unsigned long Offset() {return iOffset;};
   virtual unsigned long Size() {return Section.size();};

   virtual bool Step();
   
   debListParser(File &File);
};

#endif

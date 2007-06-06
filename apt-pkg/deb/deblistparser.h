// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: deblistparser.h,v 1.9 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################
   
   Debian Package List Parser - This implements the abstract parser 
   interface for Debian package files
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBLISTPARSER_H
#define PKGLIB_DEBLISTPARSER_H

#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/tagfile.h>

class debListParser : public pkgCacheGenerator::ListParser
{
   public:

   // Parser Helper
   struct WordList
   {
      const char *Str;
      unsigned char Val;
   };
   
   private:
   
   pkgTagFile Tags;
   pkgTagSection Section;
   unsigned long iOffset;
   string Arch;
   
   unsigned long UniqFindTagWrite(const char *Tag);
   bool ParseStatus(pkgCache::PkgIterator Pkg,pkgCache::VerIterator Ver);
   bool ParseDepends(pkgCache::VerIterator Ver,const char *Tag,
		     unsigned int Type);
   bool ParseProvides(pkgCache::VerIterator Ver);
   static bool GrabWord(string Word,WordList *List,unsigned char &Out);
   
   public:

   static unsigned char GetPrio(string Str);
      
   // These all operate against the current section
   virtual string Package();
   virtual string Version();
   virtual bool NewVersion(pkgCache::VerIterator Ver);
   virtual string Description();
   virtual string DescriptionLanguage();
   virtual MD5SumValue Description_md5();
   virtual unsigned short VersionHash();
   virtual bool UsePackage(pkgCache::PkgIterator Pkg,
			   pkgCache::VerIterator Ver);
   virtual unsigned long Offset() {return iOffset;};
   virtual unsigned long Size() {return Section.size();};

   virtual bool Step();
   
   bool LoadReleaseInfo(pkgCache::PkgFileIterator FileI,FileFd &File,
			string section);
   
   static const char *ParseDepends(const char *Start,const char *Stop,
			    string &Package,string &Ver,unsigned int &Op,
			    bool ParseArchFlags = false);
   static const char *ConvertRelation(const char *I,unsigned int &Op);

   debListParser(FileFd *File);
};

#endif

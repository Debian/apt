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
#include <apt-pkg/tagfile.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>

#include <string>
#include <vector>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#endif

class FileFd;

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
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   protected:
   pkgTagFile Tags;
   pkgTagSection Section;
   unsigned long iOffset;
   std::string Arch;
   std::vector<std::string> Architectures;
   bool MultiArchEnabled;

   unsigned long UniqFindTagWrite(const char *Tag);
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);
   bool ParseDepends(pkgCache::VerIterator &Ver,const char *Tag,
		     unsigned int Type);
   bool ParseProvides(pkgCache::VerIterator &Ver);
   bool NewProvidesAllArch(pkgCache::VerIterator &Ver, std::string const &Package, std::string const &Version);
   static bool GrabWord(std::string Word,WordList *List,unsigned char &Out);
   
   public:

   static unsigned char GetPrio(std::string Str);
      
   // These all operate against the current section
   virtual std::string Package();
   virtual std::string Architecture();
   virtual bool ArchitectureAll();
   virtual std::string Version();
   virtual bool NewVersion(pkgCache::VerIterator &Ver);
   virtual std::string Description();
   virtual std::string DescriptionLanguage();
   virtual MD5SumValue Description_md5();
   virtual unsigned short VersionHash();
#if (APT_PKG_MAJOR >= 4 && APT_PKG_MINOR >= 13)
   virtual bool SameVersion(unsigned short const Hash, pkgCache::VerIterator const &Ver);
#endif
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver);
   virtual unsigned long Offset() {return iOffset;};
   virtual unsigned long Size() {return Section.size();};

   virtual bool Step();
   
   bool LoadReleaseInfo(pkgCache::PkgFileIterator &FileI,FileFd &File,
			std::string section);

   static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op);
   static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags);
   static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags, bool const &StripMultiArch);
   static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags, bool const &StripMultiArch,
	 bool const &ParseRestrictionsList);

   static const char *ConvertRelation(const char *I,unsigned int &Op);

   debListParser(FileFd *File, std::string const &Arch = "");
   virtual ~debListParser() {};

   private:
   APT_HIDDEN unsigned char ParseMultiArch(bool const showErrors);
};

class debTranslationsParser : public debListParser
{
 public:
   // a translation can never be a real package
   virtual std::string Architecture() { return ""; }
   virtual std::string Version() { return ""; }

   debTranslationsParser(FileFd *File, std::string const &Arch = "")
      : debListParser(File, Arch) {};
};

class APT_HIDDEN debStatusListParser : public debListParser
{
 public:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);
   debStatusListParser(FileFd *File)
      : debListParser(File) {};
};
#endif

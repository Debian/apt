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
#ifdef APT_PKG_EXPOSE_STRING_VIEW
#include <apt-pkg/string_view.h>
#endif

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/indexfile.h>
#endif

class FileFd;

class APT_HIDDEN debListParser : public pkgCacheListParser
{
   public:

#ifdef APT_PKG_EXPOSE_STRING_VIEW
   // Parser Helper
   struct WordList
   {
      APT::StringView Str;
      unsigned char Val;
   };
#endif

   private:
   std::vector<std::string> forceEssential;
   std::vector<std::string> forceImportant;
   std::string MD5Buffer;

   protected:
   pkgTagFile Tags;
   pkgTagSection Section;
   map_filesize_t iOffset;

   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);
   bool ParseDepends(pkgCache::VerIterator &Ver, pkgTagSection::Key Key,
		     unsigned int Type);
   bool ParseProvides(pkgCache::VerIterator &Ver);

#ifdef APT_PKG_EXPOSE_STRING_VIEW
   APT_HIDDEN static bool GrabWord(APT::StringView Word,const WordList *List,unsigned char &Out);
#endif
   APT_HIDDEN unsigned char ParseMultiArch(bool const showErrors);

   public:

   APT_PUBLIC static unsigned char GetPrio(std::string Str);
      
   // These all operate against the current section
   virtual std::string Package() APT_OVERRIDE;
   virtual bool ArchitectureAll() APT_OVERRIDE;
#ifdef APT_PKG_EXPOSE_STRING_VIEW
   virtual APT::StringView Architecture() APT_OVERRIDE;
   virtual APT::StringView Version() APT_OVERRIDE;
#endif
   virtual bool NewVersion(pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual std::vector<std::string> AvailableDescriptionLanguages() APT_OVERRIDE;
#ifdef APT_PKG_EXPOSE_STRING_VIEW
   virtual APT::StringView Description_md5() APT_OVERRIDE;
#endif
   virtual unsigned short VersionHash() APT_OVERRIDE;
   virtual bool SameVersion(unsigned short const Hash, pkgCache::VerIterator const &Ver) APT_OVERRIDE;
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual map_filesize_t Offset() APT_OVERRIDE {return iOffset;};
   virtual map_filesize_t Size() APT_OVERRIDE {return Section.size();};

   virtual bool Step() APT_OVERRIDE;

   bool LoadReleaseInfo(pkgCache::RlsFileIterator &FileI,FileFd &File,
			std::string const &section);

   APT_PUBLIC static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op);
   APT_PUBLIC static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags);
   APT_PUBLIC static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags, bool const &StripMultiArch);
   APT_PUBLIC static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags, bool const &StripMultiArch,
	 bool const &ParseRestrictionsList);
   APT_PUBLIC static const char *ParseDepends(const char *Start,const char *Stop,
	 std::string &Package,std::string &Ver,unsigned int &Op,
	 bool const &ParseArchFlags, bool const &StripMultiArch,
	 bool const &ParseRestrictionsList,
	 std::string const &Arch);

#ifdef APT_PKG_EXPOSE_STRING_VIEW
   APT_HIDDEN static const char *ParseDepends(const char *Start,const char *Stop,
	 APT::StringView &Package,
    APT::StringView &Ver,unsigned int &Op,
	 bool const ParseArchFlags = false, bool StripMultiArch = true,
	 bool const ParseRestrictionsList = false);
   APT_HIDDEN static const char *ParseDepends(const char *Start,const char *Stop,
	 APT::StringView &Package,
	 APT::StringView &Ver,unsigned int &Op,
	 bool const ParseArchFlags, bool StripMultiArch,
	 bool const ParseRestrictionsList,
	 std::string const &Arch);
#endif

   APT_PUBLIC static const char *ConvertRelation(const char *I,unsigned int &Op);

   debListParser(FileFd *File);
   virtual ~debListParser();
};

class APT_HIDDEN debDebFileParser : public debListParser
{
 private:
   std::string DebFile;

 public:
   debDebFileParser(FileFd *File, std::string const &DebFile);
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver) APT_OVERRIDE;
};

class APT_HIDDEN debTranslationsParser : public debListParser
{
 public:
#ifdef APT_PKG_EXPOSE_STRING_VIEW
   // a translation can never be a real package
   virtual APT::StringView Architecture() APT_OVERRIDE { return ""; }
   virtual APT::StringView Version() APT_OVERRIDE { return ""; }
#endif

   debTranslationsParser(FileFd *File)
      : debListParser(File) {};
};

class APT_HIDDEN debStatusListParser : public debListParser
{
 public:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);
   debStatusListParser(FileFd *File)
      : debListParser(File) {};
};
#endif

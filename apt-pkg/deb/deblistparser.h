// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Debian Package List Parser - This implements the abstract parser 
   interface for Debian package files
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBLISTPARSER_H
#define PKGLIB_DEBLISTPARSER_H

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/tagfile.h>
#ifdef APT_COMPILING_APT
#include <apt-pkg/tagfile-keys.h>
#endif

#include <string>
#include <vector>
#include <apt-pkg/string_view.h>


class FileFd;

class APT_HIDDEN debListParser : public pkgCacheListParser
{
   public:

   // Parser Helper
   struct WordList
   {
      APT::StringView Str;
      unsigned char Val;
   };

   private:
   std::vector<std::string> forceEssential;
   std::vector<std::string> forceImportant;
   std::string MD5Buffer;
   std::string myArch;

   protected:
   pkgTagFile Tags;
   pkgTagSection Section;
   map_filesize_t iOffset;

   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);
   bool ParseDepends(pkgCache::VerIterator &Ver, pkgTagSection::Key Key,
		     unsigned int Type);
   bool ParseProvides(pkgCache::VerIterator &Ver);

   APT_HIDDEN static bool GrabWord(APT::StringView Word,const WordList *List,unsigned char &Out);
   APT_HIDDEN unsigned char ParseMultiArch(bool const showErrors);

   public:

   APT_PUBLIC static unsigned char GetPrio(std::string Str);
      
   // These all operate against the current section
   virtual std::string Package() APT_OVERRIDE;
   virtual bool ArchitectureAll() APT_OVERRIDE;
   virtual APT::StringView Architecture() APT_OVERRIDE;
   virtual APT::StringView Version() APT_OVERRIDE;
   virtual bool NewVersion(pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual std::vector<std::string> AvailableDescriptionLanguages() APT_OVERRIDE;
   virtual APT::StringView Description_md5() APT_OVERRIDE;
   virtual uint32_t VersionHash() APT_OVERRIDE;
   virtual bool SameVersion(uint32_t Hash, pkgCache::VerIterator const &Ver) APT_OVERRIDE;
   virtual bool UsePackage(pkgCache::PkgIterator &Pkg,
			   pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual map_filesize_t Offset() APT_OVERRIDE {return iOffset;};
   virtual map_filesize_t Size() APT_OVERRIDE {return Section.size();};

   virtual bool Step() APT_OVERRIDE;

   APT_PUBLIC static const char *ParseDepends(const char *Start, const char *Stop,
					      std::string &Package, std::string &Ver, unsigned int &Op,
					      bool const &ParseArchFlags = false, bool const &StripMultiArch = true,
					      bool const &ParseRestrictionsList = false,
					      std::string const &Arch = "");

   APT_PUBLIC static const char *ParseDepends(const char *Start, const char *Stop,
					      APT::StringView &Package,
					      APT::StringView &Ver, unsigned int &Op,
					      bool const ParseArchFlags = false, bool StripMultiArch = true,
					      bool const ParseRestrictionsList = false,
					      std::string Arch = "");

   APT_PUBLIC static const char *ConvertRelation(const char *I,unsigned int &Op);

   explicit debListParser(FileFd *File);
   virtual ~debListParser();

#ifdef APT_COMPILING_APT
   APT::StringView SHA256() const
   {
      return Section.Find(pkgTagSection::Key::SHA256);
   }
#endif
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
   // a translation can never be a real package
   virtual APT::StringView Architecture() APT_OVERRIDE { return ""; }
   virtual APT::StringView Version() APT_OVERRIDE { return ""; }

   explicit debTranslationsParser(FileFd *File)
      : debListParser(File) {};
};

class APT_HIDDEN debStatusListParser : public debListParser
{
 public:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;
   explicit debStatusListParser(FileFd *File)
      : debListParser(File) {};
};
#endif

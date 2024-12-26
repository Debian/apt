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


class FileFd;

class APT_HIDDEN debListParser : public pkgCacheListParser
{
   public:

   // Parser Helper
   struct WordList
   {
      std::string_view Str;
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

   APT_HIDDEN static bool GrabWord(std::string_view Word,const WordList *List,unsigned char &Out);
   APT_HIDDEN unsigned char ParseMultiArch(bool const showErrors);

   public:

   APT_PUBLIC static unsigned char GetPrio(std::string Str);

   // These all operate against the current section
   std::string Package() override;
   bool ArchitectureAll() override;
   std::string_view Architecture() override;
   std::string_view Version() override;
   bool NewVersion(pkgCache::VerIterator &Ver) override;
   std::vector<std::string> AvailableDescriptionLanguages() override;
   std::string_view Description_md5() override;
   uint32_t VersionHash() override;
   bool SameVersion(uint32_t Hash, pkgCache::VerIterator const &Ver) override;
   bool UsePackage(pkgCache::PkgIterator &Pkg,
		   pkgCache::VerIterator &Ver) override;
   map_filesize_t Offset() override { return iOffset; };
   map_filesize_t Size() override { return Section.size(); };

   bool Step() override;

   APT_PUBLIC static const char *ParseDepends(const char *Start, const char *Stop,
					      std::string &Package, std::string &Ver, unsigned int &Op,
					      bool const &ParseArchFlags = false, bool const &StripMultiArch = true,
					      bool const &ParseRestrictionsList = false,
					      std::string const &Arch = "");

#if APT_PKG_ABI <= 600
   [[deprecated("Use std::string_view variant instead")]]
   APT_PUBLIC static const char *ParseDepends(const char *Start, const char *Stop,
					      APT::StringView &Package,
					      APT::StringView &Ver, unsigned int &Op,
					      bool ParseArchFlags = false, bool StripMultiArch = true,
					      bool ParseRestrictionsList = false,
					      std::string Arch = "");
#endif
   APT_PUBLIC static const char *ParseDepends(const char *Start, const char *Stop,
					      std::string_view &Package,
					      std::string_view &Ver, unsigned int &Op,
					      bool ParseArchFlags = false, bool StripMultiArch = true,
					      bool ParseRestrictionsList = false,
					      std::string Arch = "");

   APT_PUBLIC static const char *ConvertRelation(const char *I,unsigned int &Op);

   explicit debListParser(FileFd *File);
   ~debListParser() override;

#ifdef APT_COMPILING_APT
   std::string_view SHA256() const
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
   bool UsePackage(pkgCache::PkgIterator &Pkg,
		   pkgCache::VerIterator &Ver) override;
};

class APT_HIDDEN debTranslationsParser : public debListParser
{
 public:
   // a translation can never be a real package
 std::string_view Architecture() override { return {}; }
 std::string_view Version() override { return {}; }

 explicit debTranslationsParser(FileFd *File)
     : debListParser(File) {};
};

class APT_HIDDEN debStatusListParser : public debListParser
{
 public:
   bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) override;
   explicit debStatusListParser(FileFd *File)
      : debListParser(File) {};
};
#endif

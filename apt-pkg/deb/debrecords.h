// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Debian Package Records - Parser for debian package records
   
   This provides display-type parsing for the Packages file. This is
   different than the list parser which provides cache generation
   services. There should be no overlap between these two.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBRECORDS_H
#define PKGLIB_DEBRECORDS_H

#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/tagfile.h>

#include <string>


class APT_HIDDEN debRecordParserBase : public pkgRecords::Parser
{
   void * const d;
 protected:
   pkgTagSection Section;

 public:
   // These refer to the archive file for the Version
   std::string FileName() override;
   std::string SourcePkg() override;
   std::string SourceVer() override;

   [[nodiscard]] HashStringList Hashes() const override;

   // These are some general stats about the package
   std::string Maintainer() override;
   std::string ShortDesc(std::string const &lang) override;
   std::string LongDesc(std::string const &lang) override;
   std::string Name() override;
   std::string Homepage() override;

   // An arbitrary custom field
   std::string RecordField(const char *fieldName) override;

   void GetRec(const char *&Start, const char *&Stop) override;

   debRecordParserBase();
   ~debRecordParserBase() override;
};

class APT_HIDDEN debRecordParser : public debRecordParserBase
{
   void * const d;
 protected:
   FileFd File;
   pkgTagFile Tags;

   bool Jump(pkgCache::VerFileIterator const &Ver) override;
   bool Jump(pkgCache::DescFileIterator const &Desc) override;

 public:
   debRecordParser(std::string FileName,pkgCache &Cache);
   ~debRecordParser() override;
};

// custom record parser that reads deb files directly
class APT_HIDDEN debDebFileRecordParser : public debRecordParserBase
{
   void * const d;
   std::string debFileName;
   std::string controlContent;

   APT_HIDDEN bool LoadContent();
 protected:
   // single file files, so no jumping whatsoever
 bool Jump(pkgCache::VerFileIterator const &/*Ver*/) override;
 bool Jump(pkgCache::DescFileIterator const & /*Desc*/) override;

 public:
 std::string FileName() override;

 explicit debDebFileRecordParser(std::string FileName);
 ~debDebFileRecordParser() override;
};

#endif

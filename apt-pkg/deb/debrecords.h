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
   virtual std::string FileName() APT_OVERRIDE;
   virtual std::string SourcePkg() APT_OVERRIDE;
   virtual std::string SourceVer() APT_OVERRIDE;

   virtual HashStringList Hashes() const APT_OVERRIDE;

   // These are some general stats about the package
   virtual std::string Maintainer() APT_OVERRIDE;
   virtual std::string ShortDesc(std::string const &lang) APT_OVERRIDE;
   virtual std::string LongDesc(std::string const &lang) APT_OVERRIDE;
   virtual std::string Name() APT_OVERRIDE;
   virtual std::string Homepage() APT_OVERRIDE;

   // An arbitrary custom field
   virtual std::string RecordField(const char *fieldName) APT_OVERRIDE;

   virtual void GetRec(const char *&Start,const char *&Stop) APT_OVERRIDE;

   debRecordParserBase();
   virtual ~debRecordParserBase();
};

class APT_HIDDEN debRecordParser : public debRecordParserBase
{
   void * const d;
 protected:
   FileFd File;
   pkgTagFile Tags;

   virtual bool Jump(pkgCache::VerFileIterator const &Ver) APT_OVERRIDE;
   virtual bool Jump(pkgCache::DescFileIterator const &Desc) APT_OVERRIDE;

 public:
   debRecordParser(std::string FileName,pkgCache &Cache);
   virtual ~debRecordParser();
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
   bool Jump(pkgCache::VerFileIterator const &) APT_OVERRIDE;
   bool Jump(pkgCache::DescFileIterator const &) APT_OVERRIDE;

 public:
   virtual std::string FileName() APT_OVERRIDE;

   explicit debDebFileRecordParser(std::string FileName);
   virtual ~debDebFileRecordParser();
};

#endif

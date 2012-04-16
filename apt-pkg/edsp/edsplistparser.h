// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   EDSP Package List Parser - This implements the abstract parser
   interface for the APT specific intermediate format which is passed
   to external resolvers

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPLISTPARSER_H
#define PKGLIB_EDSPLISTPARSER_H

#include <apt-pkg/deblistparser.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/tagfile.h>
#endif

class FileFd;

class edspListParser : public debListParser
{
   public:
   virtual bool NewVersion(pkgCache::VerIterator &Ver);
   virtual std::string Description();
   virtual std::string DescriptionLanguage();
   virtual MD5SumValue Description_md5();
   virtual unsigned short VersionHash();

   bool LoadReleaseInfo(pkgCache::PkgFileIterator &FileI,FileFd &File,
			std::string section);

   edspListParser(FileFd *File, std::string const &Arch = "");

   protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);

};

#endif

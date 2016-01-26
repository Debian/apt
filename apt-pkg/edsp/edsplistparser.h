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
#include <apt-pkg/md5.h>
#include <apt-pkg/pkgcache.h>

#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/tagfile.h>
#endif

class FileFd;
class edspListParserPrivate;

class APT_HIDDEN edspListParser : public debListParser
{
   edspListParserPrivate * const d;
   public:
   virtual bool NewVersion(pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual std::vector<std::string> AvailableDescriptionLanguages() APT_OVERRIDE;
   virtual MD5SumValue Description_md5() APT_OVERRIDE;
   virtual unsigned short VersionHash() APT_OVERRIDE;

   bool LoadReleaseInfo(pkgCache::RlsFileIterator &FileI,FileFd &File,
			std::string const &section);

   edspListParser(FileFd *File);
   virtual ~edspListParser();

   protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;

};

#endif

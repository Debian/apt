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
#include <apt-pkg/fileutl.h>

#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/tagfile.h>
#endif

namespace APT {
   class StringView;
}
class APT_HIDDEN edspLikeListParser : public debListParser
{
   public:
   virtual bool NewVersion(pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual std::vector<std::string> AvailableDescriptionLanguages() APT_OVERRIDE;
   virtual APT::StringView Description_md5() APT_OVERRIDE;
   virtual unsigned short VersionHash() APT_OVERRIDE;

   bool LoadReleaseInfo(pkgCache::RlsFileIterator &FileI,FileFd &File,
			std::string const &section);

   edspLikeListParser(FileFd *File);
   virtual ~edspLikeListParser();
};

class APT_HIDDEN edspListParser : public edspLikeListParser
{
   FileFd extendedstates;
   FileFd preferences;

protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;

public:
   edspListParser(FileFd *File);
   virtual ~edspListParser();
};

class APT_HIDDEN eippListParser : public edspLikeListParser
{
protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;

public:
   eippListParser(FileFd *File);
   virtual ~eippListParser();
};
#endif

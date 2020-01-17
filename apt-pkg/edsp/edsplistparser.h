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
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcache.h>

#include <string>


namespace APT {
   class StringView;
}
class APT_HIDDEN edspLikeListParser : public debListParser
{
   public:
   virtual bool NewVersion(pkgCache::VerIterator &Ver) APT_OVERRIDE;
   virtual std::vector<std::string> AvailableDescriptionLanguages() APT_OVERRIDE;
   virtual APT::StringView Description_md5() APT_OVERRIDE;
   virtual uint32_t VersionHash() APT_OVERRIDE;

   explicit edspLikeListParser(FileFd *File);
   virtual ~edspLikeListParser();
};

class APT_HIDDEN edspListParser : public edspLikeListParser
{
   FileFd extendedstates;
   FileFd preferences;

protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;

public:
   explicit edspListParser(FileFd *File);
   virtual ~edspListParser();
};

class APT_HIDDEN eippListParser : public edspLikeListParser
{
protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) APT_OVERRIDE;

public:
   explicit eippListParser(FileFd *File);
   virtual ~eippListParser();
};
#endif

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


class APT_HIDDEN edspLikeListParser : public debListParser
{
   public:
   bool NewVersion(pkgCache::VerIterator &Ver) override;
   std::vector<std::string> AvailableDescriptionLanguages() override;
   std::string_view Description_md5() override;
   uint32_t VersionHash() override;

   explicit edspLikeListParser(FileFd *File);
   ~edspLikeListParser() override;
};

class APT_HIDDEN edspListParser : public edspLikeListParser
{
   FileFd extendedstates;
   FileFd preferences;

protected:
   bool ParseStatus(pkgCache::PkgIterator &Pkg, pkgCache::VerIterator &Ver) override;

public:
   explicit edspListParser(FileFd *File);
   ~edspListParser() override;
};

class APT_HIDDEN eippListParser : public edspLikeListParser
{
protected:
   bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver) override;

public:
   explicit eippListParser(FileFd *File);
   ~eippListParser() override;
};
#endif

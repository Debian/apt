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
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/tagfile.h>

class edspListParser : public debListParser
{
   public:
   virtual bool NewVersion(pkgCache::VerIterator &Ver);
   virtual string Description();
   virtual string DescriptionLanguage();
   virtual MD5SumValue Description_md5();
   virtual unsigned short VersionHash();

   bool LoadReleaseInfo(pkgCache::PkgFileIterator &FileI,FileFd &File,
			string section);

   edspListParser(FileFd *File, string const &Arch = "");

   protected:
   virtual bool ParseStatus(pkgCache::PkgIterator &Pkg,pkgCache::VerIterator &Ver);

};

#endif

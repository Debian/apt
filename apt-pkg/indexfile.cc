// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexfile.cc,v 1.2.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Index File - Abstraction for an index of archive/souce file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/indexfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/aptconfiguration.h>

#include <clocale>
#include <cstring>
									/*}}}*/

// Global list of Item supported
static  pkgIndexFile::Type *ItmList[10];
pkgIndexFile::Type **pkgIndexFile::Type::GlobalList = ItmList;
unsigned long pkgIndexFile::Type::GlobalListLen = 0;

// Type::Type - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgIndexFile::Type::Type()
{
   ItmList[GlobalListLen] = this;
   GlobalListLen++;   
}
									/*}}}*/
// Type::GetType - Locate the type by name				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgIndexFile::Type *pkgIndexFile::Type::GetType(const char *Type)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(GlobalList[I]->Label,Type) == 0)
	 return GlobalList[I];
   return 0;
}
									/*}}}*/
// IndexFile::ArchiveInfo - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgIndexFile::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   return string();
}
									/*}}}*/
// IndexFile::FindInCache - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator pkgIndexFile::FindInCache(pkgCache &Cache) const
{
   return pkgCache::PkgFileIterator(Cache);
}
									/*}}}*/
// IndexFile::SourceIndex - Stub					/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgIndexFile::SourceInfo(pkgSrcRecords::Parser const &Record,
				pkgSrcRecords::File const &File) const
{
   return string();
}
									/*}}}*/
// IndexFile::TranslationsAvailable - Check if will use Translation	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgIndexFile::TranslationsAvailable() {
	return (APT::Configuration::getLanguages().empty() != true);
}
									/*}}}*/
// IndexFile::CheckLanguageCode - Check the Language Code		/*{{{*/
// ---------------------------------------------------------------------
/* No intern need for this method anymore as the check for correctness
   is already done in getLanguages(). Note also that this check is
   rather bad (doesn't take three character like ast into account).
   TODO: Remove method with next API break */
__attribute__ ((deprecated)) bool pkgIndexFile::CheckLanguageCode(const char *Lang)
{
  if (strlen(Lang) == 2 || (strlen(Lang) == 5 && Lang[2] == '_'))
    return true;

  if (strcmp(Lang,"C") != 0)
    _error->Warning("Wrong language code %s", Lang);

  return false;
}
									/*}}}*/
// IndexFile::LanguageCode - Return the Language Code			/*{{{*/
// ---------------------------------------------------------------------
/* As we have now possibly more than one LanguageCode this method is
   supersided by a) private classmembers or b) getLanguages().
   TODO: Remove method with next API break */
__attribute__ ((deprecated)) string pkgIndexFile::LanguageCode() {
	if (TranslationsAvailable() == false)
		return "";
	return APT::Configuration::getLanguages()[0];
}
									/*}}}*/

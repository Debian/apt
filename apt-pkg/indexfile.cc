// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexfile.cc,v 1.2.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Index File - Abstraction for an index of archive/souce file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/configuration.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/error.h>

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
// IndexFile::TranslationsAvailable - Check if will use Translation    /*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgIndexFile::TranslationsAvailable()
{
  const string Translation = _config->Find("APT::Acquire::Translation");
  
  if (Translation.compare("none") != 0)
    return CheckLanguageCode(LanguageCode().c_str());
  else
    return false;
}
									/*}}}*/
// IndexFile::CheckLanguageCode - Check the Language Code   	        /*{{{*/
// ---------------------------------------------------------------------
/* */
/* common cases: de_DE, de_DE@euro, de_DE.UTF-8, de_DE.UTF-8@euro,
                 de_DE.ISO8859-1, tig_ER
                 more in /etc/gdm/locale.conf 
*/

bool pkgIndexFile::CheckLanguageCode(const char *Lang)
{
  if (strlen(Lang) == 2 || (strlen(Lang) == 5 && Lang[2] == '_'))
    return true;

  if (strcmp(Lang,"C") != 0)
    _error->Warning("Wrong language code %s", Lang);

  return false;
}
									/*}}}*/
// IndexFile::LanguageCode - Return the Language Code            	/*{{{*/
// ---------------------------------------------------------------------
/* return the language code */
string pkgIndexFile::LanguageCode()
{
  const string Translation = _config->Find("APT::Acquire::Translation");

  if (Translation.compare("environment") == 0) 
  {
     string lang = std::setlocale(LC_MESSAGES,NULL);

     // we have a mapping of the language codes that contains all the language
     // codes that need the country code as well 
     // (like pt_BR, pt_PT, sv_SE, zh_*, en_*)
     const char *need_full_langcode[] = { "pt","sv","zh","en", NULL };
     for(const char **s = need_full_langcode;*s != NULL; s++)
	if(lang.find(*s) == 0)
	   return lang.substr(0,5);
     
     if(lang.size() > 2)
	return lang.substr(0,2);
     else
	return lang;
  }
  else 
     return Translation;
}
									/*}}}*/

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexfile.cc,v 1.2.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Index File - Abstraction for an index of archive/souce file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/macros.h>

#include <string>
#include <vector>
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
   Label = NULL;
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
pkgIndexFile::pkgIndexFile(bool Trusted) :				/*{{{*/
   Trusted(Trusted)
{
}
									/*}}}*/
// IndexFile::ArchiveInfo - Stub					/*{{{*/
std::string pkgIndexFile::ArchiveInfo(pkgCache::VerIterator /*Ver*/) const
{
   return std::string();
}
									/*}}}*/
// IndexFile::FindInCache - Stub					/*{{{*/
pkgCache::PkgFileIterator pkgIndexFile::FindInCache(pkgCache &Cache) const
{
   return pkgCache::PkgFileIterator(Cache);
}
									/*}}}*/
// IndexFile::SourceIndex - Stub					/*{{{*/
std::string pkgIndexFile::SourceInfo(pkgSrcRecords::Parser const &/*Record*/,
				pkgSrcRecords::File const &/*File*/) const
{
   return std::string();
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
APT_DEPRECATED bool pkgIndexFile::CheckLanguageCode(const char *Lang)
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
APT_DEPRECATED std::string pkgIndexFile::LanguageCode() {
	if (TranslationsAvailable() == false)
		return "";
	return APT::Configuration::getLanguages()[0];
}
									/*}}}*/

// IndexTarget - Constructor						/*{{{*/
IndexTarget::IndexTarget(std::string const &MetaKey, std::string const &ShortDesc,
      std::string const &LongDesc, std::string const &URI, bool const IsOptional,
      std::map<std::string, std::string> const &Options) :
   URI(URI), Description(LongDesc), ShortDesc(ShortDesc), MetaKey(MetaKey), IsOptional(IsOptional), Options(Options)
{
}
									/*}}}*/
std::string IndexTarget::Option(OptionKeys const EnumKey) const		/*{{{*/
{
   std::string Key;
   switch (EnumKey)
   {
#define APT_CASE(X) case X: Key = #X; break
      APT_CASE(SITE);
      APT_CASE(RELEASE);
      APT_CASE(COMPONENT);
      APT_CASE(LANGUAGE);
      APT_CASE(ARCHITECTURE);
      APT_CASE(BASE_URI);
      APT_CASE(REPO_URI);
      APT_CASE(CREATED_BY);
#undef APT_CASE
   }
   std::map<std::string,std::string>::const_iterator const M = Options.find(Key);
   if (M == Options.end())
      return "";
   return M->second;
}
									/*}}}*/

pkgIndexTargetFile::pkgIndexTargetFile(IndexTarget const &Target, bool const Trusted) :/*{{{*/
   pkgIndexFile(Trusted), Target(Target)
{
}
									/*}}}*/
std::string pkgIndexTargetFile::ArchiveURI(std::string File) const/*{{{*/
{
   return Target.Option(IndexTarget::REPO_URI) + File;
}
									/*}}}*/
std::string pkgIndexTargetFile::Describe(bool Short) const		/*{{{*/
{
   if (Short)
      return Target.Description;
   return Target.Description + " (" + IndexFileName() + ")";
}
									/*}}}*/
std::string pkgIndexTargetFile::IndexFileName() const			/*{{{*/
{
   std::string const s =_config->FindDir("Dir::State::lists") + URItoFileName(Target.URI);
   if (FileExists(s))
      return s;

   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      std::string p = s + '.' + *t;
      if (FileExists(p))
         return p;
   }
   return s;
}
									/*}}}*/
unsigned long pkgIndexTargetFile::Size() const				/*{{{*/
{
   unsigned long size = 0;

   /* we need to ignore errors here; if the lists are absent, just return 0 */
   _error->PushToStack();

   FileFd f(IndexFileName(), FileFd::ReadOnly, FileFd::Extension);
   if (!f.Failed())
      size = f.Size();

   if (_error->PendingError() == true)
       size = 0;
   _error->RevertToStack();

   return size;
}
									/*}}}*/
bool pkgIndexTargetFile::Exists() const					/*{{{*/
{
   return FileExists(IndexFileName());
}
									/*}}}*/

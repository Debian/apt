// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   The scenario file is designed to work as an intermediate file between
   APT and the resolver. Its on propose very similar to a dpkg status file
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/edspindexfile.h>
#include <apt-pkg/edsplistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>

#include <stddef.h>
#include <unistd.h>
#include <string>
									/*}}}*/

// EDSP-like Index							/*{{{*/
edspLikeIndex::edspLikeIndex(std::string const &File) : pkgDebianIndexRealFile(File, true)
{
}
std::string edspLikeIndex::GetArchitecture() const
{
   return std::string();
}
bool edspLikeIndex::HasPackages() const
{
   return true;
}
bool edspLikeIndex::Exists() const
{
   return true;
}
uint8_t edspLikeIndex::GetIndexFlags() const
{
   return 0;
}
bool edspLikeIndex::OpenListFile(FileFd &Pkg, std::string const &FileName)
{
   if (FileName.empty() == false && FileName != "/nonexistent/stdin")
      return pkgDebianIndexRealFile::OpenListFile(Pkg, FileName);
   if (Pkg.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false)
      return _error->Error("Problem opening %s",FileName.c_str());
   return true;
}
									/*}}}*/
// EDSP Index								/*{{{*/
edspIndex::edspIndex(std::string const &File) : edspLikeIndex(File)
{
}
std::string edspIndex::GetComponent() const
{
   return "edsp";
}
pkgCacheListParser * edspIndex::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return NULL;
   _error->PushToStack();
   pkgCacheListParser * const Parser = new edspListParser(&Pkg);
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   return newError ? NULL : Parser;
}
									/*}}}*/
// EIPP Index								/*{{{*/
eippIndex::eippIndex(std::string const &File) : edspLikeIndex(File)
{
}
std::string eippIndex::GetComponent() const
{
   return "eipp";
}
pkgCacheListParser * eippIndex::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return NULL;
   _error->PushToStack();
   pkgCacheListParser * const Parser = new eippListParser(&Pkg);
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   return newError ? NULL : Parser;
}
									/*}}}*/

// Index File types for APT						/*{{{*/
class APT_HIDDEN edspIFType: public pkgIndexFile::Type
{
   public:
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &) const APT_OVERRIDE
   {
      // we don't have a record parser for this type as the file is not presistent
      return NULL;
   };
   edspIFType() {Label = "EDSP scenario file";};
};
APT_HIDDEN edspIFType _apt_Edsp;
const pkgIndexFile::Type *edspIndex::GetType() const
{
   return &_apt_Edsp;
}

class APT_HIDDEN eippIFType: public pkgIndexFile::Type
{
   public:
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &) const APT_OVERRIDE
   {
      // we don't have a record parser for this type as the file is not presistent
      return NULL;
   };
   eippIFType() {Label = "EIPP scenario file";};
};
APT_HIDDEN eippIFType _apt_Eipp;
const pkgIndexFile::Type *eippIndex::GetType() const
{
   return &_apt_Eipp;
}
									/*}}}*/

edspLikeIndex::~edspLikeIndex() {}
edspIndex::~edspIndex() {}
eippIndex::~eippIndex() {}

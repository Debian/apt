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

// EDSP Index								/*{{{*/
edspIndex::edspIndex(std::string const &File) : pkgDebianIndexRealFile(File, true), d(NULL)
{
}
std::string edspIndex::GetComponent() const
{
   return "edsp";
}
std::string edspIndex::GetArchitecture() const
{
   return std::string();
}
bool edspIndex::HasPackages() const
{
   return true;
}
bool edspIndex::Exists() const
{
   return true;
}
uint8_t edspIndex::GetIndexFlags() const
{
   return 0;
}
bool edspIndex::OpenListFile(FileFd &Pkg, std::string const &FileName)
{
   if (FileName.empty() == false && FileName != "/nonexistent/stdin")
      return pkgDebianIndexRealFile::OpenListFile(Pkg, FileName);
   if (Pkg.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly) == false)
      return _error->Error("Problem opening %s",FileName.c_str());
   return true;
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
									/*}}}*/

edspIndex::~edspIndex() {}

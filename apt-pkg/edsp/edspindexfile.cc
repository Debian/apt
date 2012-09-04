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
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-item.h>

#include <sys/stat.h>
									/*}}}*/

// edspIndex::edspIndex - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
edspIndex::edspIndex(std::string File) : debStatusIndex(File)
{
}
									/*}}}*/
// StatusIndex::Merge - Load the index file into a cache		/*{{{*/
bool edspIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   FileFd Pkg;
   if (File != "stdin")
      Pkg.Open(File, FileFd::ReadOnly);
   else
      Pkg.OpenDescriptor(STDIN_FILENO, FileFd::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   edspListParser Parser(&Pkg);
   if (_error->PendingError() == true)
      return false;

   if (Prog != NULL)
      Prog->SubProgress(0,File);
   if (Gen.SelectFile(File,std::string(),*this) == false)
      return _error->Error("Problem with SelectFile %s",File.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator CFile = Gen.GetCurFile();
   CFile->Size = Pkg.FileSize();
   CFile->mtime = Pkg.ModificationTime();
   map_ptrloc const storage = Gen.WriteUniqString("edsp::scenario");
   CFile->Archive = storage;

   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeList %s",File.c_str());
   return true;
}
									/*}}}*/
// Index File types for APT						/*{{{*/
class edspIFType: public pkgIndexFile::Type
{
   public:
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const
   {
      // we don't have a record parser for this type as the file is not presistent
      return NULL;
   };
   edspIFType() {Label = "EDSP scenario file";};
};
static edspIFType _apt_Universe;

const pkgIndexFile::Type *edspIndex::GetType() const
{
   return &_apt_Universe;
}
									/*}}}*/

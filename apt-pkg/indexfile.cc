// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Index File - Abstraction for an index of archive/source file.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>

#include <apt-pkg/debindexfile.h>

#include <sys/stat.h>

#include <clocale>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
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
pkgIndexFile::pkgIndexFile(bool const Trusted) :			/*{{{*/
   d(NULL), Trusted(Trusted)
{
}
									/*}}}*/
// IndexFile::ArchiveInfo - Stub					/*{{{*/
std::string pkgIndexFile::ArchiveInfo(pkgCache::VerIterator const &) const
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

// IndexTarget - Constructor						/*{{{*/
IndexTarget::IndexTarget(std::string const &MetaKey, std::string const &ShortDesc,
      std::string const &LongDesc, std::string const &URI, bool const IsOptional,
      bool const KeepCompressed, std::map<std::string, std::string> const &Options) :
   URI(URI), Description(LongDesc), ShortDesc(ShortDesc), MetaKey(MetaKey),
   IsOptional(IsOptional), KeepCompressed(KeepCompressed), Options(Options)
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
      APT_CASE(IDENTIFIER);
      APT_CASE(TARGET_OF);
      APT_CASE(CREATED_BY);
      APT_CASE(FALLBACK_OF);
      APT_CASE(PDIFFS);
      APT_CASE(DEFAULTENABLED);
      APT_CASE(COMPRESSIONTYPES);
      APT_CASE(SOURCESENTRY);
      APT_CASE(BY_HASH);
      APT_CASE(KEEPCOMPRESSEDAS);
      APT_CASE(ALLOW_INSECURE);
      APT_CASE(ALLOW_WEAK);
      APT_CASE(ALLOW_DOWNGRADE_TO_INSECURE);
      APT_CASE(INRELEASE_PATH);
      APT_CASE(SHADOWED);
#undef APT_CASE
      case FILENAME:
      {
	 auto const M = Options.find("FILENAME");
	 if (M == Options.end())
	    return _config->FindDir("Dir::State::lists") + URItoFileName(URI);
	 return M->second;
      }
      case EXISTING_FILENAME:
	 std::string const filename = Option(FILENAME);
	 std::vector<std::string> const types = VectorizeString(Option(COMPRESSIONTYPES), ' ');
	 for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
	 {
	    if (t->empty())
	       continue;
	    std::string const file = (*t == "uncompressed") ? filename : (filename + "." + *t);
	    if (FileExists(file))
	       return file;
	 }
	 return "";
   }
   std::map<std::string,std::string>::const_iterator const M = Options.find(Key);
   if (M == Options.end())
      return "";
   return M->second;
}
									/*}}}*/
bool IndexTarget::OptionBool(OptionKeys const EnumKey) const		/*{{{*/
{
   return StringToBool(Option(EnumKey), false);
}
									/*}}}*/
std::string IndexTarget::Format(std::string format) const		/*{{{*/
{
   for (std::map<std::string, std::string>::const_iterator O = Options.begin(); O != Options.end(); ++O)
   {
      format = SubstVar(format, std::string("$(") + O->first + ")", O->second);
   }
   format = SubstVar(format, "$(METAKEY)", MetaKey);
   format = SubstVar(format, "$(SHORTDESC)", ShortDesc);
   format = SubstVar(format, "$(DESCRIPTION)", Description);
   format = SubstVar(format, "$(URI)", URI);
   format = SubstVar(format, "$(FILENAME)", Option(IndexTarget::FILENAME));
   return format;
}
									/*}}}*/

pkgDebianIndexTargetFile::pkgDebianIndexTargetFile(IndexTarget const &Target, bool const Trusted) :/*{{{*/
   pkgDebianIndexFile(Trusted), d(NULL), Target(Target)
{
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::ArchiveURI(std::string const &File) const/*{{{*/
{
   return Target.Option(IndexTarget::REPO_URI) + pkgAcquire::URIEncode(File);
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::Describe(bool const Short) const	/*{{{*/
{
   if (Short)
      return Target.Description;
   return Target.Description + " (" + IndexFileName() + ")";
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::IndexFileName() const			/*{{{*/
{
   std::string const s = Target.Option(IndexTarget::FILENAME);
   if (FileExists(s))
      return s;

   std::vector<std::string> const types = VectorizeString(Target.Option(IndexTarget::COMPRESSIONTYPES), ' ');
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      std::string p = s + '.' + *t;
      if (FileExists(p))
         return p;
   }
   return s;
}
									/*}}}*/
unsigned long pkgDebianIndexTargetFile::Size() const				/*{{{*/
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
bool pkgDebianIndexTargetFile::Exists() const					/*{{{*/
{
   if (Target.OptionBool(IndexTarget::SHADOWED))
      return false;
   return FileExists(IndexFileName());
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::GetArchitecture() const			/*{{{*/
{
   return Target.Option(IndexTarget::ARCHITECTURE);
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::GetComponent() const			/*{{{*/
{
   return Target.Option(IndexTarget::COMPONENT);
}
									/*}}}*/
bool pkgDebianIndexTargetFile::OpenListFile(FileFd &Pkg, std::string const &FileName)/*{{{*/
{
   if (Pkg.Open(FileName, FileFd::ReadOnly, FileFd::Extension) == false)
      return _error->Error("Problem opening %s",FileName.c_str());
   return true;
}
									/*}}}*/
std::string pkgDebianIndexTargetFile::GetProgressDescription() const
{
   return Target.Description;
}
IndexTarget pkgDebianIndexTargetFile::GetIndexTarget() const
{
   return Target;
}

pkgDebianIndexRealFile::pkgDebianIndexRealFile(std::string const &pFile, bool const Trusted) :/*{{{*/
   pkgDebianIndexFile(Trusted), d(NULL)
{
   if (pFile.empty())
      ;
   else if (pFile == "/nonexistent/stdin")
      File = pFile;
   else
      File = flAbsPath(pFile);
}
									/*}}}*/
// IndexRealFile::Size - Return the size of the index			/*{{{*/
unsigned long pkgDebianIndexRealFile::Size() const
{
   struct stat S;
   if (stat(File.c_str(),&S) != 0)
      return 0;
   return S.st_size;
}
									/*}}}*/
bool pkgDebianIndexRealFile::Exists() const					/*{{{*/
{
   return FileExists(File);
}
									/*}}}*/
std::string pkgDebianIndexRealFile::Describe(bool const /*Short*/) const/*{{{*/
{
   return File;
}
									/*}}}*/
std::string pkgDebianIndexRealFile::ArchiveURI(std::string const &/*File*/) const/*{{{*/
{
   return "file:" + pkgAcquire::URIEncode(File);
}
									/*}}}*/
std::string pkgDebianIndexRealFile::IndexFileName() const			/*{{{*/
{
   return File;
}
									/*}}}*/
std::string pkgDebianIndexRealFile::GetProgressDescription() const
{
   return File;
}
bool pkgDebianIndexRealFile::OpenListFile(FileFd &Pkg, std::string const &FileName)/*{{{*/
{
   if (Pkg.Open(FileName, FileFd::ReadOnly, FileFd::Extension) == false)
      return _error->Error("Problem opening %s",FileName.c_str());
   return true;
}
									/*}}}*/

pkgDebianIndexFile::pkgDebianIndexFile(bool const Trusted) : pkgIndexFile(Trusted)
{
}
pkgDebianIndexFile::~pkgDebianIndexFile()
{
}
pkgCacheListParser * pkgDebianIndexFile::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return nullptr;
   _error->PushToStack();
   std::unique_ptr<pkgCacheListParser> Parser(new debListParser(&Pkg));
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   return newError ? nullptr : Parser.release();
}
bool pkgDebianIndexFile::Merge(pkgCacheGenerator &Gen,OpProgress * const Prog)
{
   std::string const PackageFile = IndexFileName();
   FileFd Pkg;
   if (OpenListFile(Pkg, PackageFile) == false)
      return false;
   _error->PushToStack();
   std::unique_ptr<pkgCacheListParser> Parser(CreateListParser(Pkg));
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (newError == false && Parser == nullptr)
      return true;
   if (Parser == NULL)
      return false;

   if (Prog != NULL)
      Prog->SubProgress(0, GetProgressDescription());

   if (Gen.SelectFile(PackageFile, *this, GetArchitecture(), GetComponent(), GetIndexFlags()) == false)
      return _error->Error("Problem with SelectFile %s",PackageFile.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator File = Gen.GetCurFile();
   pkgCacheGenerator::Dynamic<pkgCache::PkgFileIterator> DynFile(File);
   File->Size = Pkg.FileSize();
   File->mtime = Pkg.ModificationTime();

   if (Gen.MergeList(*Parser) == false)
      return _error->Error("Problem with MergeList %s",PackageFile.c_str());
   return true;
}
pkgCache::PkgFileIterator pkgDebianIndexFile::FindInCache(pkgCache &Cache) const
{
   std::string const FileName = IndexFileName();
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; ++File)
   {
       if (File.FileName() == NULL || FileName != File.FileName())
	 continue;

      struct stat St;
      if (stat(File.FileName(),&St) != 0)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "DebianIndexFile::FindInCache - stat failed on " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      if ((map_filesize_t)St.st_size != File->Size || St.st_mtime != File->mtime)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "DebianIndexFile::FindInCache - size (" << St.st_size << " <> " << File->Size
			<< ") or mtime (" << St.st_mtime << " <> " << File->mtime
			<< ") doesn't match for " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      return File;
   }

   return File;
}

pkgIndexFile::~pkgIndexFile() {}
pkgDebianIndexTargetFile::~pkgDebianIndexTargetFile() {}
pkgDebianIndexRealFile::~pkgDebianIndexRealFile() {}

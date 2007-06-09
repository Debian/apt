// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debindexfile.cc,v 1.5.2.3 2004/01/04 19:11:00 mdz Exp $
/* ######################################################################

   Debian Specific sources.list types and the three sorts of Debian
   index files.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/debindexfile.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/debrecords.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/debmetaindex.h>

#include <sys/stat.h>
									/*}}}*/

// SourcesIndex::debSourcesIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debSourcesIndex::debSourcesIndex(string URI,string Dist,string Section,bool Trusted) :
     pkgIndexFile(Trusted), URI(URI), Dist(Dist), Section(Section)
{
}
									/*}}}*/
// SourcesIndex::SourceInfo - Short 1 liner describing a source		/*{{{*/
// ---------------------------------------------------------------------
/* The result looks like:
     http://foo/ stable/main src 1.1.1 (dsc) */
string debSourcesIndex::SourceInfo(pkgSrcRecords::Parser const &Record,
				   pkgSrcRecords::File const &File) const
{
   string Res;
   Res = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Res += Dist;
   }      
   else
      Res += Dist + '/' + Section;
   
   Res += " ";
   Res += Record.Package();
   Res += " ";
   Res += Record.Version();
   if (File.Type.empty() == false)
      Res += " (" + File.Type + ")";
   return Res;
}
									/*}}}*/
// SourcesIndex::CreateSrcParser - Get a parser for the source files	/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSrcRecords::Parser *debSourcesIndex::CreateSrcParser() const
{
   string SourcesURI = URItoFileName(IndexURI("Sources"));
   return new debSrcRecordParser(_config->FindDir("Dir::State::lists") +
				 SourcesURI,this);
}
									/*}}}*/
// SourcesIndex::Describe - Give a descriptive path to the index	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debSourcesIndex::Describe(bool Short) const
{
   char S[300];
   if (Short == true)
      snprintf(S,sizeof(S),"%s",Info("Sources").c_str());
   else
      snprintf(S,sizeof(S),"%s (%s)",Info("Sources").c_str(),
	       IndexFile("Sources").c_str());
   
   return S;
}
									/*}}}*/
// SourcesIndex::Info - One liner describing the index URI		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debSourcesIndex::Info(const char *Type) const
{
   string Info = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Info += Dist;
   }
   else
      Info += Dist + '/' + Section;   
   Info += " ";
   Info += Type;
   return Info;
}
									/*}}}*/
// SourcesIndex::Index* - Return the URI to the index files		/*{{{*/
// ---------------------------------------------------------------------
/* */
inline string debSourcesIndex::IndexFile(const char *Type) const
{
   return URItoFileName(IndexURI(Type));
}
string debSourcesIndex::IndexURI(const char *Type) const
{
   string Res;
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Res = URI + Dist;
      else 
	 Res = URI;
   }
   else
      Res = URI + "dists/" + Dist + '/' + Section +
      "/source/";
   
   Res += Type;
   return Res;
}
									/*}}}*/
// SourcesIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debSourcesIndex::Exists() const
{
   return FileExists(IndexFile("Sources"));
}
									/*}}}*/
// SourcesIndex::Size - Return the size of the index			/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long debSourcesIndex::Size() const
{
   struct stat S;
   if (stat(IndexFile("Sources").c_str(),&S) != 0)
      return 0;
   return S.st_size;
}
									/*}}}*/

// PackagesIndex::debPackagesIndex - Contructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debPackagesIndex::debPackagesIndex(string URI,string Dist,string Section,bool Trusted) : 
                  pkgIndexFile(Trusted), URI(URI), Dist(Dist), Section(Section)
{
}
									/*}}}*/
// PackagesIndex::ArchiveInfo - Short version of the archive url	/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string debPackagesIndex::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   string Res = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Res += Dist;
   }
   else
      Res += Dist + '/' + Section;
   
   Res += " ";
   Res += Ver.ParentPkg().Name();
   Res += " ";
   Res += Ver.VerStr();
   return Res;
}
									/*}}}*/
// PackagesIndex::Describe - Give a descriptive path to the index	/*{{{*/
// ---------------------------------------------------------------------
/* This should help the user find the index in the sources.list and
   in the filesystem for problem solving */
string debPackagesIndex::Describe(bool Short) const
{   
   char S[300];
   if (Short == true)
      snprintf(S,sizeof(S),"%s",Info("Packages").c_str());
   else
      snprintf(S,sizeof(S),"%s (%s)",Info("Packages").c_str(),
	       IndexFile("Packages").c_str());
   return S;
}
									/*}}}*/
// PackagesIndex::Info - One liner describing the index URI		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debPackagesIndex::Info(const char *Type) const 
{
   string Info = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Info += Dist;
   }
   else
      Info += Dist + '/' + Section;   
   Info += " ";
   Info += Type;
   return Info;
}
									/*}}}*/
// PackagesIndex::Index* - Return the URI to the index files		/*{{{*/
// ---------------------------------------------------------------------
/* */
inline string debPackagesIndex::IndexFile(const char *Type) const
{
   return _config->FindDir("Dir::State::lists") + URItoFileName(IndexURI(Type));
}
string debPackagesIndex::IndexURI(const char *Type) const
{
   string Res;
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Res = URI + Dist;
      else 
	 Res = URI;
   }
   else
      Res = URI + "dists/" + Dist + '/' + Section +
      "/binary-" + _config->Find("APT::Architecture") + '/';
   
   Res += Type;
   return Res;
}
									/*}}}*/
// PackagesIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debPackagesIndex::Exists() const
{
   return FileExists(IndexFile("Packages"));
}
									/*}}}*/
// PackagesIndex::Size - Return the size of the index			/*{{{*/
// ---------------------------------------------------------------------
/* This is really only used for progress reporting. */
unsigned long debPackagesIndex::Size() const
{
   struct stat S;
   if (stat(IndexFile("Packages").c_str(),&S) != 0)
      return 0;
   return S.st_size;
}
									/*}}}*/
// PackagesIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debPackagesIndex::Merge(pkgCacheGenerator &Gen,OpProgress &Prog) const
{
   string PackageFile = IndexFile("Packages");
   FileFd Pkg(PackageFile,FileFd::ReadOnly);
   debListParser Parser(&Pkg);
   if (_error->PendingError() == true)
      return _error->Error("Problem opening %s",PackageFile.c_str());
   
   Prog.SubProgress(0,Info("Packages"));
   ::URI Tmp(URI);
   if (Gen.SelectFile(PackageFile,Tmp.Host,*this) == false)
      return _error->Error("Problem with SelectFile %s",PackageFile.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator File = Gen.GetCurFile();
   struct stat St;
   if (fstat(Pkg.Fd(),&St) != 0)
      return _error->Errno("fstat","Failed to stat");
   File->Size = St.st_size;
   File->mtime = St.st_mtime;
   
   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeList %s",PackageFile.c_str());

   // Check the release file
   string ReleaseFile = debReleaseIndex(URI,Dist).MetaIndexFile("Release");
   if (FileExists(ReleaseFile) == true)
   {
      FileFd Rel(ReleaseFile,FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return false;
      Parser.LoadReleaseInfo(File,Rel,Section);
   }
   
   return true;
}
									/*}}}*/
// PackagesIndex::FindInCache - Find this index				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator debPackagesIndex::FindInCache(pkgCache &Cache) const
{
   string FileName = IndexFile("Packages");
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; File++)
   {
       if (File.FileName() == NULL || FileName != File.FileName())
	 continue;
      
      struct stat St;
      if (stat(File.FileName(),&St) != 0)
	 return pkgCache::PkgFileIterator(Cache);
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
	 return pkgCache::PkgFileIterator(Cache);
      return File;
   }
   
   return File;
}
									/*}}}*/

// TranslationsIndex::debTranslationsIndex - Contructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
debTranslationsIndex::debTranslationsIndex(string URI,string Dist,string Section) : 
                  pkgIndexFile(true), URI(URI), Dist(Dist), Section(Section)
{
}
									/*}}}*/
// TranslationIndex::Trans* - Return the URI to the translation files	/*{{{*/
// ---------------------------------------------------------------------
/* */
inline string debTranslationsIndex::IndexFile(const char *Type) const
{
   return _config->FindDir("Dir::State::lists") + URItoFileName(IndexURI(Type));
}
string debTranslationsIndex::IndexURI(const char *Type) const
{
   string Res;
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Res = URI + Dist;
      else 
	 Res = URI;
   }
   else
      Res = URI + "dists/" + Dist + '/' + Section +
      "/i18n/Translation-";
   
   Res += Type;
   return Res;
}
									/*}}}*/
// TranslationsIndex::GetIndexes - Fetch the index files		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::GetIndexes(pkgAcquire *Owner) const
{
   if (TranslationsAvailable()) {
     string TranslationFile = "Translation-" + LanguageCode();
     new pkgAcqIndexTrans(Owner, IndexURI(LanguageCode().c_str()),
			  Info(TranslationFile.c_str()),
			  TranslationFile);
   }

   return true;
}
									/*}}}*/
// TranslationsIndex::Describe - Give a descriptive path to the index	/*{{{*/
// ---------------------------------------------------------------------
/* This should help the user find the index in the sources.list and
   in the filesystem for problem solving */
string debTranslationsIndex::Describe(bool Short) const
{   
   char S[300];
   if (Short == true)
      snprintf(S,sizeof(S),"%s",Info(TranslationFile().c_str()).c_str());
   else
      snprintf(S,sizeof(S),"%s (%s)",Info(TranslationFile().c_str()).c_str(),
	       IndexFile(LanguageCode().c_str()).c_str());
   return S;
}
									/*}}}*/
// TranslationsIndex::Info - One liner describing the index URI		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debTranslationsIndex::Info(const char *Type) const 
{
   string Info = ::URI::SiteOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Info += Dist;
   }
   else
      Info += Dist + '/' + Section;   
   Info += " ";
   Info += Type;
   return Info;
}
									/*}}}*/
bool debTranslationsIndex::HasPackages() const
{
   if(!TranslationsAvailable())
      return false;
   
   return FileExists(IndexFile(LanguageCode().c_str()));
}

// TranslationsIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::Exists() const
{
   return FileExists(IndexFile(LanguageCode().c_str()));
}
									/*}}}*/
// TranslationsIndex::Size - Return the size of the index		/*{{{*/
// ---------------------------------------------------------------------
/* This is really only used for progress reporting. */
unsigned long debTranslationsIndex::Size() const
{
   struct stat S;
   if (stat(IndexFile(LanguageCode().c_str()).c_str(),&S) != 0)
      return 0;
   return S.st_size;
}
									/*}}}*/
// TranslationsIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::Merge(pkgCacheGenerator &Gen,OpProgress &Prog) const
{
   // Check the translation file, if in use
   string TranslationFile = IndexFile(LanguageCode().c_str());
   if (TranslationsAvailable() && FileExists(TranslationFile))
   {
     FileFd Trans(TranslationFile,FileFd::ReadOnly);
     debListParser TransParser(&Trans);
     if (_error->PendingError() == true)
       return false;
     
     Prog.SubProgress(0, Info(TranslationFile.c_str()));
     if (Gen.SelectFile(TranslationFile,string(),*this) == false)
       return _error->Error("Problem with SelectFile %s",TranslationFile.c_str());

     // Store the IMS information
     pkgCache::PkgFileIterator TransFile = Gen.GetCurFile();
     struct stat TransSt;
     if (fstat(Trans.Fd(),&TransSt) != 0)
       return _error->Errno("fstat","Failed to stat");
     TransFile->Size = TransSt.st_size;
     TransFile->mtime = TransSt.st_mtime;
   
     if (Gen.MergeList(TransParser) == false)
       return _error->Error("Problem with MergeList %s",TranslationFile.c_str());
   }

   return true;
}
									/*}}}*/
// TranslationsIndex::FindInCache - Find this index				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator debTranslationsIndex::FindInCache(pkgCache &Cache) const
{
   string FileName = IndexFile(LanguageCode().c_str());
   
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; File++)
   {
      if (FileName != File.FileName())
	 continue;

      struct stat St;
      if (stat(File.FileName(),&St) != 0)
	 return pkgCache::PkgFileIterator(Cache);
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
	 return pkgCache::PkgFileIterator(Cache);
      return File;
   }   
   return File;
}
									/*}}}*/
// StatusIndex::debStatusIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debStatusIndex::debStatusIndex(string File) : pkgIndexFile(true), File(File)
{
}
									/*}}}*/
// StatusIndex::Size - Return the size of the index			/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long debStatusIndex::Size() const
{
   struct stat S;
   if (stat(File.c_str(),&S) != 0)
      return 0;
   return S.st_size;
}
									/*}}}*/
// StatusIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debStatusIndex::Merge(pkgCacheGenerator &Gen,OpProgress &Prog) const
{
   FileFd Pkg(File,FileFd::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   debListParser Parser(&Pkg);
   if (_error->PendingError() == true)
      return false;
   
   Prog.SubProgress(0,File);
   if (Gen.SelectFile(File,string(),*this,pkgCache::Flag::NotSource) == false)
      return _error->Error("Problem with SelectFile %s",File.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator CFile = Gen.GetCurFile();
   struct stat St;
   if (fstat(Pkg.Fd(),&St) != 0)
      return _error->Errno("fstat","Failed to stat");
   CFile->Size = St.st_size;
   CFile->mtime = St.st_mtime;
   CFile->Archive = Gen.WriteUniqString("now");
   
   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeList %s",File.c_str());   
   return true;
}
									/*}}}*/
// StatusIndex::FindInCache - Find this index				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator debStatusIndex::FindInCache(pkgCache &Cache) const
{
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; File++)
   {
      if (this->File != File.FileName())
	 continue;
      
      struct stat St;
      if (stat(File.FileName(),&St) != 0)
	 return pkgCache::PkgFileIterator(Cache);
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
	 return pkgCache::PkgFileIterator(Cache);
      return File;
   }   
   return File;
}
									/*}}}*/
// StatusIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debStatusIndex::Exists() const
{
   // Abort if the file does not exist.
   return true;
}
									/*}}}*/

// Index File types for Debian						/*{{{*/
class debIFTypeSrc : public pkgIndexFile::Type
{
   public:
   
   debIFTypeSrc() {Label = "Debian Source Index";};
};
class debIFTypePkg : public pkgIndexFile::Type
{
   public:
   
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const 
   {
      return new debRecordParser(File.FileName(),*File.Cache());
   };
   debIFTypePkg() {Label = "Debian Package Index";};
};
class debIFTypeTrans : public debIFTypePkg
{
   public:
   debIFTypeTrans() {Label = "Debian Translation Index";};
};
class debIFTypeStatus : public pkgIndexFile::Type
{
   public:
   
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const 
   {
      return new debRecordParser(File.FileName(),*File.Cache());
   };
   debIFTypeStatus() {Label = "Debian dpkg status file";};
};
static debIFTypeSrc _apt_Src;
static debIFTypePkg _apt_Pkg;
static debIFTypeTrans _apt_Trans;
static debIFTypeStatus _apt_Status;

const pkgIndexFile::Type *debSourcesIndex::GetType() const
{
   return &_apt_Src;
}
const pkgIndexFile::Type *debPackagesIndex::GetType() const
{
   return &_apt_Pkg;
}
const pkgIndexFile::Type *debTranslationsIndex::GetType() const
{
   return &_apt_Trans;
}
const pkgIndexFile::Type *debStatusIndex::GetType() const
{
   return &_apt_Status;
}

									/*}}}*/

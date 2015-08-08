// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debindexfile.cc,v 1.5.2.3 2004/01/04 19:11:00 mdz Exp $
/* ######################################################################

   Debian Specific sources.list types and the three sorts of Debian
   index files.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debindexfile.h>
#include <apt-pkg/debsrcrecords.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/debrecords.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/debmetaindex.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/sptr.h>

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
									/*}}}*/

using std::string;

// SourcesIndex::debSourcesIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debSourcesIndex::debSourcesIndex(IndexTarget const &Target,bool const Trusted) :
     pkgIndexTargetFile(Target, Trusted)
{
}
									/*}}}*/
// SourcesIndex::SourceInfo - Short 1 liner describing a source		/*{{{*/
// ---------------------------------------------------------------------
/* The result looks like:
     http://foo/debian/ stable/main src 1.1.1 (dsc) */
string debSourcesIndex::SourceInfo(pkgSrcRecords::Parser const &Record,
				   pkgSrcRecords::File const &File) const
{
   string Res = Target.Description;
   Res.erase(Target.Description.rfind(' '));

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
   string const SourcesURI = IndexFileName();
   if (FileExists(SourcesURI))
      return new debSrcRecordParser(SourcesURI, this);
   return NULL;
}
									/*}}}*/

// PackagesIndex::debPackagesIndex - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debPackagesIndex::debPackagesIndex(IndexTarget const &Target, bool const Trusted) :
                  pkgIndexTargetFile(Target, Trusted)
{
}
									/*}}}*/
// PackagesIndex::ArchiveInfo - Short version of the archive url	/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string debPackagesIndex::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   std::string const Dist = Target.Option(IndexTarget::RELEASE);
   string Res = Target.Option(IndexTarget::SITE) + " " + Dist;
   std::string const Component = Target.Option(IndexTarget::COMPONENT);
   if (Component.empty() == false)
      Res += "/" + Component;

   Res += " ";
   Res += Ver.ParentPkg().Name();
   Res += " ";
   if (Dist.empty() == false && Dist[Dist.size() - 1] != '/')
      Res.append(Ver.Arch()).append(" ");
   Res += Ver.VerStr();
   return Res;
}
									/*}}}*/
// PackagesIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debPackagesIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   string const PackageFile = IndexFileName();
   FileFd Pkg(PackageFile,FileFd::ReadOnly, FileFd::Extension);
   debListParser Parser(&Pkg, Target.Option(IndexTarget::ARCHITECTURE));

   if (_error->PendingError() == true)
      return _error->Error("Problem opening %s",PackageFile.c_str());
   if (Prog != NULL)
      Prog->SubProgress(0, Target.Description);


   std::string const URI = Target.Option(IndexTarget::REPO_URI);
   std::string Dist = Target.Option(IndexTarget::RELEASE);
   if (Dist.empty())
      Dist = "/";
   ::URI Tmp(URI);
   if (Gen.SelectFile(PackageFile, *this, Target.Option(IndexTarget::ARCHITECTURE), Target.Option(IndexTarget::COMPONENT)) == false)
      return _error->Error("Problem with SelectFile %s",PackageFile.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator File = Gen.GetCurFile();
   pkgCacheGenerator::Dynamic<pkgCache::PkgFileIterator> DynFile(File);
   File->Size = Pkg.FileSize();
   File->mtime = Pkg.ModificationTime();

   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeList %s",PackageFile.c_str());

   return true;
}
									/*}}}*/
// PackagesIndex::FindInCache - Find this index				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::PkgFileIterator debPackagesIndex::FindInCache(pkgCache &Cache) const
{
   string const FileName = IndexFileName();
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; ++File)
   {
       if (File.FileName() == NULL || FileName != File.FileName())
	 continue;
      
      struct stat St;
      if (stat(File.FileName(),&St) != 0)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "PackagesIndex::FindInCache - stat failed on " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "PackagesIndex::FindInCache - size (" << St.st_size << " <> " << File->Size
			<< ") or mtime (" << St.st_mtime << " <> " << File->mtime
			<< ") doesn't match for " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      return File;
   }
   
   return File;
}
									/*}}}*/

// TranslationsIndex::debTranslationsIndex - Constructor			/*{{{*/
debTranslationsIndex::debTranslationsIndex(IndexTarget const &Target) :
			pkgIndexTargetFile(Target, true)
{}
									/*}}}*/
bool debTranslationsIndex::HasPackages() const				/*{{{*/
{
   return Exists();
}
									/*}}}*/
// TranslationsIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   // Check the translation file, if in use
   string const TranslationFile = IndexFileName();
   if (FileExists(TranslationFile))
   {
     FileFd Trans(TranslationFile,FileFd::ReadOnly, FileFd::Extension);
     debTranslationsParser TransParser(&Trans);
     if (_error->PendingError() == true)
       return false;

     if (Prog != NULL)
	Prog->SubProgress(0, Target.Description);
     if (Gen.SelectFile(TranslationFile, *this, "", Target.Option(IndexTarget::COMPONENT), pkgCache::Flag::NotSource | pkgCache::Flag::NoPackages) == false)
       return _error->Error("Problem with SelectFile %s",TranslationFile.c_str());

     // Store the IMS information
     pkgCache::PkgFileIterator TransFile = Gen.GetCurFile();
     TransFile->Size = Trans.FileSize();
     TransFile->mtime = Trans.ModificationTime();

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
   string FileName = IndexFileName();

   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; ++File)
   {
      if (FileName != File.FileName())
	 continue;

      struct stat St;
      if (stat(File.FileName(),&St) != 0)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "TranslationIndex::FindInCache - stat failed on " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "TranslationIndex::FindInCache - size (" << St.st_size << " <> " << File->Size
			<< ") or mtime (" << St.st_mtime << " <> " << File->mtime
			<< ") doesn't match for " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
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
bool debStatusIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   FileFd Pkg(File,FileFd::ReadOnly, FileFd::Extension);
   if (_error->PendingError() == true)
      return false;
   debListParser Parser(&Pkg);
   if (_error->PendingError() == true)
      return false;

   if (Prog != NULL)
      Prog->SubProgress(0,File);
   if (Gen.SelectFile(File, *this, "", "now", pkgCache::Flag::NotSource) == false)
      return _error->Error("Problem with SelectFile %s",File.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator CFile = Gen.GetCurFile();
   pkgCacheGenerator::Dynamic<pkgCache::PkgFileIterator> DynFile(CFile);
   CFile->Size = Pkg.FileSize();
   CFile->mtime = Pkg.ModificationTime();

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
   for (; File.end() == false; ++File)
   {
      if (this->File != File.FileName())
	 continue;
      
      struct stat St;
      if (stat(File.FileName(),&St) != 0)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "StatusIndex::FindInCache - stat failed on " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      if ((unsigned)St.st_size != File->Size || St.st_mtime != File->mtime)
      {
         if (_config->FindB("Debug::pkgCacheGen", false))
	    std::clog << "StatusIndex::FindInCache - size (" << St.st_size << " <> " << File->Size
			<< ") or mtime (" << St.st_mtime << " <> " << File->mtime
			<< ") doesn't match for " << File.FileName() << std::endl;
	 return pkgCache::PkgFileIterator(Cache);
      }
      return File;
   }   
   return File;
}
									/*}}}*/
// StatusIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
APT_CONST bool debStatusIndex::Exists() const
{
   // Abort if the file does not exist.
   return true;
}
									/*}}}*/

// debDebPkgFile - Single .deb file					/*{{{*/
debDebPkgFileIndex::debDebPkgFileIndex(std::string DebFile)
   : pkgIndexFile(true), DebFile(DebFile)
{
   DebFileFullPath = flAbsPath(DebFile);
}

std::string debDebPkgFileIndex::ArchiveURI(std::string /*File*/) const
{
   return "file:" + DebFileFullPath;
}

bool debDebPkgFileIndex::Exists() const
{
   return FileExists(DebFile);
}
bool debDebPkgFileIndex::GetContent(std::ostream &content, std::string const &debfile)
{
   // get the control data out of the deb file via dpkg-deb -I
   std::string dpkg = _config->Find("Dir::Bin::dpkg","dpkg-deb");
   std::vector<const char *> Args;
   Args.push_back(dpkg.c_str());
   Args.push_back("-I");
   Args.push_back(debfile.c_str());
   Args.push_back("control");
   Args.push_back(NULL);
   FileFd PipeFd;
   pid_t Child;
   if(Popen((const char**)&Args[0], PipeFd, Child, FileFd::ReadOnly) == false)
      return _error->Error("Popen failed");

   char buffer[1024];
   do {
      unsigned long long actual = 0;
      if (PipeFd.Read(buffer, sizeof(buffer)-1, &actual) == false)
	 return _error->Errno("read", "Failed to read dpkg pipe");
      if (actual == 0)
	 break;
      buffer[actual] = '\0';
      content << buffer;
   } while(true);
   ExecWait(Child, "Popen");

   content << "Filename: " << debfile << "\n";
   struct stat Buf;
   if (stat(debfile.c_str(), &Buf) != 0)
      return false;
   content << "Size: " << Buf.st_size << "\n";

   return true;
}
bool debDebPkgFileIndex::Merge(pkgCacheGenerator& Gen, OpProgress* Prog) const
{
   if(Prog)
      Prog->SubProgress(0, "Reading deb file");

   // write the control data to a tempfile
   SPtr<FileFd> DebControl = GetTempFile("deb-file-" + flNotDir(DebFile));
   if(DebControl == NULL)
      return false;
   std::ostringstream content;
   if (GetContent(content, DebFile) == false)
      return false;
   std::string const contentstr = content.str();
   DebControl->Write(contentstr.c_str(), contentstr.length());
   // rewind for the listparser
   DebControl->Seek(0);

   // and give it to the list parser
   debDebFileParser Parser(DebControl, DebFile);
   if(Gen.SelectFile(DebFile, *this, "", "now", pkgCache::Flag::LocalSource) == false)
      return _error->Error("Problem with SelectFile %s", DebFile.c_str());

   pkgCache::PkgFileIterator File = Gen.GetCurFile();
   File->Size = DebControl->Size();
   File->mtime = DebControl->ModificationTime();

   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeLister for %s", DebFile.c_str());

   return true;
}
pkgCache::PkgFileIterator debDebPkgFileIndex::FindInCache(pkgCache &Cache) const
{
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; ++File)
   {
       if (File.FileName() == NULL || DebFile != File.FileName())
	 continue;

       return File;
   }
   
   return File;
}
unsigned long debDebPkgFileIndex::Size() const
{
   struct stat buf;
   if(stat(DebFile.c_str(), &buf) != 0)
      return 0;
   return buf.st_size;
}
									/*}}}*/

// debDscFileIndex stuff
debDscFileIndex::debDscFileIndex(std::string &DscFile) 
   : pkgIndexFile(true), DscFile(DscFile)
{
}

bool debDscFileIndex::Exists() const
{
   return FileExists(DscFile);
}

unsigned long debDscFileIndex::Size() const
{
   struct stat buf;
   if(stat(DscFile.c_str(), &buf) == 0)
      return buf.st_size;
   return 0;
}

// DscFileIndex::CreateSrcParser - Get a parser for the .dsc file	/*{{{*/
pkgSrcRecords::Parser *debDscFileIndex::CreateSrcParser() const
{
   if (!FileExists(DscFile))
      return NULL;

   return new debDscRecordParser(DscFile,this);
}
									/*}}}*/
// Index File types for Debian						/*{{{*/
class APT_HIDDEN debIFTypeSrc : public pkgIndexFile::Type
{
   public:
   
   debIFTypeSrc() {Label = "Debian Source Index";};
};
class APT_HIDDEN debIFTypePkg : public pkgIndexFile::Type
{
   public:
   
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const 
   {
      return new debRecordParser(File.FileName(),*File.Cache());
   };
   debIFTypePkg() {Label = "Debian Package Index";};
};
class APT_HIDDEN debIFTypeTrans : public debIFTypePkg
{
   public:
   debIFTypeTrans() {Label = "Debian Translation Index";};
};
class APT_HIDDEN debIFTypeStatus : public pkgIndexFile::Type
{
   public:
   
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const 
   {
      return new debRecordParser(File.FileName(),*File.Cache());
   };
   debIFTypeStatus() {Label = "Debian dpkg status file";};
};
class APT_HIDDEN debIFTypeDebPkgFile : public pkgIndexFile::Type
{
   public:
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator File) const 
   {
      return new debDebFileRecordParser(File.FileName());
   };
   debIFTypeDebPkgFile() {Label = "deb Package file";};
};
class APT_HIDDEN debIFTypeDscFile : public pkgIndexFile::Type
{
   public:
   virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string DscFile) const
   {
      return new debDscRecordParser(DscFile, NULL);
   };
   debIFTypeDscFile() {Label = "dsc File Source Index";};
};
class APT_HIDDEN debIFTypeDebianSourceDir : public pkgIndexFile::Type
{
   public:
   virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string SourceDir) const
   {
      return new debDscRecordParser(SourceDir + string("/debian/control"), NULL);
   };
   debIFTypeDebianSourceDir() {Label = "debian/control File Source Index";};
};

APT_HIDDEN debIFTypeSrc _apt_Src;
APT_HIDDEN debIFTypePkg _apt_Pkg;
APT_HIDDEN debIFTypeTrans _apt_Trans;
APT_HIDDEN debIFTypeStatus _apt_Status;
APT_HIDDEN debIFTypeDebPkgFile _apt_DebPkgFile;
// file based pseudo indexes
APT_HIDDEN debIFTypeDscFile _apt_DscFile;
APT_HIDDEN debIFTypeDebianSourceDir _apt_DebianSourceDir;

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
const pkgIndexFile::Type *debDebPkgFileIndex::GetType() const
{
   return &_apt_DebPkgFile;
}
const pkgIndexFile::Type *debDscFileIndex::GetType() const
{
   return &_apt_DscFile;
}
const pkgIndexFile::Type *debDebianSourceDirIndex::GetType() const
{
   return &_apt_DebianSourceDir;
}
									/*}}}*/

debStatusIndex::~debStatusIndex() {}
debPackagesIndex::~debPackagesIndex() {}
debTranslationsIndex::~debTranslationsIndex() {}
debSourcesIndex::~debSourcesIndex() {}

debDebPkgFileIndex::~debDebPkgFileIndex() {}
debDscFileIndex::~debDscFileIndex() {}

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
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>

#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

// Sources Index							/*{{{*/
debSourcesIndex::debSourcesIndex(IndexTarget const &Target,bool const Trusted) :
     pkgDebianIndexTargetFile(Target, Trusted), d(NULL)
{
}
std::string debSourcesIndex::SourceInfo(pkgSrcRecords::Parser const &Record,
				   pkgSrcRecords::File const &File) const
{
   // The result looks like: http://foo/debian/ stable/main src 1.1.1 (dsc)
   std::string Res = Target.Description;
   Res.erase(Target.Description.rfind(' '));

   Res += " ";
   Res += Record.Package();
   Res += " ";
   Res += Record.Version();
   if (File.Type.empty() == false)
      Res += " (" + File.Type + ")";
   return Res;
}
pkgSrcRecords::Parser *debSourcesIndex::CreateSrcParser() const
{
   std::string const SourcesURI = IndexFileName();
   if (FileExists(SourcesURI))
      return new debSrcRecordParser(SourcesURI, this);
   return NULL;
}
bool debSourcesIndex::OpenListFile(FileFd &, std::string const &)
{
   return true;
}
pkgCacheListParser * debSourcesIndex::CreateListParser(FileFd &)
{
   return NULL;
}
uint8_t debSourcesIndex::GetIndexFlags() const
{
   return 0;
}
									/*}}}*/
// Packages Index							/*{{{*/
debPackagesIndex::debPackagesIndex(IndexTarget const &Target, bool const Trusted) :
                  pkgDebianIndexTargetFile(Target, Trusted), d(NULL)
{
}
std::string debPackagesIndex::ArchiveInfo(pkgCache::VerIterator const &Ver) const
{
   std::string Res = Target.Description;
   {
      auto const space = Target.Description.rfind(' ');
      if (space != std::string::npos)
	 Res.erase(space);
   }

   Res += " ";
   Res += Ver.ParentPkg().Name();
   Res += " ";
   std::string const Dist = Target.Option(IndexTarget::RELEASE);
   if (Dist.empty() == false && Dist[Dist.size() - 1] != '/')
      Res.append(Ver.Arch()).append(" ");
   Res += Ver.VerStr();
   return Res;
}
uint8_t debPackagesIndex::GetIndexFlags() const
{
   return 0;
}
									/*}}}*/
// Translation-* Index							/*{{{*/
debTranslationsIndex::debTranslationsIndex(IndexTarget const &Target) :
			pkgDebianIndexTargetFile(Target, true), d(NULL)
{}
bool debTranslationsIndex::HasPackages() const
{
   return Exists();
}
bool debTranslationsIndex::OpenListFile(FileFd &Pkg, std::string const &FileName)
{
   if (FileExists(FileName))
      return pkgDebianIndexTargetFile::OpenListFile(Pkg, FileName);
   return true;
}
uint8_t debTranslationsIndex::GetIndexFlags() const
{
   return pkgCache::Flag::NotSource | pkgCache::Flag::NoPackages;
}
std::string debTranslationsIndex::GetArchitecture() const
{
   return std::string();
}
pkgCacheListParser * debTranslationsIndex::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return nullptr;
   _error->PushToStack();
   pkgCacheListParser * const Parser = new debTranslationsParser(&Pkg);
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (newError)
   {
      delete Parser;
      return nullptr;
   }
   else
      return Parser;
}
									/*}}}*/
// dpkg/status Index							/*{{{*/
debStatusIndex::debStatusIndex(std::string const &File) : pkgDebianIndexRealFile(File, true), d(NULL)
{
}
std::string debStatusIndex::GetArchitecture() const
{
   return std::string();
}
std::string debStatusIndex::GetComponent() const
{
   return "now";
}
uint8_t debStatusIndex::GetIndexFlags() const
{
   return pkgCache::Flag::NotSource;
}

pkgCacheListParser * debStatusIndex::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return nullptr;
   _error->PushToStack();
   pkgCacheListParser * const Parser = new debStatusListParser(&Pkg);
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (newError)
   {
      delete Parser;
      return nullptr;
   }
   else
      return Parser;
}
									/*}}}*/
// DebPkgFile Index - a single .deb file as an index			/*{{{*/
debDebPkgFileIndex::debDebPkgFileIndex(std::string const &DebFile)
   : pkgDebianIndexRealFile(DebFile, true), d(NULL), DebFile(DebFile)
{
}
bool debDebPkgFileIndex::GetContent(std::ostream &content, std::string const &debfile)
{
   struct stat Buf;
   if (stat(debfile.c_str(), &Buf) != 0)
      return false;

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

   content << "Filename: " << debfile << "\n";
   content << "Size: " << std::to_string(Buf.st_size) << "\n";
   bool first_line_seen = false;
   char buffer[1024];
   do {
      unsigned long long actual = 0;
      if (PipeFd.Read(buffer, sizeof(buffer)-1, &actual) == false)
	 return _error->Errno("read", "Failed to read dpkg pipe");
      if (actual == 0)
	 break;
      buffer[actual] = '\0';
      char const * b = buffer;
      if (first_line_seen == false)
      {
	 for (; *b != '\0' && (*b == '\n' || *b == '\r'); ++b)
	    /* skip over leading newlines */;
	 if (*b == '\0')
	    continue;
	 first_line_seen = true;
      }
      content << b;
   } while(true);
   ExecWait(Child, "Popen");

   return true;
}
bool debDebPkgFileIndex::OpenListFile(FileFd &Pkg, std::string const &FileName)
{
   // write the control data to a tempfile
   if (GetTempFile("deb-file-" + flNotDir(FileName), true, &Pkg) == NULL)
      return false;
   std::ostringstream content;
   if (GetContent(content, FileName) == false)
      return false;
   std::string const contentstr = content.str();
   if (contentstr.empty())
      return true;
   if (Pkg.Write(contentstr.c_str(), contentstr.length()) == false || Pkg.Seek(0) == false)
      return false;
   return true;
}
pkgCacheListParser * debDebPkgFileIndex::CreateListParser(FileFd &Pkg)
{
   if (Pkg.IsOpen() == false)
      return nullptr;
   _error->PushToStack();
   pkgCacheListParser * const Parser = new debDebFileParser(&Pkg, DebFile);
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (newError)
   {
      delete Parser;
      return nullptr;
   }
   else
      return Parser;
}
uint8_t debDebPkgFileIndex::GetIndexFlags() const
{
   return pkgCache::Flag::LocalSource;
}
std::string debDebPkgFileIndex::GetArchitecture() const
{
   return std::string();
}
std::string debDebPkgFileIndex::GetComponent() const
{
   return "local-deb";
}
pkgCache::PkgFileIterator debDebPkgFileIndex::FindInCache(pkgCache &Cache) const
{
   std::string const FileName = IndexFileName();
   pkgCache::PkgFileIterator File = Cache.FileBegin();
   for (; File.end() == false; ++File)
   {
       if (File.FileName() == NULL || FileName != File.FileName())
	 continue;
      // we can't do size checks here as file size != content size
      return File;
   }

   return File;
}
std::string debDebPkgFileIndex::ArchiveInfo_impl(pkgCache::VerIterator const &Ver) const
{
   std::string Res = IndexFileName() + " ";
   Res.append(Ver.ParentPkg().Name()).append(" ");
   Res.append(Ver.Arch()).append(" ");
   Res.append(Ver.VerStr());
   return Res;
}
									/*}}}*/
// DscFile Index - a single .dsc file as an index			/*{{{*/
debDscFileIndex::debDscFileIndex(std::string const &DscFile)
   : pkgDebianIndexRealFile(DscFile, true), d(NULL)
{
}
pkgSrcRecords::Parser *debDscFileIndex::CreateSrcParser() const
{
   if (Exists() == false)
      return NULL;
   return new debDscRecordParser(File, this);
}
std::string debDscFileIndex::GetComponent() const
{
   return "local-dsc";
}
std::string debDscFileIndex::GetArchitecture() const
{
   return "source";
}
uint8_t debDscFileIndex::GetIndexFlags() const
{
   return pkgCache::Flag::LocalSource;
}
									/*}}}*/
// ControlFile Index - a directory with a debian/control file		/*{{{*/
std::string debDebianSourceDirIndex::GetComponent() const
{
   return "local-control";
}
									/*}}}*/
// String Package Index - a string of Packages file content		/*{{{*/
std::string debStringPackageIndex::GetArchitecture() const
{
   return std::string();
}
std::string debStringPackageIndex::GetComponent() const
{
   return "apt-tmp-index";
}
uint8_t debStringPackageIndex::GetIndexFlags() const
{
   return pkgCache::Flag::NotSource;
}
const pkgIndexFile::Type *debStringPackageIndex::GetType() const
{
   return pkgIndexFile::Type::GetType("Debian Package Index");
}
debStringPackageIndex::debStringPackageIndex(std::string const &content) :
   pkgDebianIndexRealFile("", false), d(NULL)
{
   char fn[1024];
   std::string const tempdir = GetTempDir();
   snprintf(fn, sizeof(fn), "%s/%s.XXXXXX", tempdir.c_str(), "apt-tmp-index");
   int const fd = mkstemp(fn);
   File = fn;
   FileFd::Write(fd, content.data(), content.length());
   close(fd);
}
debStringPackageIndex::~debStringPackageIndex()
{
   RemoveFile("~debStringPackageIndex", File);
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
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &File) const APT_OVERRIDE
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
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &File) const APT_OVERRIDE
   {
      return new debRecordParser(File.FileName(),*File.Cache());
   };
   debIFTypeStatus() {Label = "Debian dpkg status file";};
};
class APT_HIDDEN debIFTypeDebPkgFile : public pkgIndexFile::Type
{
   public:
   virtual pkgRecords::Parser *CreatePkgParser(pkgCache::PkgFileIterator const &File) const APT_OVERRIDE
   {
      return new debDebFileRecordParser(File.FileName());
   };
   debIFTypeDebPkgFile() {Label = "Debian deb file";};
};
class APT_HIDDEN debIFTypeDscFile : public pkgIndexFile::Type
{
   public:
   virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string const &DscFile) const APT_OVERRIDE
   {
      return new debDscRecordParser(DscFile, NULL);
   };
   debIFTypeDscFile() {Label = "Debian dsc file";};
};
class APT_HIDDEN debIFTypeDebianSourceDir : public pkgIndexFile::Type
{
   public:
   virtual pkgSrcRecords::Parser *CreateSrcPkgParser(std::string const &SourceDir) const APT_OVERRIDE
   {
      return new debDscRecordParser(SourceDir + std::string("/debian/control"), NULL);
   };
   debIFTypeDebianSourceDir() {Label = "Debian control file";};
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

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
debSourcesIndex::debSourcesIndex(string URI,string Dist,string Section,bool Trusted) :
     pkgIndexFile(Trusted), URI(URI), Dist(Dist), Section(Section)
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
   string Res;
   Res = ::URI::ArchiveOnly(URI) + ' ';
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
   string SourcesURI = _config->FindDir("Dir::State::lists") + 
      URItoFileName(IndexURI("Sources"));

   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      string p;
      p = SourcesURI + '.' + *t;
      if (FileExists(p))
         return new debSrcRecordParser(p, this);
   }
   if (FileExists(SourcesURI))
      return new debSrcRecordParser(SourcesURI, this);
   return NULL;
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
   string Info = ::URI::ArchiveOnly(URI) + ' ';
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
string debSourcesIndex::IndexFile(const char *Type) const
{
   string s = URItoFileName(IndexURI(Type));

   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      string p = s + '.' + *t;
      if (FileExists(p))
         return p;
   }
   return s;
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
   unsigned long size = 0;

   /* we need to ignore errors here; if the lists are absent, just return 0 */
   _error->PushToStack();

   FileFd f(IndexFile("Sources"), FileFd::ReadOnly, FileFd::Extension);
   if (!f.Failed())
      size = f.Size();

   if (_error->PendingError() == true)
       size = 0;
   _error->RevertToStack();

   return size;
}
									/*}}}*/

// PackagesIndex::debPackagesIndex - Contructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debPackagesIndex::debPackagesIndex(string const &URI, string const &Dist, string const &Section,
					bool const &Trusted, string const &Arch) :
                  pkgIndexFile(Trusted), URI(URI), Dist(Dist), Section(Section), Architecture(Arch)
{
	if (Architecture == "native")
		Architecture = _config->Find("APT::Architecture");
}
									/*}}}*/
// PackagesIndex::ArchiveInfo - Short version of the archive url	/*{{{*/
// ---------------------------------------------------------------------
/* This is a shorter version that is designed to be < 60 chars or so */
string debPackagesIndex::ArchiveInfo(pkgCache::VerIterator Ver) const
{
   string Res = ::URI::ArchiveOnly(URI) + ' ';
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
   if (Dist[Dist.size() - 1] != '/')
      Res.append(Ver.Arch()).append(" ");
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
   string Info = ::URI::ArchiveOnly(URI) + ' ';
   if (Dist[Dist.size() - 1] == '/')
   {
      if (Dist != "/")
	 Info += Dist;
   }
   else
      Info += Dist + '/' + Section;   
   Info += " ";
   if (Dist[Dist.size() - 1] != '/')
      Info += Architecture + " ";
   Info += Type;
   return Info;
}
									/*}}}*/
// PackagesIndex::Index* - Return the URI to the index files		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debPackagesIndex::IndexFile(const char *Type) const
{
   string s =_config->FindDir("Dir::State::lists") + URItoFileName(IndexURI(Type));

   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      string p = s + '.' + *t;
      if (FileExists(p))
         return p;
   }
   return s;
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
      "/binary-" + Architecture + '/';
   
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
   unsigned long size = 0;

   /* we need to ignore errors here; if the lists are absent, just return 0 */
   _error->PushToStack();

   FileFd f(IndexFile("Packages"), FileFd::ReadOnly, FileFd::Extension);
   if (!f.Failed())
      size = f.Size();

   if (_error->PendingError() == true)
       size = 0;
   _error->RevertToStack();

   return size;
}
									/*}}}*/
// PackagesIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debPackagesIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   string PackageFile = IndexFile("Packages");
   FileFd Pkg(PackageFile,FileFd::ReadOnly, FileFd::Extension);
   debListParser Parser(&Pkg, Architecture);

   if (_error->PendingError() == true)
      return _error->Error("Problem opening %s",PackageFile.c_str());
   if (Prog != NULL)
      Prog->SubProgress(0,Info("Packages"));
   ::URI Tmp(URI);
   if (Gen.SelectFile(PackageFile,Tmp.Host,*this) == false)
      return _error->Error("Problem with SelectFile %s",PackageFile.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator File = Gen.GetCurFile();
   pkgCacheGenerator::Dynamic<pkgCache::PkgFileIterator> DynFile(File);
   File->Size = Pkg.FileSize();
   File->mtime = Pkg.ModificationTime();
   
   if (Gen.MergeList(Parser) == false)
      return _error->Error("Problem with MergeList %s",PackageFile.c_str());

   // Check the release file
   string ReleaseFile = debReleaseIndex(URI,Dist).MetaIndexFile("InRelease");
   bool releaseExists = false;
   if (FileExists(ReleaseFile) == true)
      releaseExists = true;
   else
      ReleaseFile = debReleaseIndex(URI,Dist).MetaIndexFile("Release");

   if (releaseExists == true || FileExists(ReleaseFile) == true)
   {
      FileFd Rel;
      // Beware: The 'Release' file might be clearsigned in case the
      // signature for an 'InRelease' file couldn't be checked
      if (OpenMaybeClearSignedFile(ReleaseFile, Rel) == false)
	 return false;

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

// TranslationsIndex::debTranslationsIndex - Contructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
debTranslationsIndex::debTranslationsIndex(std::string const &URI, std::string const &Dist,
						std::string const &Section, std::string const &Translation) :
			pkgIndexFile(true), URI(URI), Dist(Dist), Section(Section),
				Language(Translation)
{}
									/*}}}*/
// TranslationIndex::Trans* - Return the URI to the translation files	/*{{{*/
// ---------------------------------------------------------------------
/* */
string debTranslationsIndex::IndexFile(const char *Type) const
{
   string s =_config->FindDir("Dir::State::lists") + URItoFileName(IndexURI(Type));

   std::vector<std::string> types = APT::Configuration::getCompressionTypes();
   for (std::vector<std::string>::const_iterator t = types.begin(); t != types.end(); ++t)
   {
      string p = s + '.' + *t;
      if (FileExists(p))
         return p;
   }
   return s;
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
// TranslationsIndex::Describe - Give a descriptive path to the index	/*{{{*/
// ---------------------------------------------------------------------
/* This should help the user find the index in the sources.list and
   in the filesystem for problem solving */
string debTranslationsIndex::Describe(bool Short) const
{
   std::string S;
   if (Short == true)
      strprintf(S,"%s",Info(TranslationFile().c_str()).c_str());
   else
      strprintf(S,"%s (%s)",Info(TranslationFile().c_str()).c_str(),
	       IndexFile(Language.c_str()).c_str());
   return S;
}
									/*}}}*/
// TranslationsIndex::Info - One liner describing the index URI		/*{{{*/
// ---------------------------------------------------------------------
/* */
string debTranslationsIndex::Info(const char *Type) const 
{
   string Info = ::URI::ArchiveOnly(URI) + ' ';
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
bool debTranslationsIndex::HasPackages() const				/*{{{*/
{
   return FileExists(IndexFile(Language.c_str()));
}
									/*}}}*/
// TranslationsIndex::Exists - Check if the index is available		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::Exists() const
{
   return FileExists(IndexFile(Language.c_str()));
}
									/*}}}*/
// TranslationsIndex::Size - Return the size of the index		/*{{{*/
// ---------------------------------------------------------------------
/* This is really only used for progress reporting. */
unsigned long debTranslationsIndex::Size() const
{
   unsigned long size = 0;

   /* we need to ignore errors here; if the lists are absent, just return 0 */
   _error->PushToStack();

   FileFd f(IndexFile(Language.c_str()), FileFd::ReadOnly, FileFd::Extension);
   if (!f.Failed())
      size = f.Size();

   if (_error->PendingError() == true)
       size = 0;
   _error->RevertToStack();

   return size;
}
									/*}}}*/
// TranslationsIndex::Merge - Load the index file into a cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool debTranslationsIndex::Merge(pkgCacheGenerator &Gen,OpProgress *Prog) const
{
   // Check the translation file, if in use
   string TranslationFile = IndexFile(Language.c_str());
   if (FileExists(TranslationFile))
   {
     FileFd Trans(TranslationFile,FileFd::ReadOnly, FileFd::Extension);
     debTranslationsParser TransParser(&Trans);
     if (_error->PendingError() == true)
       return false;
     
     if (Prog != NULL)
	Prog->SubProgress(0, Info(TranslationFile.c_str()));
     if (Gen.SelectFile(TranslationFile,string(),*this) == false)
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
   string FileName = IndexFile(Language.c_str());
   
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
   if (Gen.SelectFile(File,string(),*this,pkgCache::Flag::NotSource) == false)
      return _error->Error("Problem with SelectFile %s",File.c_str());

   // Store the IMS information
   pkgCache::PkgFileIterator CFile = Gen.GetCurFile();
   CFile->Size = Pkg.FileSize();
   CFile->mtime = Pkg.ModificationTime();
   map_stringitem_t const storage = Gen.StoreString(pkgCacheGenerator::MIXED, "now");
   CFile->Archive = storage;
   
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
   if(Gen.SelectFile(DebFile, "local", *this, pkgCache::Flag::LocalSource) == false)
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexcopy.cc,v 1.10 2002/03/26 07:38:58 jgg Exp $
/* ######################################################################

   Index Copying - Aid for copying and verifying the index files
   
   This class helps apt-cache reconstruct a damaged index files. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/cdrom.h>
#include <apt-pkg/gpgv.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/debmetaindex.h>

#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include "indexcopy.h"
#include <apti18n.h>
									/*}}}*/

using namespace std;

// IndexCopy::CopyPackages - Copy the package files from the CD		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool IndexCopy::CopyPackages(string CDROM,string Name,vector<string> &List,
			     pkgCdromStatus *log)
{
   OpProgress *Progress = NULL;
   if (List.empty() == true)
      return true;
   
   if(log) 
      Progress = log->GetOpProgress();
   
   bool NoStat = _config->FindB("APT::CDROM::Fast",false);
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   
   // Prepare the progress indicator
   off_t TotalSize = 0;
   std::vector<APT::Configuration::Compressor> const compressor = APT::Configuration::getCompressors();
   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
   {
      struct stat Buf;
      bool found = false;
      std::string file = std::string(*I).append(GetFileName());
      for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	   c != compressor.end(); ++c)
      {
	 if (stat((file + c->Extension).c_str(), &Buf) != 0)
	    continue;
	 found = true;
	 break;
      }

      if (found == false)
	 return _error->Errno("stat", "Stat failed for %s", file.c_str());
      TotalSize += Buf.st_size;
   }

   off_t CurrentSize = 0;
   unsigned int NotFound = 0;
   unsigned int WrongSize = 0;
   unsigned int Packages = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
   {      
      string OrigPath = string(*I,CDROM.length());
      
      // Open the package file
      FileFd Pkg(*I + GetFileName(), FileFd::ReadOnly, FileFd::Auto);
      off_t const FileSize = Pkg.Size();

      pkgTagFile Parser(&Pkg);
      if (Pkg.IsOpen() == false || Pkg.Failed())
	 return false;
      
      // Open the output file
      char S[400];
      snprintf(S,sizeof(S),"cdrom:[%s]/%s%s",Name.c_str(),
	       (*I).c_str() + CDROM.length(),GetFileName());
      string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
      TargetF += URItoFileName(S);
      FileFd Target;
      if (_config->FindB("APT::CDROM::NoAct",false) == true)
      {
	 TargetF = "/dev/null";
         Target.Open(TargetF,FileFd::WriteExists);
      } else {
         Target.Open(TargetF,FileFd::WriteAtomic);
      }
      if (Target.IsOpen() == false || Target.Failed())
	 return false;

      // Setup the progress meter
      if(Progress)
	 Progress->OverallProgress(CurrentSize,TotalSize,FileSize,
				   string("Reading ") + Type() + " Indexes");

      // Parse
      if(Progress)
	 Progress->SubProgress(Pkg.Size());
      pkgTagSection Section;
      this->Section = &Section;
      string Prefix;
      unsigned long Hits = 0;
      unsigned long Chop = 0;
      while (Parser.Step(Section) == true)
      {
	 if(Progress)
	    Progress->Progress(Parser.Offset());
	 string File;
	 unsigned long long Size;
	 if (GetFile(File,Size) == false)
	    return false;

	 if (Chop != 0)
	    File = OrigPath + ChopDirs(File,Chop);

	 // See if the file exists
	 if (NoStat == false || Hits < 10)
	 {
	    // Attempt to fix broken structure
	    if (Hits == 0)
	    {
	       if (ReconstructPrefix(Prefix,OrigPath,CDROM,File) == false &&
		   ReconstructChop(Chop,*I,File) == false)
	       {
		  if (Debug == true)
		     clog << "Missed: " << File << endl;
		  NotFound++;
		  continue;
	       }
	       if (Chop != 0)
		  File = OrigPath + ChopDirs(File,Chop);
	    }

	    // Get the size
	    struct stat Buf;
	    if (stat((CDROM + Prefix + File).c_str(),&Buf) != 0 ||
		Buf.st_size == 0)
	    {
	       bool Mangled = false;
	       // Attempt to fix busted symlink support for one instance
	       string OrigFile = File;
	       string::size_type Start = File.find("binary-");
	       string::size_type End = File.find("/",Start+3);
	       if (Start != string::npos && End != string::npos)
	       {
		  File.replace(Start,End-Start,"binary-all");
		  Mangled = true;
	       }

	       if (Mangled == false ||
		   stat((CDROM + Prefix + File).c_str(),&Buf) != 0)
	       {
		  if (Debug == true)
		     clog << "Missed(2): " << OrigFile << endl;
		  NotFound++;
		  continue;
	       }
	    }

	    // Size match
	    if ((unsigned long long)Buf.st_size != Size)
	    {
	       if (Debug == true)
		  clog << "Wrong Size: " << File << endl;
	       WrongSize++;
	       continue;
	    }
	 }

	 Packages++;
	 Hits++;

	 if (RewriteEntry(Target, File) == false)
	    return false;
      }

      if (Debug == true)
	 cout << " Processed by using Prefix '" << Prefix << "' and chop " << Chop << endl;

      if (_config->FindB("APT::CDROM::NoAct",false) == false)
      {
	 // Move out of the partial directory
	 Target.Close();
	 string FinalF = _config->FindDir("Dir::State::lists");
	 FinalF += URItoFileName(S);
	 if (rename(TargetF.c_str(),FinalF.c_str()) != 0)
	    return _error->Errno("rename","Failed to rename");
	 ChangeOwnerAndPermissionOfFile("CopyPackages", FinalF.c_str(), "root", ROOT_GROUP, 0644);
      }

      /* Mangle the source to be in the proper notation with
	 prefix dist [component] */
      *I = string(*I,Prefix.length());
      ConvertToSourceList(CDROM,*I);
      *I = Prefix + ' ' + *I;

      CurrentSize += FileSize;
   }
   if(Progress)
      Progress->Done();

   // Some stats
   if(log) {
      stringstream msg;
      if(NotFound == 0 && WrongSize == 0)
	 ioprintf(msg, _("Wrote %i records.\n"), Packages);
      else if (NotFound != 0 && WrongSize == 0)
	 ioprintf(msg, _("Wrote %i records with %i missing files.\n"),
		  Packages, NotFound);
      else if (NotFound == 0 && WrongSize != 0)
	 ioprintf(msg, _("Wrote %i records with %i mismatched files\n"),
		  Packages, WrongSize);
      if (NotFound != 0 && WrongSize != 0)
	 ioprintf(msg, _("Wrote %i records with %i missing files and %i mismatched files\n"), Packages, NotFound, WrongSize);
   }

   if (Packages == 0)
      _error->Warning("No valid records were found.");

   if (NotFound + WrongSize > 10)
      _error->Warning("A lot of entries were discarded, something may be wrong.\n");

   return true;
}
									/*}}}*/
// IndexCopy::ChopDirs - Chop off the leading directory components	/*{{{*/
// ---------------------------------------------------------------------
/* */
string IndexCopy::ChopDirs(string Path,unsigned int Depth)
{
   string::size_type I = 0;
   do
   {
      I = Path.find('/',I+1);
      Depth--;
   }
   while (I != string::npos && Depth != 0);

   if (I == string::npos)
      return string();

   return string(Path,I+1);
}
									/*}}}*/
// IndexCopy::ReconstructPrefix - Fix strange prefixing			/*{{{*/
// ---------------------------------------------------------------------
/* This prepends dir components from the path to the package files to
   the path to the deb until it is found */
bool IndexCopy::ReconstructPrefix(string &Prefix,string OrigPath,string CD,
				  string File)
{
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   unsigned int Depth = 1;
   string MyPrefix = Prefix;
   while (1)
   {
      struct stat Buf;
      if (stat((CD + MyPrefix + File).c_str(),&Buf) != 0)
      {
	 if (Debug == true)
	    cout << "Failed, " << CD + MyPrefix + File << endl;
	 if (GrabFirst(OrigPath,MyPrefix,Depth++) == true)
	    continue;
	 
	 return false;
      }
      else
      {
	 Prefix = MyPrefix;
	 return true;
      }      
   }
   return false;
}
									/*}}}*/
// IndexCopy::ReconstructChop - Fixes bad source paths			/*{{{*/
// ---------------------------------------------------------------------
/* This removes path components from the filename and prepends the location
   of the package files until a file is found */
bool IndexCopy::ReconstructChop(unsigned long &Chop,string Dir,string File)
{
   // Attempt to reconstruct the filename
   unsigned long Depth = 0;
   while (1)
   {
      struct stat Buf;
      if (stat((Dir + File).c_str(),&Buf) != 0)
      {
	 File = ChopDirs(File,1);
	 Depth++;
	 if (File.empty() == false)
	    continue;
	 return false;
      }
      else
      {
	 Chop = Depth;
	 return true;
      }
   }
   return false;
}
									/*}}}*/
// IndexCopy::ConvertToSourceList - Convert a Path to a sourcelist 	/*{{{*/
// ---------------------------------------------------------------------
/* We look for things in dists/ notation and convert them to 
   <dist> <component> form otherwise it is left alone. This also strips
   the CD path. 
 
   This implements a regex sort of like: 
    (.*)/dists/([^/]*)/(.*)/binary-* 
     ^          ^      ^- Component
     |          |-------- Distribution
     |------------------- Path
   
   It was deciced to use only a single word for dist (rather than say
   unstable/non-us) to increase the chance that each CD gets a single
   line in sources.list.
 */
void IndexCopy::ConvertToSourceList(string CD,string &Path)
{
   // Strip the cdrom base path
   Path = string(Path,CD.length());
   if (Path.empty() == true)
      Path = "/";
   
   // Too short to be a dists/ type
   if (Path.length() < strlen("dists/"))
      return;
   
   // Not a dists type.
   if (stringcmp(Path.c_str(),Path.c_str()+strlen("dists/"),"dists/") != 0)
      return;
      
   // Isolate the dist
   string::size_type Slash = strlen("dists/");
   string::size_type Slash2 = Path.find('/',Slash + 1);
   if (Slash2 == string::npos || Slash2 + 2 >= Path.length())
      return;
   string Dist = string(Path,Slash,Slash2 - Slash);
   
   // Isolate the component
   Slash = Slash2;
   for (unsigned I = 0; I != 10; I++)
   {
      Slash = Path.find('/',Slash+1);
      if (Slash == string::npos || Slash + 2 >= Path.length())
	 return;
      string Comp = string(Path,Slash2+1,Slash - Slash2-1);
	 
      // Verify the trailing binary- bit
      string::size_type BinSlash = Path.find('/',Slash + 1);
      if (Slash == string::npos)
	 return;
      string Binary = string(Path,Slash+1,BinSlash - Slash-1);
      
      if (strncmp(Binary.c_str(), "binary-", strlen("binary-")) == 0)
      {
	 Binary.erase(0, strlen("binary-"));
	 if (APT::Configuration::checkArchitecture(Binary) == false)
	    continue;
      }
      else if (Binary != "source")
	 continue;

      Path = Dist + ' ' + Comp;
      return;
   }   
}
									/*}}}*/
// IndexCopy::GrabFirst - Return the first Depth path components	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool IndexCopy::GrabFirst(string Path,string &To,unsigned int Depth)
{
   string::size_type I = 0;
   do
   {
      I = Path.find('/',I+1);
      Depth--;
   }
   while (I != string::npos && Depth != 0);
   
   if (I == string::npos)
      return false;

   To = string(Path,0,I+1);
   return true;
}
									/*}}}*/
// PackageCopy::GetFile - Get the file information from the section	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool PackageCopy::GetFile(string &File,unsigned long long &Size)
{
   File = Section->FindS("Filename");
   Size = Section->FindI("Size");
   if (File.empty() || Size == 0)
      return _error->Error("Cannot find filename or size tag");
   return true;
}
									/*}}}*/
// PackageCopy::RewriteEntry - Rewrite the entry with a new filename	/*{{{*/
bool PackageCopy::RewriteEntry(FileFd &Target,string const &File)
{
   std::vector<pkgTagSection::Tag> Changes;
   Changes.push_back(pkgTagSection::Tag::Rewrite("Filename", File));

   if (Section->Write(Target, TFRewritePackageOrder, Changes) == false)
      return false;
   return Target.Write("\n", 1);
}
									/*}}}*/
// SourceCopy::GetFile - Get the file information from the section	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SourceCopy::GetFile(string &File,unsigned long long &Size)
{
   string Files = Section->FindS("Files");
   if (Files.empty() == true)
      return false;

   // Stash the / terminated directory prefix
   string Base = Section->FindS("Directory");
   if (Base.empty() == false && Base[Base.length()-1] != '/')
      Base += '/';
   
   // Read the first file triplet
   const char *C = Files.c_str();
   string sSize;
   string MD5Hash;
   
   // Parse each of the elements
   if (ParseQuoteWord(C,MD5Hash) == false ||
       ParseQuoteWord(C,sSize) == false ||
       ParseQuoteWord(C,File) == false)
      return _error->Error("Error parsing file record");
   
   // Parse the size and append the directory
   Size = strtoull(sSize.c_str(), NULL, 10);
   File = Base + File;
   return true;
}
									/*}}}*/
// SourceCopy::RewriteEntry - Rewrite the entry with a new filename	/*{{{*/
bool SourceCopy::RewriteEntry(FileFd &Target, std::string const &File)
{
   string const Dir(File,0,File.rfind('/'));
   std::vector<pkgTagSection::Tag> Changes;
   Changes.push_back(pkgTagSection::Tag::Rewrite("Directory", Dir));

   if (Section->Write(Target, TFRewriteSourceOrder, Changes) == false)
      return false;
   return Target.Write("\n", 1);
}
									/*}}}*/
// SigVerify::Verify - Verify a files md5sum against its metaindex	/*{{{*/
bool SigVerify::Verify(string prefix, string file, metaIndex *MetaIndex)
{
   const metaIndex::checkSum *Record = MetaIndex->Lookup(file);
   bool const Debug = _config->FindB("Debug::aptcdrom",false);

   // we skip non-existing files in the verifcation of the Release file
   // as non-existing files do not harm, but a warning scares people and
   // makes it hard to strip unneeded files from an ISO like uncompressed
   // indexes as it is done on the mirrors (see also LP: #255545 )
   if(!RealFileExists(prefix+file))
   {
      if (Debug == true)
	 cout << "Skipping nonexistent in " << prefix << " file " << file << std::endl;
      return true;
   }

   if (!Record)
   {
      _error->Warning(_("Can't find authentication record for: %s"), file.c_str());
      return false;
   }

   if (!Record->Hashes.VerifyFile(prefix+file))
   {
      _error->Warning(_("Hash mismatch for: %s"),file.c_str());
      return false;
   }

   if(Debug == true)
   {
      cout << "File: " << prefix+file << endl
	 << "Expected Hash " << endl;
      for (HashStringList::const_iterator hs = Record->Hashes.begin(); hs != Record->Hashes.end(); ++hs)
	 std::cout <<  "\t- " << hs->toStr() << std::endl;
   }

   return true;
}
									/*}}}*/
bool SigVerify::CopyMetaIndex(string CDROM, string CDName,		/*{{{*/
			      string prefix, string file)
{
      char S[400];
      snprintf(S,sizeof(S),"cdrom:[%s]/%s%s",CDName.c_str(),
	       (prefix).c_str() + CDROM.length(),file.c_str());
      string TargetF = _config->FindDir("Dir::State::lists");
      TargetF += URItoFileName(S);

      FileFd Target;
      FileFd Rel;
      Target.Open(TargetF,FileFd::WriteAtomic);
      Rel.Open(prefix + file,FileFd::ReadOnly);
      if (CopyFile(Rel,Target) == false || Target.Close() == false)
	 return _error->Error("Copying of '%s' for '%s' from '%s' failed", file.c_str(), CDName.c_str(), prefix.c_str());
      ChangeOwnerAndPermissionOfFile("CopyPackages", TargetF.c_str(), "root", ROOT_GROUP, 0644);

      return true;
}
									/*}}}*/
bool SigVerify::CopyAndVerify(string CDROM,string Name,vector<string> &SigList,	/*{{{*/
			      vector<string> /*PkgList*/,vector<string> /*SrcList*/)
{
   if (SigList.empty() == true)
      return true;

   bool Debug = _config->FindB("Debug::aptcdrom",false);

   // Read all Release files
   for (vector<string>::iterator I = SigList.begin(); I != SigList.end(); ++I)
   {
      if(Debug)
	 cout << "Signature verify for: " << *I << endl;

      metaIndex *MetaIndex = new debReleaseIndex("","", {});
      string prefix = *I; 

      string const releasegpg = *I+"Release.gpg";
      string const release = *I+"Release";
      string const inrelease = *I+"InRelease";
      bool useInRelease = true;

      // a Release.gpg without a Release should never happen
      if (RealFileExists(inrelease) == true)
	 ;
      else if(RealFileExists(release) == false || RealFileExists(releasegpg) == false)
      {
	 delete MetaIndex;
	 continue;
      }
      else
	 useInRelease = false;

      pid_t pid = ExecFork();
      if(pid < 0) {
	 _error->Error("Fork failed");
	 return false;
      }
      if(pid == 0)
      {
	 if (useInRelease == true)
	    ExecGPGV(inrelease, inrelease);
	 else
	    ExecGPGV(release, releasegpg);
      }

      if(!ExecWait(pid, "gpgv")) {
	 _error->Warning("Signature verification failed for: %s",
			 (useInRelease ? inrelease.c_str() : releasegpg.c_str()));
	 // something went wrong, don't copy the Release.gpg
	 // FIXME: delete any existing gpg file?
	 delete MetaIndex;
	 continue;
      }

      // Open the Release file and add it to the MetaIndex
      std::string ErrorText;
      if(MetaIndex->Load(release, &ErrorText) == false)
      {
	 _error->Error("%s", ErrorText.c_str());
	 return false;
      }

      // go over the Indexfiles and see if they verify
      // if so, remove them from our copy of the lists
      vector<string> keys = MetaIndex->MetaKeys();
      for (vector<string>::iterator I = keys.begin(); I != keys.end(); ++I)
      { 
	 if(!Verify(prefix,*I, MetaIndex)) {
	    // something went wrong, don't copy the Release.gpg
	    // FIXME: delete any existing gpg file?
	    _error->Discard();
	    continue;	 
	 }
      }

      // we need a fresh one for the Release.gpg
      delete MetaIndex;
   
      // everything was fine, copy the Release and Release.gpg file
      if (useInRelease == true)
	 CopyMetaIndex(CDROM, Name, prefix, "InRelease");
      else
      {
	 CopyMetaIndex(CDROM, Name, prefix, "Release");
	 CopyMetaIndex(CDROM, Name, prefix, "Release.gpg");
      }
   }   

   return true;
}
									/*}}}*/
// SigVerify::RunGPGV - deprecated wrapper calling ExecGPGV		/*{{{*/
APT_NORETURN bool SigVerify::RunGPGV(std::string const &File, std::string const &FileOut,
      int const &statusfd, int fd[2]) {
   ExecGPGV(File, FileOut, statusfd, fd);
}
APT_NORETURN bool SigVerify::RunGPGV(std::string const &File, std::string const &FileOut,
      int const &statusfd) {
   ExecGPGV(File, FileOut, statusfd);
}
									/*}}}*/
bool TranslationsCopy::CopyTranslations(string CDROM,string Name,	/*{{{*/
				vector<string> &List, pkgCdromStatus *log)
{
   OpProgress *Progress = NULL;
   if (List.empty() == true)
      return true;
   
   if(log) 
      Progress = log->GetOpProgress();
   
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   
   // Prepare the progress indicator
   off_t TotalSize = 0;
   std::vector<APT::Configuration::Compressor> const compressor = APT::Configuration::getCompressors();
   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
   {
      struct stat Buf;
      bool found = false;
      std::string file = *I;
      for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	   c != compressor.end(); ++c)
      {
	 if (stat((file + c->Extension).c_str(), &Buf) != 0)
	    continue;
	 found = true;
	 break;
      }

      if (found == false)
	 return _error->Errno("stat", "Stat failed for %s", file.c_str());
      TotalSize += Buf.st_size;
   }

   off_t CurrentSize = 0;
   unsigned int NotFound = 0;
   unsigned int WrongSize = 0;
   unsigned int Packages = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
   {
      // Open the package file
      FileFd Pkg(*I, FileFd::ReadOnly, FileFd::Auto);
      off_t const FileSize = Pkg.Size();

      pkgTagFile Parser(&Pkg);
      if (Pkg.IsOpen() == false || Pkg.Failed())
	 return false;

      // Open the output file
      char S[400];
      snprintf(S,sizeof(S),"cdrom:[%s]/%s",Name.c_str(),
	       (*I).c_str() + CDROM.length());
      string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
      TargetF += URItoFileName(S);
      FileFd Target;
      if (_config->FindB("APT::CDROM::NoAct",false) == true)
      {
	 TargetF = "/dev/null";
	 Target.Open(TargetF,FileFd::WriteExists);
      } else {
	 Target.Open(TargetF,FileFd::WriteAtomic);
      }
      if (Pkg.IsOpen() == false || Pkg.Failed())
	 return false;

      // Setup the progress meter
      if(Progress)
	 Progress->OverallProgress(CurrentSize,TotalSize,FileSize,
				   string("Reading Translation Indexes"));

      // Parse
      if(Progress)
	 Progress->SubProgress(Pkg.Size());
      pkgTagSection Section;
      this->Section = &Section;
      string Prefix;
      unsigned long Hits = 0;
      while (Parser.Step(Section) == true)
      {
	 if(Progress)
	    Progress->Progress(Parser.Offset());

	 if (Section.Write(Target) == false || Target.Write("\n", 1) == false)
	    return false;

	 Packages++;
	 Hits++;
      }

      if (Debug == true)
	 cout << " Processed by using Prefix '" << Prefix << "' and chop " << endl;

      if (_config->FindB("APT::CDROM::NoAct",false) == false)
      {
	 // Move out of the partial directory
	 Target.Close();
	 string FinalF = _config->FindDir("Dir::State::lists");
	 FinalF += URItoFileName(S);
	 if (rename(TargetF.c_str(),FinalF.c_str()) != 0)
	    return _error->Errno("rename","Failed to rename");
	 ChangeOwnerAndPermissionOfFile("CopyTranslations", FinalF.c_str(), "root", ROOT_GROUP, 0644);
      }

      CurrentSize += FileSize;
   }
   if(Progress)
      Progress->Done();

   // Some stats
   if(log) {
      stringstream msg;
      if(NotFound == 0 && WrongSize == 0)
	 ioprintf(msg, _("Wrote %i records.\n"), Packages);
      else if (NotFound != 0 && WrongSize == 0)
	 ioprintf(msg, _("Wrote %i records with %i missing files.\n"),
		  Packages, NotFound);
      else if (NotFound == 0 && WrongSize != 0)
	 ioprintf(msg, _("Wrote %i records with %i mismatched files\n"),
		  Packages, WrongSize);
      if (NotFound != 0 && WrongSize != 0)
	 ioprintf(msg, _("Wrote %i records with %i missing files and %i mismatched files\n"), Packages, NotFound, WrongSize);
   }

   if (Packages == 0)
      _error->Warning("No valid records were found.");

   if (NotFound + WrongSize > 10)
      _error->Warning("A lot of entries were discarded, something may be wrong.\n");

   return true;
}
									/*}}}*/

IndexCopy::IndexCopy() : d(nullptr), Section(nullptr) {}
APT_CONST IndexCopy::~IndexCopy() {}

PackageCopy::PackageCopy() : IndexCopy(), d(NULL) {}
APT_CONST PackageCopy::~PackageCopy() {}
SourceCopy::SourceCopy() : IndexCopy(), d(NULL) {}
APT_CONST SourceCopy::~SourceCopy() {}
TranslationsCopy::TranslationsCopy() : d(nullptr), Section(nullptr) {}
APT_CONST TranslationsCopy::~TranslationsCopy() {}
SigVerify::SigVerify() : d(NULL) {}
APT_CONST SigVerify::~SigVerify() {}

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: indexcopy.cc,v 1.10 2002/03/26 07:38:58 jgg Exp $
/* ######################################################################

   Index Copying - Aid for copying and verifying the index files
   
   This class helps apt-cache reconstruct a damaged index files. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include "indexcopy.h"

#include <apt-pkg/error.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/cdrom.h>
#include <apti18n.h>

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
									/*}}}*/

using namespace std;



// IndexCopy::CopyPackages - Copy the package files from the CD		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool IndexCopy::CopyPackages(string CDROM,string Name,vector<string> &List,
			     pkgCdromStatus *log)
{
   OpProgress *Progress = NULL;
   if (List.size() == 0)
      return true;
   
   if(log) 
      Progress = log->GetOpProgress();
   
   bool NoStat = _config->FindB("APT::CDROM::Fast",false);
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   
   // Prepare the progress indicator
   unsigned long TotalSize = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      struct stat Buf;
      if (stat(string(*I + GetFileName()).c_str(),&Buf) != 0 &&
	  stat(string(*I + GetFileName() + ".gz").c_str(),&Buf) != 0)
	 return _error->Errno("stat","Stat failed for %s",
			      string(*I + GetFileName()).c_str());
      TotalSize += Buf.st_size;
   }	

   unsigned long CurrentSize = 0;
   unsigned int NotFound = 0;
   unsigned int WrongSize = 0;
   unsigned int Packages = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {      
      string OrigPath = string(*I,CDROM.length());
      unsigned long FileSize = 0;
      
      // Open the package file
      FileFd Pkg;
      if (FileExists(*I + GetFileName()) == true)
      {
	 Pkg.Open(*I + GetFileName(),FileFd::ReadOnly);
	 FileSize = Pkg.Size();
      }      
      else
      {
	 FileFd From(*I + GetFileName() + ".gz",FileFd::ReadOnly);
	 if (_error->PendingError() == true)
	    return false;
	 FileSize = From.Size();
	 
	 // Get a temp file
	 FILE *tmp = tmpfile();
	 if (tmp == 0)
	    return _error->Errno("tmpfile","Unable to create a tmp file");
	 Pkg.Fd(dup(fileno(tmp)));
	 fclose(tmp);
	 
	 // Fork gzip
	 pid_t Process = fork();
	 if (Process < 0)
	    return _error->Errno("fork","Couldn't fork gzip");
	 
	 // The child
	 if (Process == 0)
	 {	    
	    dup2(From.Fd(),STDIN_FILENO);
	    dup2(Pkg.Fd(),STDOUT_FILENO);
	    SetCloseExec(STDIN_FILENO,false);
	    SetCloseExec(STDOUT_FILENO,false);
	    
	    const char *Args[3];
	    string Tmp =  _config->Find("Dir::bin::gzip","gzip");
	    Args[0] = Tmp.c_str();
	    Args[1] = "-d";
	    Args[2] = 0;
	    execvp(Args[0],(char **)Args);
	    exit(100);
	 }
	 
	 // Wait for gzip to finish
	 if (ExecWait(Process,_config->Find("Dir::bin::gzip","gzip").c_str(),false) == false)
	    return _error->Error("gzip failed, perhaps the disk is full.");
	 
	 Pkg.Seek(0);
      }
      pkgTagFile Parser(&Pkg);
      if (_error->PendingError() == true)
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
      FILE *TargetFl = fdopen(dup(Target.Fd()),"w");
      if (_error->PendingError() == true)
	 return false;
      if (TargetFl == 0)
	 return _error->Errno("fdopen","Failed to reopen fd");
      
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
	 unsigned long Size;
	 if (GetFile(File,Size) == false)
	 {
	    fclose(TargetFl);
	    return false;
	 }
	 
	 if (Chop != 0)
	    File = OrigPath + ChopDirs(File,Chop);
	 
	 // See if the file exists
	 bool Mangled = false;
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
	    if (stat(string(CDROM + Prefix + File).c_str(),&Buf) != 0 || 
		Buf.st_size == 0)
	    {
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
		   stat(string(CDROM + Prefix + File).c_str(),&Buf) != 0)
	       {
		  if (Debug == true)
		     clog << "Missed(2): " << OrigFile << endl;
		  NotFound++;
		  continue;
	       }	       
	    }	    
	    			    	    
	    // Size match
	    if ((unsigned)Buf.st_size != Size)
	    {
	       if (Debug == true)
		  clog << "Wrong Size: " << File << endl;
	       WrongSize++;
	       continue;
	    }
	 }
	 
	 Packages++;
	 Hits++;
	 
	 if (RewriteEntry(TargetFl,File) == false)
	 {
	    fclose(TargetFl);
	    return false;
	 }
      }
      fclose(TargetFl);

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
      if (stat(string(CD + MyPrefix + File).c_str(),&Buf) != 0)
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
      if (stat(string(Dir + File).c_str(),&Buf) != 0)
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
   char S[300];
   snprintf(S,sizeof(S),"binary-%s",_config->Find("Apt::Architecture").c_str());
   
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
      
      if (Binary != S && Binary != "source")
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
bool PackageCopy::GetFile(string &File,unsigned long &Size)
{
   File = Section->FindS("Filename");
   Size = Section->FindI("Size");
   if (File.empty() || Size == 0)
      return _error->Error("Cannot find filename or size tag");
   return true;
}
									/*}}}*/
// PackageCopy::RewriteEntry - Rewrite the entry with a new filename	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool PackageCopy::RewriteEntry(FILE *Target,string File)
{
   TFRewriteData Changes[] = {{"Filename",File.c_str()},
                              {}};
   
   if (TFRewrite(Target,*Section,TFRewritePackageOrder,Changes) == false)
      return false;
   fputc('\n',Target);
   return true;
}
									/*}}}*/
// SourceCopy::GetFile - Get the file information from the section	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SourceCopy::GetFile(string &File,unsigned long &Size)
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
   Size = atoi(sSize.c_str());
   File = Base + File;
   return true;
}
									/*}}}*/
// SourceCopy::RewriteEntry - Rewrite the entry with a new filename	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SourceCopy::RewriteEntry(FILE *Target,string File)
{
   string Dir(File,0,File.rfind('/'));
   TFRewriteData Changes[] = {{"Directory",Dir.c_str()},
                              {}};
   
   if (TFRewrite(Target,*Section,TFRewriteSourceOrder,Changes) == false)
      return false;
   fputc('\n',Target);
   return true;
}
									/*}}}*/
// SigVerify::Verify - Verify a files md5sum against its metaindex     	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool SigVerify::Verify(string prefix, string file, indexRecords *MetaIndex)
{
   const indexRecords::checkSum *Record = MetaIndex->Lookup(file);

   // we skip non-existing files in the verifcation to support a cdrom
   // with no Packages file (just a Package.gz), see LP: #255545
   // (non-existing files are not considered a error)
   if(!FileExists(prefix+file))
   {
      _error->Warning(_("Skipping nonexistent file %s"), string(prefix+file).c_str());
      return true;
   }

   if (!Record) 
   {
      _error->Warning(_("Can't find authentication record for: %s"), file.c_str());
      return false;
   }

   if (!Record->Hash.VerifyFile(prefix+file))
   {
      _error->Warning(_("Hash mismatch for: %s"),file.c_str());
      return false;
   }

   if(_config->FindB("Debug::aptcdrom",false)) 
   {
      cout << "File: " << prefix+file << endl;
      cout << "Expected Hash " << Record->Hash.toStr() << endl;
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
      if (_error->PendingError() == true)
	 return false;
      if (CopyFile(Rel,Target) == false)
	 return false;
   
      return true;
}
									/*}}}*/
bool SigVerify::CopyAndVerify(string CDROM,string Name,vector<string> &SigList,	/*{{{*/
			      vector<string> PkgList,vector<string> SrcList)
{
   if (SigList.size() == 0)
      return true;

   bool Debug = _config->FindB("Debug::aptcdrom",false);

   // Read all Release files
   for (vector<string>::iterator I = SigList.begin(); I != SigList.end(); I++)
   { 
      if(Debug)
	 cout << "Signature verify for: " << *I << endl;

      indexRecords *MetaIndex = new indexRecords;
      string prefix = *I; 

      string const releasegpg = *I+"Release.gpg";
      string const release = *I+"Release";

      // a Release.gpg without a Release should never happen
      if(FileExists(release) == false)
      {
	 delete MetaIndex;
	 continue;
      }

      pid_t pid = ExecFork();
      if(pid < 0) {
	 _error->Error("Fork failed");
	 return false;
      }
      if(pid == 0)
	 RunGPGV(release, releasegpg);

      if(!ExecWait(pid, "gpgv")) {
	 _error->Warning("Signature verification failed for: %s",
			 releasegpg.c_str());
	 // something went wrong, don't copy the Release.gpg
	 // FIXME: delete any existing gpg file?
	 continue;
      }

      // Open the Release file and add it to the MetaIndex
      if(!MetaIndex->Load(release))
      {
	 _error->Error("%s",MetaIndex->ErrorText.c_str());
	 return false;
      }
      
      // go over the Indexfiles and see if they verify
      // if so, remove them from our copy of the lists
      vector<string> keys = MetaIndex->MetaKeys();
      for (vector<string>::iterator I = keys.begin(); I != keys.end(); I++)
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
      CopyMetaIndex(CDROM, Name, prefix, "Release");
      CopyMetaIndex(CDROM, Name, prefix, "Release.gpg");
   }   

   return true;
}
									/*}}}*/
// SigVerify::RunGPGV - returns the command needed for verify		/*{{{*/
// ---------------------------------------------------------------------
/* Generating the commandline for calling gpgv is somehow complicated as
   we need to add multiple keyrings and user supplied options. Also, as
   the cdrom code currently can not use the gpgv method we have two places
   these need to be done - so the place for this method is wrong but better
   than code duplication… */
bool SigVerify::RunGPGV(std::string const &File, std::string const &FileGPG,
			int const &statusfd, int fd[2])
{
   string const gpgvpath = _config->Find("Dir::Bin::gpg", "/usr/bin/gpgv");
   // FIXME: remove support for deprecated APT::GPGV setting
   string const trustedFile = _config->Find("APT::GPGV::TrustedKeyring", _config->FindFile("Dir::Etc::Trusted"));
   string const trustedPath = _config->FindDir("Dir::Etc::TrustedParts");

   bool const Debug = _config->FindB("Debug::Acquire::gpgv", false);

   if (Debug == true)
   {
      std::clog << "gpgv path: " << gpgvpath << std::endl;
      std::clog << "Keyring file: " << trustedFile << std::endl;
      std::clog << "Keyring path: " << trustedPath << std::endl;
   }

   std::vector<string> keyrings;
   if (DirectoryExists(trustedPath))
     keyrings = GetListOfFilesInDir(trustedPath, "gpg", false, true);
   if (FileExists(trustedFile) == true)
     keyrings.push_back(trustedFile);

   std::vector<const char *> Args;
   Args.reserve(30);

   if (keyrings.empty() == true)
      return false;

   Args.push_back(gpgvpath.c_str());
   Args.push_back("--ignore-time-conflict");

   if (statusfd != -1)
   {
      Args.push_back("--status-fd");
      char fd[10];
      snprintf(fd, sizeof(fd), "%i", statusfd);
      Args.push_back(fd);
   }

   for (vector<string>::const_iterator K = keyrings.begin();
	K != keyrings.end(); ++K)
   {
      Args.push_back("--keyring");
      Args.push_back(K->c_str());
   }

   Configuration::Item const *Opts;
   Opts = _config->Tree("Acquire::gpgv::Options");
   if (Opts != 0)
   {
      Opts = Opts->Child;
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 Args.push_back(Opts->Value.c_str());
      }
   }

   Args.push_back(FileGPG.c_str());
   if (FileGPG != File)
      Args.push_back(File.c_str());
   Args.push_back(NULL);

   if (Debug == true)
   {
      std::clog << "Preparing to exec: " << gpgvpath;
      for (std::vector<const char *>::const_iterator a = Args.begin(); *a != NULL; ++a)
	 std::clog << " " << *a;
      std::clog << std::endl;
   }

   if (statusfd != -1)
   {
      int const nullfd = open("/dev/null", O_RDONLY);
      close(fd[0]);
      // Redirect output to /dev/null; we read from the status fd
      dup2(nullfd, STDOUT_FILENO);
      dup2(nullfd, STDERR_FILENO);
      // Redirect the pipe to the status fd (3)
      dup2(fd[1], statusfd);

      putenv((char *)"LANG=");
      putenv((char *)"LC_ALL=");
      putenv((char *)"LC_MESSAGES=");
   }

   execvp(gpgvpath.c_str(), (char **) &Args[0]);
   return true;
}
									/*}}}*/
bool TranslationsCopy::CopyTranslations(string CDROM,string Name,	/*{{{*/
				vector<string> &List, pkgCdromStatus *log)
{
   OpProgress *Progress = NULL;
   if (List.size() == 0)
      return true;
   
   if(log) 
      Progress = log->GetOpProgress();
   
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   
   // Prepare the progress indicator
   unsigned long TotalSize = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      struct stat Buf;
      if (stat(string(*I).c_str(),&Buf) != 0 &&
	  stat(string(*I + ".gz").c_str(),&Buf) != 0)
	 return _error->Errno("stat","Stat failed for %s",
			      string(*I).c_str());
      TotalSize += Buf.st_size;
   }	

   unsigned long CurrentSize = 0;
   unsigned int NotFound = 0;
   unsigned int WrongSize = 0;
   unsigned int Packages = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {      
      string OrigPath = string(*I,CDROM.length());
      unsigned long FileSize = 0;
      
      // Open the package file
      FileFd Pkg;
      if (FileExists(*I) == true)
      {
	 Pkg.Open(*I,FileFd::ReadOnly);
	 FileSize = Pkg.Size();
      }      
      else
      {
	 FileFd From(*I + ".gz",FileFd::ReadOnly);
	 if (_error->PendingError() == true)
	    return false;
	 FileSize = From.Size();
	 
	 // Get a temp file
	 FILE *tmp = tmpfile();
	 if (tmp == 0)
	    return _error->Errno("tmpfile","Unable to create a tmp file");
	 Pkg.Fd(dup(fileno(tmp)));
	 fclose(tmp);
	 
	 // Fork gzip
	 pid_t Process = fork();
	 if (Process < 0)
	    return _error->Errno("fork","Couldn't fork gzip");
	 
	 // The child
	 if (Process == 0)
	 {	    
	    dup2(From.Fd(),STDIN_FILENO);
	    dup2(Pkg.Fd(),STDOUT_FILENO);
	    SetCloseExec(STDIN_FILENO,false);
	    SetCloseExec(STDOUT_FILENO,false);
	    
	    const char *Args[3];
	    string Tmp =  _config->Find("Dir::bin::gzip","gzip");
	    Args[0] = Tmp.c_str();
	    Args[1] = "-d";
	    Args[2] = 0;
	    execvp(Args[0],(char **)Args);
	    exit(100);
	 }
	 
	 // Wait for gzip to finish
	 if (ExecWait(Process,_config->Find("Dir::bin::gzip","gzip").c_str(),false) == false)
	    return _error->Error("gzip failed, perhaps the disk is full.");
	 
	 Pkg.Seek(0);
      }
      pkgTagFile Parser(&Pkg);
      if (_error->PendingError() == true)
	 return false;
      
      // Open the output file
      char S[400];
      snprintf(S,sizeof(S),"cdrom:[%s]/%s",Name.c_str(),
	       (*I).c_str() + CDROM.length());
      string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
      TargetF += URItoFileName(S);
      if (_config->FindB("APT::CDROM::NoAct",false) == true)
	 TargetF = "/dev/null";
      FileFd Target(TargetF,FileFd::WriteAtomic);
      FILE *TargetFl = fdopen(dup(Target.Fd()),"w");
      if (_error->PendingError() == true)
	 return false;
      if (TargetFl == 0)
	 return _error->Errno("fdopen","Failed to reopen fd");
      
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
      unsigned long Chop = 0;
      while (Parser.Step(Section) == true)
      {
	 if(Progress)
	    Progress->Progress(Parser.Offset());

	 const char *Start;
	 const char *Stop;
	 Section.GetSection(Start,Stop);
	 fwrite(Start,Stop-Start, 1, TargetFl);
	 fputc('\n',TargetFl);

	 Packages++;
	 Hits++;
      }
      fclose(TargetFl);

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

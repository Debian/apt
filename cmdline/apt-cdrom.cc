// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cdrom.cc,v 1.14 1998/12/22 08:41:20 jgg Exp $
/* ######################################################################
   
   APT CDROM - Tool for handling APT's CDROM database.
   
   Currently the only option is 'add' which will take the current CD
   in the drive and add it into the database.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/cdromutl.h>
#include <strutl.h>
#include <config.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
									/*}}}*/

// FindPackages - Find the package files on the CDROM			/*{{{*/
// ---------------------------------------------------------------------
/* We look over the cdrom for package files. This is a recursive
   search that short circuits when it his a package file in the dir.
   This speeds it up greatly as the majority of the size is in the
   binary-* sub dirs. */
bool FindPackages(string CD,vector<string> &List, unsigned int Depth = 0)
{
   static ino_t Inodes[9];
   if (Depth >= 7)
      return true;

   if (CD[CD.length()-1] != '/')
      CD += '/';   
   
   if (chdir(CD.c_str()) != 0)
      return _error->Errno("chdir","Unable to change to %s",CD.c_str());

   /* Aha! We found some package files. We assume that everything under 
      this dir is controlled by those package files so we don't look down
      anymore */
   struct stat Buf;
   if (stat("Packages",&Buf) == 0 ||
       stat("Packages.gz",&Buf) == 0)
   {
      List.push_back(CD);
      
      // Continue down if thorough is given
      if (_config->FindB("APT::CDROM::Thorough",false) == false)
	 return true;
   }

   DIR *D = opendir(".");
   if (D == 0)
      return _error->Errno("opendir","Unable to read %s",CD.c_str());
   
   // Run over the directory
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0 ||
	  strcmp(Dir->d_name,"source") == 0 ||
	  strcmp(Dir->d_name,"experimental") == 0 ||
	  strcmp(Dir->d_name,"binary-all") == 0)
	 continue;

      // See if the name is a sub directory
      struct stat Buf;
      if (stat(Dir->d_name,&Buf) != 0)
	 continue;      
      
      if (S_ISDIR(Buf.st_mode) == 0)
	 continue;
      
      unsigned int I;
      for (I = 0; I != Depth; I++)
	 if (Inodes[I] == Buf.st_ino)
	    break;
      if (I != Depth)
      {
	 cout << "Inode throw away " <<  Dir->d_name << endl;
	 continue;
      }
      
      // Store the inodes weve seen
      Inodes[Depth] = Buf.st_ino;

      // Descend
      if (FindPackages(CD + Dir->d_name,List,Depth+1) == false)
	 break;

      if (chdir(CD.c_str()) != 0)
	 return _error->Errno("chdir","Unable to change to ",CD.c_str());
   };

   closedir(D);
   
   return !_error->PendingError();
}
									/*}}}*/
// DropBinaryArch - Dump dirs with a string like /binary-<foo>/		/*{{{*/
// ---------------------------------------------------------------------
/* Here we drop everything that is not this machines arch */
bool DropBinaryArch(vector<string> &List)
{
   char S[300];
   sprintf(S,"/binary-%s/",_config->Find("Apt::Architecture").c_str());
   
   for (unsigned int I = 0; I < List.size(); I++)
   {
      const char *Str = List[I].c_str();
      
      const char *Res;
      if ((Res = strstr(Str,"/binary-")) == 0)
	 continue;

      // Weird, remove it.
      if (strlen(Res) < strlen(S))
      {
	 List.erase(List.begin() + I);
	 I--;
	 continue;
      }
	  
      // See if it is our arch
      if (stringcmp(Res,Res + strlen(S),S) == 0)
	 continue;
      
      // Erase it
      List.erase(List.begin() + I);
      I--;
   }
   
   return true;
}
									/*}}}*/
// Score - We compute a 'score' for a path				/*{{{*/
// ---------------------------------------------------------------------
/* Paths are scored based on how close they come to what I consider
   normal. That is ones that have 'dist' 'stable' 'frozen' will score
   higher than ones without. */
int Score(string Path)
{
   int Res = 0;
   if (Path.find("stable/") != string::npos)
      Res += 2;
   if (Path.find("/binary-") != string::npos)
      Res += 2;
   if (Path.find("frozen/") != string::npos)
      Res += 2;
   if (Path.find("/dists/") != string::npos)
      Res += 4;
   if (Path.find("/main/") != string::npos)
      Res += 2;
   if (Path.find("/contrib/") != string::npos)
      Res += 2;
   if (Path.find("/non-free/") != string::npos)
      Res += 2;
   if (Path.find("/non-US/") != string::npos)
      Res += 2;
   return Res;
}
									/*}}}*/
// DropRepeats - Drop repeated files resulting from symlinks		/*{{{*/
// ---------------------------------------------------------------------
/* Here we go and stat every file that we found and strip dup inodes. */
bool DropRepeats(vector<string> &List)
{
   // Get a list of all the inodes
   ino_t *Inodes = new ino_t[List.size()];
   for (unsigned int I = 0; I != List.size(); I++)
   {
      struct stat Buf;
      if (stat((List[I] + "Packages").c_str(),&Buf) != 0)
	 _error->Errno("stat","Failed to stat %s",List[I].c_str());
      Inodes[I] = Buf.st_ino;
   }
   
   // Look for dups
   for (unsigned int I = 0; I != List.size(); I++)
   {
      for (unsigned int J = I+1; J < List.size(); J++)
      {
	 // No match
	 if (Inodes[J] != Inodes[I])
	    continue;
	 
	 // We score the two paths.. and erase one
	 int ScoreA = Score(List[I]);
	 int ScoreB = Score(List[J]);
	 if (ScoreA < ScoreB)
	 {
	    List[I] = string();
	    break;
	 }
	 
	 List[J] = string();
      }
   }  
 
   // Wipe erased entries
   for (unsigned int I = 0; I < List.size();)
   {
      if (List[I].empty() == false)
	 I++;
      else
	 List.erase(List.begin()+I);
   }
   
   return true;
}
									/*}}}*/
// ConvertToSourceList - Convert a Path to a sourcelist entry		/*{{{*/
// ---------------------------------------------------------------------
/* We look for things in dists/ notation and convert them to 
   <dist> <component> form otherwise it is left alone. This also strips
   the CD path. */
void ConvertToSourceList(string CD,string &Path)
{
   char S[300];
   sprintf(S,"binary-%s",_config->Find("Apt::Architecture").c_str());
   
   // Strip the cdrom base path
   Path = string(Path,CD.length());
   if (Path.empty() == true)
      Path = "/";
   
   // Too short to be a dists/ type
   if (Path.length() < strlen("dists/"))
      return;
   
   // Not a dists type.
   if (stringcmp(Path.begin(),Path.begin()+strlen("dists/"),"dists/") != 0)
      return;

   // Isolate the dist
   string::size_type Slash = strlen("dists/");
   string::size_type Slash2 = Path.find('/',Slash + 1);
   if (Slash2 == string::npos || Slash2 + 2 >= Path.length())
      return;
   string Dist = string(Path,Slash,Slash2 - Slash);
   
   // Isolate the component
   Slash = Path.find('/',Slash2+1);
   if (Slash == string::npos || Slash + 2 >= Path.length())
      return;
   string Comp = string(Path,Slash2+1,Slash - Slash2-1);
   
   // Verify the trailing binar - bit
   Slash2 = Path.find('/',Slash + 1);
   if (Slash == string::npos)
      return;
   string Binary = string(Path,Slash+1,Slash2 - Slash-1);
   
   if (Binary != S)
      return;
   
   Path = Dist + ' ' + Comp;
}
									/*}}}*/
// GrabFirst - Return the first Depth path components			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GrabFirst(string Path,string &To,unsigned int Depth)
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
// ChopDirs - Chop off the leading directory components			/*{{{*/
// ---------------------------------------------------------------------
/* */
string ChopDirs(string Path,unsigned int Depth)
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
// ReconstructPrefix - Fix strange prefixing				/*{{{*/
// ---------------------------------------------------------------------
/* This prepends dir components from the path to the package files to
   the path to the deb until it is found */
bool ReconstructPrefix(string &Prefix,string OrigPath,string CD,
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
// ReconstructChop - Fixes bad source paths				/*{{{*/
// ---------------------------------------------------------------------
/* This removes path components from the filename and prepends the location
   of the package files until a file is found */
bool ReconstructChop(unsigned long &Chop,string Dir,string File)
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

// CopyPackages - Copy the package files from the CD			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CopyPackages(string CDROM,string Name,vector<string> &List)
{
   OpTextProgress Progress;
   
   bool NoStat = _config->FindB("APT::CDROM::Fast",false);
   bool Debug = _config->FindB("Debug::aptcdrom",false);
   
   // Prepare the progress indicator
   unsigned long TotalSize = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      struct stat Buf;
      if (stat(string(*I + "Packages").c_str(),&Buf) != 0)
	 return _error->Errno("stat","Stat failed for %s",
			      string(*I + "Packages").c_str());
      TotalSize += Buf.st_size;
   }	

   unsigned long CurrentSize = 0;
   unsigned int NotFound = 0;
   unsigned int WrongSize = 0;
   unsigned int Packages = 0;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {      
      string OrigPath = string(*I,CDROM.length());
      
      // Open the package file
      FileFd Pkg(*I + "Packages",FileFd::ReadOnly);
      pkgTagFile Parser(Pkg);
      if (_error->PendingError() == true)
	 return false;
      
      // Open the output file
      char S[400];
      sprintf(S,"cdrom:%s/%sPackages",Name.c_str(),(*I).c_str() + CDROM.length());
      string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
      TargetF += URItoFileName(S);
      if (_config->FindB("APT::CDROM::NoAct",false) == true)
	 TargetF = "/dev/null";
      FileFd Target(TargetF,FileFd::WriteEmpty);      
      if (_error->PendingError() == true)
	 return false;
      
      // Setup the progress meter
      Progress.OverallProgress(CurrentSize,TotalSize,Pkg.Size(),
			       "Reading Package Lists");

      // Parse
      Progress.SubProgress(Pkg.Size());
      pkgTagSection Section;
      string Prefix;
      unsigned long Hits = 0;
      unsigned long Chop = 0;
      while (Parser.Step(Section) == true)
      {
	 Progress.Progress(Parser.Offset());
	 
	 string File = Section.FindS("Filename");
	 unsigned long Size = Section.FindI("Size");
	 if (File.empty() || Size == 0)
	    return _error->Error("Cannot find filename or size tag");
	 
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
		  NotFound++;
		  continue;
	       }
	       if (Chop != 0)
		  File = OrigPath + ChopDirs(File,Chop);
	    }
	    
	    // Get the size
	    struct stat Buf;
	    if (stat(string(CDROM + Prefix + File).c_str(),&Buf) != 0)
	    {
	       NotFound++;
	       continue;
	    }	    
	    			    	    
	    // Size match
	    if ((unsigned)Buf.st_size != Size)
	    {
	       WrongSize++;
	       continue;
	    }
	 }
	 
	 Packages++;
	 Hits++;
	 
	 // Copy it to the target package file
	 const char *Start;
	 const char *Stop;
	 if (Chop != 0)
	 {
	    // Mangle the output filename
	    const char *Filename;
	    Section.Find("Filename",Filename,Stop);
	    
	    /* We need to rewrite the filename field so we emit
	       all fields except the filename file and rewrite that one */
	    for (unsigned int I = 0; I != Section.Count(); I++)
	    {
	       Section.Get(Start,Stop,I);
	       if (Start <= Filename && Stop > Filename)
	       {
		  char S[500];
		  sprintf(S,"Filename: %s\n",File.c_str());
		  if (I + 1 == Section.Count())
		     strcat(S,"\n");
		  if (Target.Write(S,strlen(S)) == false)
		     return false;
	       }
	       else
		  if (Target.Write(Start,Stop-Start) == false)
		     return false;		  
	    }
	    if (Target.Write("\n",1) == false)
	       return false;
	 }
	 else
	 {
	    Section.GetSection(Start,Stop);
	    if (Target.Write(Start,Stop-Start) == false)
	       return false;
	 }	 
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

	 // Copy the release file
	 sprintf(S,"cdrom:%s/%sRelease",Name.c_str(),(*I).c_str() + CDROM.length());
	 string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
	 TargetF += URItoFileName(S);
	 if (FileExists(*I + "Release") == true)
	 {
	    FileFd Target(TargetF,FileFd::WriteEmpty);
	    FileFd Rel(*I + "Release",FileFd::ReadOnly);
	    if (_error->PendingError() == true)
	       return false;
	    
	    if (CopyFile(Rel,Target) == false)
	       return false;
	 }
	 else
	 {
	    // Empty release file
	    FileFd Target(TargetF,FileFd::WriteEmpty);	    
	 }	 

	 // Rename the release file
	 FinalF = _config->FindDir("Dir::State::lists");
	 FinalF += URItoFileName(S);
	 if (rename(TargetF.c_str(),FinalF.c_str()) != 0)
	    return _error->Errno("rename","Failed to rename");
      }
      
      /* Mangle the source to be in the proper notation with
       	 prefix dist [component] */ 
      *I = string(*I,Prefix.length());
      ConvertToSourceList(CDROM,*I);
      *I = Prefix + ' ' + *I;
      
      CurrentSize += Pkg.Size();
   }   
   Progress.Done();
   
   // Some stats
   cout << "Wrote " << Packages << " package records" ;
   if (NotFound != 0)
      cout << " with " << NotFound << " missing files";
   if (NotFound != 0 && WrongSize != 0)
      cout << " and"; 
   if (WrongSize != 0)
      cout << " with " << WrongSize << " mismatched files";
   cout << '.' << endl;
   
   if (Packages == 0)
      return _error->Error("No valid package records were found.");
   
   if (NotFound + WrongSize > 10)
      cout << "Alot of package entires were discarded, perhaps this CD is funny?" << endl;

   return true;
}
									/*}}}*/

// ReduceSourceList - Takes the path list and reduces it		/*{{{*/
// ---------------------------------------------------------------------
/* This takes the list of source list expressed entires and collects
   similar ones to form a single entry for each dist */
bool ReduceSourcelist(string CD,vector<string> &List)
{
   sort(List.begin(),List.end());
   
   // Collect similar entries
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      // Find a space..
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
	 continue;
      string::size_type SSpace = (*I).find(' ',Space + 1);
      if (SSpace == string::npos)
	 continue;
      
      string Word1 = string(*I,Space,SSpace-Space);
      for (vector<string>::iterator J = List.begin(); J != I; J++)
      {
	 // Find a space..
	 string::size_type Space2 = (*J).find(' ');
	 if (Space2 == string::npos)
	    continue;
	 string::size_type SSpace2 = (*J).find(' ',Space2 + 1);
	 if (SSpace2 == string::npos)
	    continue;
	 
	 if (string(*J,Space2,SSpace2-Space2) != Word1)
	    continue;
	 
	 *J += string(*I,SSpace);
	 *I = string();
      }
   }   

   // Wipe erased entries
   for (unsigned int I = 0; I < List.size();)
   {
      if (List[I].empty() == false)
	 I++;
      else
	 List.erase(List.begin()+I);
   }
}
									/*}}}*/
// WriteDatabase - Write the CDROM Database file			/*{{{*/
// ---------------------------------------------------------------------
/* We rewrite the configuration class associated with the cdrom database. */
bool WriteDatabase(Configuration &Cnf)
{
   string DFile = _config->FindFile("Dir::State::cdroms");
   string NewFile = DFile + ".new";
   
   unlink(NewFile.c_str());
   ofstream Out(NewFile.c_str());
   if (!Out)
      return _error->Errno("ofstream::ofstream",
			   "Failed to open %s.new",DFile.c_str());
   
   /* Write out all of the configuration directives by walking the
      configuration tree */
   const Configuration::Item *Top = Cnf.Tree(0);
   for (; Top != 0;)
   {
      // Print the config entry
      if (Top->Value.empty() == false)
	 Out <<  Top->FullTag() + " \"" << Top->Value << "\";" << endl;
      
      if (Top->Child != 0)
      {
	 Top = Top->Child;
	 continue;
      }
      
      while (Top != 0 && Top->Next == 0)
	 Top = Top->Parent;
      if (Top != 0)
	 Top = Top->Next;
   }   

   Out.close();
   
   rename(DFile.c_str(),string(DFile + '~').c_str());
   if (rename(NewFile.c_str(),DFile.c_str()) != 0)
      return _error->Errno("rename","Failed to rename %s.new to %s",
			   DFile.c_str(),DFile.c_str());

   return true;
}
									/*}}}*/
// WriteSourceList - Write an updated sourcelist			/*{{{*/
// ---------------------------------------------------------------------
/* This reads the old source list and copies it into the new one. It 
   appends the new CDROM entires just after the first block of comments.
   This places them first in the file. It also removes any old entries
   that were the same. */
bool WriteSourceList(string Name,vector<string> &List)
{
   string File = _config->FindFile("Dir::Etc::sourcelist");

   // Open the stream for reading
   ifstream F(File.c_str(),ios::in | ios::nocreate);
   if (!F != 0)
      return _error->Errno("ifstream::ifstream","Opening %s",File.c_str());

   string NewFile = File + ".new";
   unlink(NewFile.c_str());
   ofstream Out(NewFile.c_str());
   if (!Out)
      return _error->Errno("ofstream::ofstream",
			   "Failed to open %s.new",File.c_str());

   // Create a short uri without the path
   string ShortURI = "cdrom:" + Name + "/";   
   
   char Buffer[300];
   int CurLine = 0;
   bool First = true;
   while (F.eof() == false)
   {      
      F.getline(Buffer,sizeof(Buffer));
      CurLine++;
      _strtabexpand(Buffer,sizeof(Buffer));
      _strstrip(Buffer);
            
      // Comment or blank
      if (Buffer[0] == '#' || Buffer[0] == 0)
      {
	 Out << Buffer << endl;
	 continue;
      }

      if (First == true)
      {
	 for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
	 {
	    string::size_type Space = (*I).find(' ');
	    if (Space == string::npos)
	       return _error->Error("Internal error");
	    
	    Out << "deb \"cdrom:" << Name << "/" << string(*I,0,Space) << 
	       "\" " << string(*I,Space+1) << endl;
	 }
      }
      First = false;
      
      // Grok it
      string Type;
      string URI;
      char *C = Buffer;
      if (ParseQuoteWord(C,Type) == false ||
	  ParseQuoteWord(C,URI) == false)
      {
	 Out << Buffer << endl;
	 continue;
      }

      // Emit lines like this one
      if (Type != "deb" || string(URI,0,ShortURI.length()) != ShortURI)
      {
	 Out << Buffer << endl;
	 continue;
      }      
   }
   
   // Just in case the file was empty
   if (First == true)
   {
      for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
      {
	 string::size_type Space = (*I).find(' ');
	 if (Space == string::npos)
	    return _error->Error("Internal error");
	 
	 Out << "deb \"cdrom:" << Name << "/" << string(*I,0,Space) << 
	    "\" " << string(*I,Space+1) << endl;
      }
   }
   
   Out.close();

   rename(File.c_str(),string(File + '~').c_str());
   if (rename(NewFile.c_str(),File.c_str()) != 0)
      return _error->Errno("rename","Failed to rename %s.new to %s",
			   File.c_str(),File.c_str());
   
   return true;
}
									/*}}}*/

// Prompt - Simple prompt						/*{{{*/
// ---------------------------------------------------------------------
/* */
void Prompt(const char *Text)
{
   char C;
   cout << Text << ' ' << flush;
   read(STDIN_FILENO,&C,1);
   if (C != '\n')
      cout << endl;
}
									/*}}}*/
// PromptLine - Prompt for an input line				/*{{{*/
// ---------------------------------------------------------------------
/* */
string PromptLine(const char *Text)
{
   cout << Text << ':' << endl;
   
   string Res;
   getline(cin,Res);
   return Res;
}
									/*}}}*/

// DoAdd - Add a new CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* This does the main add bit.. We show some status and things. The
   sequence is to mount/umount the CD, Ident it then scan it for package 
   files and reduce that list. Then we copy over the package files and
   verify them. Then rewrite the database files */
bool DoAdd(CommandLine &)
{
   // Startup
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;
   
   cout << "Using CD-ROM mount point " << CDROM << endl;
      
   // Read the database
   Configuration Database;
   string DFile = _config->FindFile("Dir::State::cdroms");
   if (FileExists(DFile) == true)
   {
      if (ReadConfigFile(Database,DFile) == false)
	 return _error->Error("Unable to read the cdrom database %s",
			      DFile.c_str());
   }
   
   // Unmount the CD and get the user to put in the one they want
   if (_config->FindB("APT::CDROM::NoMount",false) == false)
   {
      cout << "Unmounting CD-ROM" << endl;
      UnmountCdrom(CDROM);

      // Mount the new CDROM
      Prompt("Please insert a Disc in the drive and press any key");
      cout << "Mounting CD-ROM" << endl;
      if (MountCdrom(CDROM) == false)
      {
	 cout << "Failed to mount the cdrom." << endl;
	 return false;
      }
   }
   
   // Hash the CD to get an ID
   cout << "Identifying.. " << flush;
   string ID;
   if (IdentCdrom(CDROM,ID) == false)
   {
      cout << endl;
      return false;
   }
   
   cout << '[' << ID << ']' << endl;

   cout << "Scanning Disc for index files..  " << flush;
   // Get the CD structure
   vector<string> List;
   string StartDir = SafeGetCWD();
   if (FindPackages(CDROM,List) == false)
   {
      cout << endl;
      return false;
   }
   
   chdir(StartDir.c_str());

   if (_config->FindB("Debug::aptcdrom",false) == true)
   {
      cout << "I found:" << endl;
      for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
      {
	 cout << *I << endl;
      }      
   }   
   
   // Fix up the list
   DropBinaryArch(List);
   DropRepeats(List);
   cout << "Found " << List.size() << " package index files." << endl;

   if (List.size() == 0)
      return _error->Error("Unable to locate any package files, perhaps this is not a Debian Disc");
   
   // Check if the CD is in the database
   string Name;
   if (Database.Exists("CD::" + ID) == false ||
       _config->FindB("APT::CDROM::Rename",false) == true)
   {
      // Try to use the CDs label if at all possible
      if (FileExists(CDROM + "/.disk/info") == true)
      {
	 ifstream F(string(CDROM+ "/.disk/info").c_str());
	 if (!F == 0)
	    getline(F,Name);

	 if (Name.empty() == false)
	 {
	    cout << "Found label '" << Name << "'" << endl;
	    Database.Set("CD::" + ID + "::Label",Name);
	 }	 
      }
      
      if (_config->FindB("APT::CDROM::Rename",false) == true ||
	  Name.empty() == true)
      {
	 cout << "Please provide a name for this Disc, such as 'Debian 2.1r1 Disk 1'";
	 while (1)
	 {
	    Name = PromptLine("");
	    if (Name.empty() == false &&
		Name.find('"') == string::npos &&
		Name.find(':') == string::npos &&
		Name.find('/') == string::npos)
	       break;
	    cout << "That is not a valid name, try again " << endl;
	 }	 
      }      
   }
   else
      Name = Database.Find("CD::" + ID);
   
   string::iterator J = Name.begin();
   for (; J != Name.end(); J++)
      if (*J == '/' || *J == '"' || *J == ':')
	 *J = '_';
   
   Database.Set("CD::" + ID,Name);
   cout << "This Disc is called '" << Name << "'" << endl;
   
   // Copy the package files to the state directory
   if (CopyPackages(CDROM,Name,List) == false)
      return false;
   
   ReduceSourcelist(CDROM,List);

   // Write the database and sourcelist
   if (_config->FindB("APT::cdrom::NoAct",false) == false)
   {
      if (WriteDatabase(Database) == false)
	 return false;
      
      cout << "Writing new source list" << endl;
      if (WriteSourceList(Name,List) == false)
	 return false;
   }

   // Print the sourcelist entries
   cout << "Source List entries for this Disc are:" << endl;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
	 return _error->Error("Internal error");

      cout << "deb \"cdrom:" << Name << "/" << string(*I,0,Space) << 
	 "\" " << string(*I,Space+1) << endl;
   }
      
   return true;
}
									/*}}}*/

// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   cout << PACKAGE << ' ' << VERSION << " for " << ARCHITECTURE <<
       " compiled on " << __DATE__ << "  " << __TIME__ << endl;
   
   cout << "Usage: apt-cdrom [options] command" << endl;
   cout << endl;
   cout << "apt-cdrom is a tool to add CDROM's to APT's source list. The " << endl;
   cout << "CDROM mount point and device information is taken from apt.conf" << endl;
   cout << "and /etc/fstab." << endl;
   cout << endl;
   cout << "Commands:" << endl;
   cout << "   add - Add a CDROM" << endl;
   cout << endl;
   cout << "Options:" << endl;
   cout << "  -h   This help text" << endl;
   cout << "  -d   CD-ROM mount point" << endl;
   cout << "  -r   Rename a recognized CD-ROM" << endl;
   cout << "  -m   No mounting" << endl;
   cout << "  -f   Fast mode, don't check package files" << endl;
   cout << "  -a   Thorough scan mode" << endl;
   cout << "  -c=? Read this configuration file" << endl;
   cout << "  -o=? Set an arbitary configuration option, ie -o dir::cache=/tmp" << endl;
   cout << "See fstab(5)" << endl;
   return 100;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'d',"cdrom","Acquire::cdrom::mount",CommandLine::HasArg},
      {'r',"rename","APT::CDROM::Rename",0},
      {'m',"no-mount","APT::CDROM::NoMount",0},
      {'f',"fast","APT::CDROM::Fast",0},
      {'n',"just-print","APT::CDROM::NoAct",0},
      {'n',"recon","APT::CDROM::NoAct",0},      
      {'n',"no-act","APT::CDROM::NoAct",0},
      {'a',"thorough","APT::CDROM::Thorough",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {
      {"add",&DoAdd},
      {0,0}};
	 
   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitialize(*_config) == false ||
       CmdL.Parse(argc,argv) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();
   
   // Match the operation
   CmdL.DispatchArg(Cmds);

   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
   
   return 0;
}

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cdrom.cc,v 1.1 1998/11/27 01:52:56 jgg Exp $
/* ######################################################################
   
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/tagfile.h>
#include <strutl.h>
#include <config.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
									/*}}}*/

// UnmountCdrom - Unmount a cdrom					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool UnmountCdrom(string Path)
{
   int Child = fork();
   if (Child < -1)
      return _error->Errno("fork","Failed to fork");

   // The child
   if (Child == 0)
   {
      // Make all the fds /dev/null
      for (int I = 0; I != 10;)
	 close(I);
      for (int I = 0; I != 3;)
	 dup2(open("/dev/null",O_RDWR),I);
      
      const char *Args[10];
      Args[0] = "umount";
      Args[1] = Path.c_str();
      Args[2] = 0;
      execvp(Args[0],(char **)Args);      
      exit(100);
   }

   // Wait for mount
   int Status = 0;
   while (waitpid(Child,&Status,0) != Child)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }
   
   // Check for an error code.
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      return false;
   return true;
}
									/*}}}*/
// MountCdrom - Mount a cdrom						/*{{{*/
// ---------------------------------------------------------------------
/* We fork mount.. */
bool MountCdrom(string Path)
{
   int Child = fork();
   if (Child < -1)
      return _error->Errno("fork","Failed to fork");

   // The child
   if (Child == 0)
   {
      // Make all the fds /dev/null
      for (int I = 0; I != 10;)
	 close(I);      
      for (int I = 0; I != 3;)
	 dup2(open("/dev/null",O_RDWR),I);
      
      const char *Args[10];
      Args[0] = "mount";
      Args[1] = Path.c_str();
      Args[2] = 0;
      execvp(Args[0],(char **)Args);      
      exit(100);
   }

   // Wait for mount
   int Status = 0;
   while (waitpid(Child,&Status,0) != Child)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }
   
   // Check for an error code.
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
      return false;
   return true;
}
									/*}}}*/
// IdentCdrom - Generate a unique string for this CD			/*{{{*/
// ---------------------------------------------------------------------
/* We convert everything we hash into a string, this prevents byte size/order
   from effecting the outcome. */
bool IdentCdrom(string CD,string &Res)
{
   MD5Summation Hash;

   string StartDir = SafeGetCWD();
   if (chdir(CD.c_str()) != 0)
      return _error->Errno("chdir","Unable to change to %s",CD.c_str());
   
   DIR *D = opendir(".");
   if (D == 0)
      return _error->Errno("opendir","Unable to read %s",CD.c_str());
      
   // Run over the directory
   char S[300];
   for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
   {
      // Skip some files..
      if (strcmp(Dir->d_name,".") == 0 ||
	  strcmp(Dir->d_name,"..") == 0)
	 continue;
   
      sprintf(S,"%lu",Dir->d_ino);
      Hash.Add(S);
      Hash.Add(Dir->d_name);
   };
   
   chdir(StartDir.c_str());
   closedir(D);
   
   // Some stats from the fsys
   struct statfs Buf;
   if (statfs(CD.c_str(),&Buf) != 0)
      return _error->Errno("statfs","Failed to stat the cdrom");
   
   sprintf(S,"%u %u",Buf.f_blocks,Buf.f_bfree);
   Hash.Add(S);
   
   Res = Hash.Result().Value();
   return true;   
}
									/*}}}*/

// FindPackage - Find the package files on the CDROM			/*{{{*/
// ---------------------------------------------------------------------
/* We look over the cdrom for package files. This is a recursive
   search that short circuits when it his a package file in the dir.
   This speeds it up greatly as the majority of the size is in the
   binary-* sub dirs. */
bool FindPackages(string CD,vector<string> &List, int Depth = 0)
{
   if (Depth >= 5)
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
      {
	 _error->Errno("Stat","Stat failed for %s",Dir->d_name);
	 break;
      }
      
      if (S_ISDIR(Buf.st_mode) == 0)
	 continue;
      
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
// CopyPackages - Copy the package files from the CD			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool CopyPackages(string CDROM,string Name,vector<string> &List)
{
   OpTextProgress Progress;
   
   bool NoStat = _config->FindB("APT::CDROM::Fast",false);
   
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
      // Open the package file 
      FileFd Pkg(*I + "Packages",FileFd::ReadOnly);
      pkgTagFile Parser(Pkg);
      if (_error->PendingError() == true)
	 return false;
      
      // Open the output file
      char S[400];
      sprintf(S,"cdrom:%s/%sPackages",Name.c_str(),(*I).c_str() + CDROM.length());
      string TargetF = _config->FindDir("Dir::State::lists") + "partial/";
      FileFd Target(TargetF + URItoFileName(S),FileFd::WriteEmpty);      
      if (_error->PendingError() == true)
	 return false;
      
      // Setup the progress meter
      Progress.OverallProgress(CurrentSize,TotalSize,Pkg.Size(),
			       "Reading Package Lists");

      // Parse
      Progress.SubProgress(Pkg.Size());
      pkgTagSection Section;
      while (Parser.Step(Section) == true)
      {
	 Progress.Progress(Parser.Offset());
	 
	 string File = Section.FindS("Filename");
	 unsigned long Size = Section.FindI("Size");
	 if (File.empty() || Size == 0)
	    return _error->Error("Cannot find filename or size tag");
	 
	 // See if the file exists
	 if (NoStat == false)
	 {
	    struct stat Buf;
	    File = CDROM + File; 
	    if (stat(File.c_str(),&Buf) != 0)
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
	 
	 // Copy it to the target package file
	 const char *Start;
	 const char *Stop;
	 Section.GetSection(Start,Stop);
	 if (Target.Write(Start,Stop-Start) == false)
	    return false;
      }
      
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
   if (NotFound + WrongSize > 10)
      cout << "Alot of package entires were discarded, perhaps this CD is funny?" << endl;
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
      if (stat(List[I].c_str(),&Buf) != 0)
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
// ConvertToSourceList - Takes the path list and converts it		/*{{{*/
// ---------------------------------------------------------------------
/* This looks at each element and decides if it can be expressed using
   dists/ form or if it requires an absolute specficiation. It also 
   strips the leading CDROM path from the paths. */
bool ConvertToSourcelist(string CD,vector<string> &List)
{
   char S[300];
   sprintf(S,"binary-%s",_config->Find("Apt::Architecture").c_str());

   sort(List.begin(),List.end());
   
   // Convert to source list notation
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {      
      // Strip the cdrom base path
      *I = string(*I,CD.length());
   
      // Too short to be a dists/ type
      if ((*I).length() < strlen("dists/"))
	 continue;
   
      // Not a dists type.
      if (stringcmp((*I).begin(),(*I).begin()+strlen("dists/"),"dists/") != 0)
	 continue;

      // Isolate the dist
      string::size_type Slash = strlen("dists/");
      string::size_type Slash2 = (*I).find('/',Slash + 1);
      if (Slash2 == string::npos || Slash2 + 2 >= (*I).length())
	 continue;
      string Dist = string(*I,Slash,Slash2 - Slash);

      // Isolate the component
      Slash = (*I).find('/',Slash2+1);
      if (Slash == string::npos || Slash + 2 >= (*I).length())
	 continue;
      string Comp = string(*I,Slash2+1,Slash - Slash2-1);
      
      // Verify the trailing binar - bit
      Slash2 = (*I).find('/',Slash + 1);
      if (Slash == string::npos)
	 continue;
      string Binary = string(*I,Slash+1,Slash2 - Slash-1);
      
      if (Binary != S)
	 continue;
      
      *I = Dist + ' ' + Comp;
   }
   
   // Collect similar entries
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      // Find a space..
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
	 continue;
      
      string Word1 = string(*I,0,Space);
      for (vector<string>::iterator J = List.begin(); J != I; J++)
      {
	 // Find a space..
	 string::size_type Space2 = (*J).find(' ');
	 if (Space2 == string::npos)
	    continue;
	 
	 if (string(*J,0,Space2) != Word1)
	    continue;
	 
	 *J += string(*I,Space);
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
/* */
bool DoAdd(CommandLine &)
{
   // Startup
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
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
      Prompt("Please insert a CD-ROM and press any key");
      cout << "Mounting CD-ROM" << endl;
      if (MountCdrom(CDROM) == false)
      {
	 cout << "Failed to mount the cdrom." << endl;
	 return false;
      }
   }
   
   // Hash the CD to get an ID
   cout << "Indentifying.. " << flush;
   string ID;
   if (IdentCdrom(CDROM,ID) == false)
      return false;
   cout << '[' << ID << ']' << endl;

   cout << "Scanning Disc for index files..  " << flush;
   // Get the CD structure
   vector<string> List;
   string StartDir = SafeGetCWD();
   if (FindPackages(CDROM,List) == false)
      return false;
   chdir(StartDir.c_str());
   
   // Fix up the list
   DropBinaryArch(List);
   DropRepeats(List);
   cout << "Found " << List.size() << " package index files." << endl;

   if (List.size() == 0)
      return _error->Error("Unable to locate any package files, perhaps this is not a debian CD-ROM");
   
   // Check if the CD is in the database
   string Name;
   if (Database.Exists("CD::" + ID) == false ||
       _config->FindB("APT::CDROM::Rename",false) == true)
   {
      cout << "Please provide a name for this CD-ROM, such as 'Debian 2.1r1 Disk 1'";
      Name = PromptLine("");
   }
   else
      Name = Database.Find("CD::" + ID);
   cout << "This Disc is called '" << Name << "'" << endl;
   
   // Copy the package files to the state directory
   if (CopyPackages(CDROM,Name,List) == false)
      return false;
   
   ConvertToSourcelist(CDROM,List);

   // Print the sourcelist entries
   cout << "Source List entires for this Disc are:" << endl;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
      cout << "deb \"cdrom:" << Name << "/\" " << *I << endl;
   
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cdrom.cc,v 1.45 2003/11/19 23:50:51 mdz Exp $
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
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/strutl.h>
#include <config.h>
#include <apti18n.h>
    
#include "indexcopy.h"

#include <locale.h>
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

using namespace std;

// FindPackages - Find the package files on the CDROM			/*{{{*/
// ---------------------------------------------------------------------
/* We look over the cdrom for package files. This is a recursive
   search that short circuits when it his a package file in the dir.
   This speeds it up greatly as the majority of the size is in the
   binary-* sub dirs. */
bool FindPackages(string CD,vector<string> &List,vector<string> &SList,
		  string &InfoDir,unsigned int Depth = 0)
{
   static ino_t Inodes[9];
   if (Depth >= 7)
      return true;

   if (CD[CD.length()-1] != '/')
      CD += '/';   

   if (chdir(CD.c_str()) != 0)
      return _error->Errno("chdir","Unable to change to %s",CD.c_str());

   // Look for a .disk subdirectory
   struct stat Buf;
   if (stat(".disk",&Buf) == 0)
   {
      if (InfoDir.empty() == true)
	 InfoDir = CD + ".disk/";
   }

   // Don't look into directories that have been marked to ingore.
   if (stat(".aptignr",&Buf) == 0)
      return true;
   
   /* Aha! We found some package files. We assume that everything under 
      this dir is controlled by those package files so we don't look down
      anymore */
   if (stat("Packages",&Buf) == 0 || stat("Packages.gz",&Buf) == 0)
   {
      List.push_back(CD);
      
      // Continue down if thorough is given
      if (_config->FindB("APT::CDROM::Thorough",false) == false)
	 return true;
   }
   if (stat("Sources.gz",&Buf) == 0 || stat("Sources",&Buf) == 0)
   {
      SList.push_back(CD);
      
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
	  //strcmp(Dir->d_name,"source") == 0 ||
	  strcmp(Dir->d_name,".disk") == 0 ||
	  strcmp(Dir->d_name,"experimental") == 0 ||
	  strcmp(Dir->d_name,"binary-all") == 0 ||
          strcmp(Dir->d_name,"debian-installer") == 0)
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
	 continue;
      
      // Store the inodes weve seen
      Inodes[Depth] = Buf.st_ino;

      // Descend
      if (FindPackages(CD + Dir->d_name,List,SList,InfoDir,Depth+1) == false)
	 break;

      if (chdir(CD.c_str()) != 0)
	 return _error->Errno("chdir","Unable to change to %s",CD.c_str());
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
   snprintf(S,sizeof(S),"/binary-%s/",
	    _config->Find("Apt::Architecture").c_str());
   
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
   normal. That is ones that have 'dist' 'stable' 'testing' will score
   higher than ones without. */
int Score(string Path)
{
   int Res = 0;
   if (Path.find("stable/") != string::npos)
      Res += 29;
   if (Path.find("/binary-") != string::npos)
      Res += 20;
   if (Path.find("testing/") != string::npos)
      Res += 28;
   if (Path.find("unstable/") != string::npos)
      Res += 27;
   if (Path.find("/dists/") != string::npos)
      Res += 40;
   if (Path.find("/main/") != string::npos)
      Res += 20;
   if (Path.find("/contrib/") != string::npos)
      Res += 20;
   if (Path.find("/non-free/") != string::npos)
      Res += 20;
   if (Path.find("/non-US/") != string::npos)
      Res += 20;
   if (Path.find("/source/") != string::npos)
      Res += 10;
   if (Path.find("/debian/") != string::npos)
      Res -= 10;
   return Res;
}
									/*}}}*/
// DropRepeats - Drop repeated files resulting from symlinks		/*{{{*/
// ---------------------------------------------------------------------
/* Here we go and stat every file that we found and strip dup inodes. */
bool DropRepeats(vector<string> &List,const char *Name)
{
   // Get a list of all the inodes
   ino_t *Inodes = new ino_t[List.size()];
   for (unsigned int I = 0; I != List.size(); I++)
   {
      struct stat Buf;
      if (stat((List[I] + Name).c_str(),&Buf) != 0 &&
	  stat((List[I] + Name + ".gz").c_str(),&Buf) != 0)
	 _error->Errno("stat","Failed to stat %s%s",List[I].c_str(),
		       Name);
      Inodes[I] = Buf.st_ino;
   }
   
   if (_error->PendingError() == true)
      return false;
   
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

// ReduceSourceList - Takes the path list and reduces it		/*{{{*/
// ---------------------------------------------------------------------
/* This takes the list of source list expressed entires and collects
   similar ones to form a single entry for each dist */
void ReduceSourcelist(string CD,vector<string> &List)
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
      string Prefix = string(*I,0,Space);
      for (vector<string>::iterator J = List.begin(); J != I; J++)
      {
	 // Find a space..
	 string::size_type Space2 = (*J).find(' ');
	 if (Space2 == string::npos)
	    continue;
	 string::size_type SSpace2 = (*J).find(' ',Space2 + 1);
	 if (SSpace2 == string::npos)
	    continue;
	 
	 if (string(*J,0,Space2) != Prefix)
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
bool WriteSourceList(string Name,vector<string> &List,bool Source)
{
   if (List.size() == 0)
      return true;

   string File = _config->FindFile("Dir::Etc::sourcelist");

   // Open the stream for reading
   ifstream F((FileExists(File)?File.c_str():"/dev/null"),
	      ios::in );
   if (!F != 0)
      return _error->Errno("ifstream::ifstream","Opening %s",File.c_str());

   string NewFile = File + ".new";
   unlink(NewFile.c_str());
   ofstream Out(NewFile.c_str());
   if (!Out)
      return _error->Errno("ofstream::ofstream",
			   "Failed to open %s.new",File.c_str());

   // Create a short uri without the path
   string ShortURI = "cdrom:[" + Name + "]/";   
   string ShortURI2 = "cdrom:" + Name + "/";     // For Compatibility

   const char *Type;
   if (Source == true)
      Type = "deb-src";
   else
      Type = "deb";
   
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
	    Out << Type << " cdrom:[" << Name << "]/" << string(*I,0,Space) <<
	       " " << string(*I,Space+1) << endl;
	 }
      }
      First = false;
      
      // Grok it
      string cType;
      string URI;
      const char *C = Buffer;
      if (ParseQuoteWord(C,cType) == false ||
	  ParseQuoteWord(C,URI) == false)
      {
	 Out << Buffer << endl;
	 continue;
      }

      // Emit lines like this one
      if (cType != Type || (string(URI,0,ShortURI.length()) != ShortURI &&
	  string(URI,0,ShortURI.length()) != ShortURI2))
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
	 
	 Out << "deb cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
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
      Prompt("Please insert a Disc in the drive and press enter");
      cout << "Mounting CD-ROM" << endl;
      if (MountCdrom(CDROM) == false)
	 return _error->Error("Failed to mount the cdrom.");
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
   vector<string> sList;
   string StartDir = SafeGetCWD();
   string InfoDir;
   if (FindPackages(CDROM,List,sList,InfoDir) == false)
   {
      cout << endl;
      return false;
   }
   
   chdir(StartDir.c_str());

   if (_config->FindB("Debug::aptcdrom",false) == true)
   {
      cout << "I found (binary):" << endl;
      for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
	 cout << *I << endl;
      cout << "I found (source):" << endl;
      for (vector<string>::iterator I = sList.begin(); I != sList.end(); I++)
	 cout << *I << endl;
   }   
   
   // Fix up the list
   DropBinaryArch(List);
   DropRepeats(List,"Packages");
   DropRepeats(sList,"Sources");
   cout << "Found " << List.size() << " package indexes and " << sList.size() << 
      " source indexes." << endl;

   if (List.size() == 0 && sList.size() == 0)
      return _error->Error("Unable to locate any package files, perhaps this is not a Debian Disc");
   
   // Check if the CD is in the database
   string Name;
   if (Database.Exists("CD::" + ID) == false ||
       _config->FindB("APT::CDROM::Rename",false) == true)
   {
      // Try to use the CDs label if at all possible
      if (InfoDir.empty() == false &&
	  FileExists(InfoDir + "/info") == true)
      {
	 ifstream F(string(InfoDir + "/info").c_str());
	 if (!F == 0)
	    getline(F,Name);

	 if (Name.empty() == false)
	 {
	    // Escape special characters
	    string::iterator J = Name.begin();
	    for (; J != Name.end(); J++)
	       if (*J == '"' || *J == ']' || *J == '[')
		  *J = '_';
	    
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
		Name.find('[') == string::npos &&
		Name.find(']') == string::npos)
	       break;
	    cout << "That is not a valid name, try again " << endl;
	 }	 
      }      
   }
   else
      Name = Database.Find("CD::" + ID);

   // Escape special characters
   string::iterator J = Name.begin();
   for (; J != Name.end(); J++)
      if (*J == '"' || *J == ']' || *J == '[')
	 *J = '_';
   
   Database.Set("CD::" + ID,Name);
   cout << "This Disc is called:" << endl << " '" << Name << "'" << endl;
   
   // Copy the package files to the state directory
   PackageCopy Copy;
   SourceCopy SrcCopy;
   if (Copy.CopyPackages(CDROM,Name,List) == false ||
       SrcCopy.CopyPackages(CDROM,Name,sList) == false)
      return false;
   
   ReduceSourcelist(CDROM,List);
   ReduceSourcelist(CDROM,sList);

   // Write the database and sourcelist
   if (_config->FindB("APT::cdrom::NoAct",false) == false)
   {
      if (WriteDatabase(Database) == false)
	 return false;
      
      cout << "Writing new source list" << endl;
      if (WriteSourceList(Name,List,false) == false ||
	  WriteSourceList(Name,sList,true) == false)
	 return false;
   }

   // Print the sourcelist entries
   cout << "Source List entries for this Disc are:" << endl;
   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
	 return _error->Error("Internal error");

      cout << "deb cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	 " " << string(*I,Space+1) << endl;
   }

   for (vector<string>::iterator I = sList.begin(); I != sList.end(); I++)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
	 return _error->Error("Internal error");

      cout << "deb-src cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	 " " << string(*I,Space+1) << endl;
   }

   cout << "Repeat this process for the rest of the CDs in your set." << endl;

   // Unmount and finish
   if (_config->FindB("APT::CDROM::NoMount",false) == false)
      UnmountCdrom(CDROM);
   
   return true;
}
									/*}}}*/
// DoIdent - Ident a CDROM						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoIdent(CommandLine &)
{
   // Startup
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;
   
   cout << "Using CD-ROM mount point " << CDROM << endl;
   cout << "Mounting CD-ROM" << endl;
   if (MountCdrom(CDROM) == false)
      return _error->Error("Failed to mount the cdrom.");
   
   // Hash the CD to get an ID
   cout << "Identifying.. " << flush;
   string ID;
   if (IdentCdrom(CDROM,ID) == false)
   {
      cout << endl;
      return false;
   }
   
   cout << '[' << ID << ']' << endl;

   // Read the database
   Configuration Database;
   string DFile = _config->FindFile("Dir::State::cdroms");
   if (FileExists(DFile) == true)
   {
      if (ReadConfigFile(Database,DFile) == false)
	 return _error->Error("Unable to read the cdrom database %s",
			      DFile.c_str());
   }
   cout << "Stored Label: '" << Database.Find("CD::" + ID) << "'" << endl;
   return true;
}
									/*}}}*/

// ShowHelp - Show the help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_OS,COMMON_CPU,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return 0;
   
   cout << 
      "Usage: apt-cdrom [options] command\n"
      "\n"
      "apt-cdrom is a tool to add CDROM's to APT's source list. The\n"
      "CDROM mount point and device information is taken from apt.conf\n"
      "and /etc/fstab.\n"
      "\n"
      "Commands:\n"
      "   add - Add a CDROM\n"
      "   ident - Report the identity of a CDROM\n"
      "\n"
      "Options:\n"
      "  -h   This help text\n"
      "  -d   CD-ROM mount point\n"
      "  -r   Rename a recognized CD-ROM\n"
      "  -m   No mounting\n"
      "  -f   Fast mode, don't check package files\n"
      "  -a   Thorough scan mode\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitary configuration option, eg -o dir::cache=/tmp\n"
      "See fstab(5)\n";
   return 0;
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
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
      {"ident",&DoIdent},
      {0,0}};

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true || _config->FindB("version") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Deal with stdout not being a tty
   if (isatty(STDOUT_FILENO) && _config->FindI("quiet",0) < 1)
      _config->Set("quiet","1");
   
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

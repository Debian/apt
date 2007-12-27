/*
 */

#include<apt-pkg/init.h>
#include<apt-pkg/error.h>
#include<apt-pkg/cdromutl.h>
#include<apt-pkg/strutl.h>
#include<apt-pkg/cdrom.h>
#include<sstream>
#include<fstream>
#include<config.h>
#include<apti18n.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <algorithm>


#include "indexcopy.h"

using namespace std;

// FindPackages - Find the package files on the CDROM			/*{{{*/
// ---------------------------------------------------------------------
/* We look over the cdrom for package files. This is a recursive
   search that short circuits when it his a package file in the dir.
   This speeds it up greatly as the majority of the size is in the
   binary-* sub dirs. */
bool pkgCdrom::FindPackages(string CD,
			    vector<string> &List,
			    vector<string> &SList, 
			    vector<string> &SigList,
			    vector<string> &TransList,
			    string &InfoDir, pkgCdromStatus *log,
			    unsigned int Depth)
{
   static ino_t Inodes[9];
   DIR *D;

   // if we have a look we "pulse" now
   if(log)
      log->Update();

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


   /* Check _first_ for a signature file as apt-cdrom assumes that all files
      under a Packages/Source file are in control of that file and stops 
      the scanning
   */
   if (stat("Release.gpg",&Buf) == 0)
   {
      SigList.push_back(CD);
   }
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

   // see if we find translatin indexes
   if (stat("i18n",&Buf) == 0)
   {
      D = opendir("i18n");
      for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
      {
	 if(strstr(Dir->d_name,"Translation") != NULL) 
	 {
	    if (_config->FindB("Debug::aptcdrom",false) == true)
	       std::clog << "found translations: " << Dir->d_name << "\n";
	    string file = Dir->d_name;
	    if(file.substr(file.size()-3,file.size()) == ".gz")
	       file = file.substr(0,file.size()-3);
	    TransList.push_back(CD+"i18n/"+ file);
	 }
      }
      closedir(D);
   }

   
   D = opendir(".");
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
      if (FindPackages(CD + Dir->d_name,List,SList,SigList,TransList,InfoDir,log,Depth+1) == false)
	 break;

      if (chdir(CD.c_str()) != 0)
	 return _error->Errno("chdir","Unable to change to %s",CD.c_str());
   };

   closedir(D);
   
   return !_error->PendingError();
}

// Score - We compute a 'score' for a path				/*{{{*/
// ---------------------------------------------------------------------
/* Paths are scored based on how close they come to what I consider
   normal. That is ones that have 'dist' 'stable' 'testing' will score
   higher than ones without. */
int pkgCdrom::Score(string Path)
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

   // check for symlinks in the patch leading to the actual file
   // a symlink gets a big penalty
   struct stat Buf;
   string statPath = flNotFile(Path);
   string cdromPath = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   while(statPath != cdromPath && statPath != "./") {
      statPath.resize(statPath.size()-1);  // remove the trailing '/'
      if (lstat(statPath.c_str(),&Buf) == 0) {
        if(S_ISLNK(Buf.st_mode)) {
           Res -= 60;
           break;
        }
      }
      statPath = flNotFile(statPath); // descent
   }

   return Res;
}

									/*}}}*/
// DropBinaryArch - Dump dirs with a string like /binary-<foo>/		/*{{{*/
// ---------------------------------------------------------------------
/* Here we drop everything that is not this machines arch */
bool pkgCdrom::DropBinaryArch(vector<string> &List)
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


// DropRepeats - Drop repeated files resulting from symlinks		/*{{{*/
// ---------------------------------------------------------------------
/* Here we go and stat every file that we found and strip dup inodes. */
bool pkgCdrom::DropRepeats(vector<string> &List,const char *Name)
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
void pkgCdrom::ReduceSourcelist(string CD,vector<string> &List)
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
bool pkgCdrom::WriteDatabase(Configuration &Cnf)
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
bool pkgCdrom::WriteSourceList(string Name,vector<string> &List,bool Source)
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

   string Type;
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
      if (F.fail() && !F.eof())
	 return _error->Error(_("Line %u too long in source list %s."),
			      CurLine,File.c_str());
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


bool pkgCdrom::Ident(string &ident, pkgCdromStatus *log)
{
   stringstream msg;

   // Startup
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;

   if(log) {
      msg.str("");
      ioprintf(msg, _("Using CD-ROM mount point %s\nMounting CD-ROM\n"),
		      CDROM.c_str());
      log->Update(msg.str());
   }
   if (MountCdrom(CDROM) == false)
      return _error->Error("Failed to mount the cdrom.");

   // Hash the CD to get an ID
   if(log) 
      log->Update(_("Identifying.. "));
   

   if (IdentCdrom(CDROM,ident) == false)
   {
      ident = "";
      return false;
   }

   msg.str("");
   ioprintf(msg, "[%s]\n",ident.c_str());
   log->Update(msg.str());


   // Read the database
   Configuration Database;
   string DFile = _config->FindFile("Dir::State::cdroms");
   if (FileExists(DFile) == true)
   {
      if (ReadConfigFile(Database,DFile) == false)
	 return _error->Error("Unable to read the cdrom database %s",
			      DFile.c_str());
   }
   if(log) {
      msg.str("");
      ioprintf(msg, _("Stored label: %s\n"),
      Database.Find("CD::"+ident).c_str());
      log->Update(msg.str());
   }

   // Unmount and finish
   if (_config->FindB("APT::CDROM::NoMount",false) == false) {
      log->Update(_("Unmounting CD-ROM...\n"), STEP_LAST);
      UnmountCdrom(CDROM);
   }

   return true;
}


bool pkgCdrom::Add(pkgCdromStatus *log)
{
   stringstream msg;

   // Startup
   string CDROM = _config->FindDir("Acquire::cdrom::mount","/cdrom/");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;
   
   if(log) {
      log->SetTotal(STEP_LAST);
      msg.str("");
      ioprintf(msg, _("Using CD-ROM mount point %s\n"), CDROM.c_str());
      log->Update(msg.str(), STEP_PREPARE);
   }

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
      if(log)
	 log->Update(_("Unmounting CD-ROM\n"), STEP_UNMOUNT);
      UnmountCdrom(CDROM);

      if(log) {
	 log->Update(_("Waiting for disc...\n"), STEP_WAIT);
	 if(!log->ChangeCdrom()) {
	    // user aborted
	    return false; 
	 }
      }

      // Mount the new CDROM
      log->Update(_("Mounting CD-ROM...\n"), STEP_MOUNT);
      if (MountCdrom(CDROM) == false)
	 return _error->Error("Failed to mount the cdrom.");
   }
   
   // Hash the CD to get an ID
   if(log)
      log->Update(_("Identifying.. "), STEP_IDENT);
   string ID;
   if (IdentCdrom(CDROM,ID) == false)
   {
      log->Update("\n");
      return false;
   }
   if(log) 
      log->Update("["+ID+"]\n");

   if(log) 
      log->Update(_("Scanning disc for index files..\n"),STEP_SCAN);
   
   // Get the CD structure
   vector<string> List;
   vector<string> SourceList;
   vector<string> SigList;
   vector<string> TransList;
   string StartDir = SafeGetCWD();
   string InfoDir;
   if (FindPackages(CDROM,List,SourceList, SigList,TransList,InfoDir,log) == false)
   {
      log->Update("\n");
      return false;
   }

   chdir(StartDir.c_str());

   if (_config->FindB("Debug::aptcdrom",false) == true)
   {
      cout << "I found (binary):" << endl;
      for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
	 cout << *I << endl;
      cout << "I found (source):" << endl;
      for (vector<string>::iterator I = SourceList.begin(); I != SourceList.end(); I++)
	 cout << *I << endl;
      cout << "I found (Signatures):" << endl;
      for (vector<string>::iterator I = SigList.begin(); I != SigList.end(); I++)
	 cout << *I << endl;
   }   

   //log->Update(_("Cleaning package lists..."), STEP_CLEAN);

   // Fix up the list
   DropBinaryArch(List);
   DropRepeats(List,"Packages");
   DropRepeats(SourceList,"Sources");
   DropRepeats(SigList,"Release.gpg");
   DropRepeats(TransList,"");
   if(log) {
      msg.str("");
      ioprintf(msg, _("Found %zu package indexes, %zu source indexes, "
		      "%zu translation indexes and %zu signatures\n"), 
	       List.size(), SourceList.size(), TransList.size(),
	       SigList.size());
      log->Update(msg.str(), STEP_SCAN);
   }

   if (List.size() == 0 && SourceList.size() == 0) 
   {
      if (_config->FindB("APT::CDROM::NoMount",false) == false) 
	 UnmountCdrom(CDROM);
      return _error->Error("Unable to locate any package files, perhaps this is not a Debian Disc");
   }

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
	    
	    if(log) {
	       msg.str("");
	       ioprintf(msg, _("Found label '%s'\n"), Name.c_str());
	       log->Update(msg.str());
	    }
	    Database.Set("CD::" + ID + "::Label",Name);
	 }	 
      }
      
      if (_config->FindB("APT::CDROM::Rename",false) == true ||
	  Name.empty() == true)
      {
	 if(!log) 
         {
	    if (_config->FindB("APT::CDROM::NoMount",false) == false) 
	       UnmountCdrom(CDROM);
	    return _error->Error("No disc name found and no way to ask for it");
	 }

	 while(true) {
	    if(!log->AskCdromName(Name)) {
	       // user canceld
	       return false; 
	    }
	    cout << "Name: '" << Name << "'" << endl;

	    if (Name.empty() == false &&
		Name.find('"') == string::npos &&
		Name.find('[') == string::npos &&
		Name.find(']') == string::npos)
	       break;
	    log->Update(_("That is not a valid name, try again.\n"));
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
   if(log) {
      msg.str("");
      ioprintf(msg, _("This disc is called: \n'%s'\n"), Name.c_str());
      log->Update(msg.str());
   }

   log->Update(_("Copying package lists..."), STEP_COPY);
   // take care of the signatures and copy them if they are ok
   // (we do this before PackageCopy as it modifies "List" and "SourceList")
   SigVerify SignVerify;
   SignVerify.CopyAndVerify(CDROM, Name, SigList, List, SourceList);
   
   // Copy the package files to the state directory
   PackageCopy Copy;
   SourceCopy SrcCopy;
   TranslationsCopy TransCopy;
   if (Copy.CopyPackages(CDROM,Name,List, log) == false ||
       SrcCopy.CopyPackages(CDROM,Name,SourceList, log) == false ||
       TransCopy.CopyTranslations(CDROM,Name,TransList, log) == false)
      return false;

   // reduce the List so that it takes less space in sources.list
   ReduceSourcelist(CDROM,List);
   ReduceSourcelist(CDROM,SourceList);

   // Write the database and sourcelist
   if (_config->FindB("APT::cdrom::NoAct",false) == false)
   {
      if (WriteDatabase(Database) == false)
	 return false;
      
      if(log) {
	 log->Update(_("Writing new source list\n"), STEP_WRITE);
      }
      if (WriteSourceList(Name,List,false) == false ||
	  WriteSourceList(Name,SourceList,true) == false)
	 return false;
   }

   // Print the sourcelist entries
   if(log) 
      log->Update(_("Source list entries for this disc are:\n"));

   for (vector<string>::iterator I = List.begin(); I != List.end(); I++)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
      {
	 if (_config->FindB("APT::CDROM::NoMount",false) == false) 
	    UnmountCdrom(CDROM);
	 return _error->Error("Internal error");
      }

      if(log) {
	 msg.str("");
	 msg << "deb cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
	 log->Update(msg.str());
      }
   }

   for (vector<string>::iterator I = SourceList.begin(); I != SourceList.end(); I++)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
      {
	 if (_config->FindB("APT::CDROM::NoMount",false) == false) 
	    UnmountCdrom(CDROM);
	 return _error->Error("Internal error");
      }

      if(log) {
	 msg.str("");
	 msg << "deb-src cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
	 log->Update(msg.str());
      }
   }

   

   // Unmount and finish
   if (_config->FindB("APT::CDROM::NoMount",false) == false) {
      log->Update(_("Unmounting CD-ROM...\n"), STEP_LAST);
      UnmountCdrom(CDROM);
   }

   return true;
}

/*
 */
#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/cdromutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/cdrom.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexcopy.h>


#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <algorithm>
#include <dlfcn.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include<apti18n.h>

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
   if (InfoDir.empty() == true)
   {
      if (DirectoryExists(".disk") == true)
	 InfoDir = InfoDir + CD + ".disk/";
   }

   // Don't look into directories that have been marked to ignore.
   if (RealFileExists(".aptignr") == true)
      return true;

   /* Check _first_ for a signature file as apt-cdrom assumes that all files
      under a Packages/Source file are in control of that file and stops 
      the scanning
   */
   if (RealFileExists("Release.gpg") == true || RealFileExists("InRelease") == true)
   {
      SigList.push_back(CD);
   }

   /* Aha! We found some package files. We assume that everything under 
      this dir is controlled by those package files so we don't look down
      anymore */
   std::vector<APT::Configuration::Compressor> const compressor = APT::Configuration::getCompressors();
   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	c != compressor.end(); ++c)
   {
      if (RealFileExists(std::string("Packages").append(c->Extension).c_str()) == false)
	 continue;

      if (_config->FindB("Debug::aptcdrom",false) == true)
	 std::clog << "Found Packages in " << CD << std::endl;
      List.push_back(CD);

      // Continue down if thorough is given
      if (_config->FindB("APT::CDROM::Thorough",false) == false)
	 return true;
      break;
   }
   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	c != compressor.end(); ++c)
   {
      if (RealFileExists(std::string("Sources").append(c->Extension).c_str()) == false)
	 continue;

      if (_config->FindB("Debug::aptcdrom",false) == true)
	 std::clog << "Found Sources in " << CD << std::endl;
      SList.push_back(CD);

      // Continue down if thorough is given
      if (_config->FindB("APT::CDROM::Thorough",false) == false)
	 return true;
      break;
   }

   // see if we find translation indices
   if (DirectoryExists("i18n") == true)
   {
      D = opendir("i18n");
      for (struct dirent *Dir = readdir(D); Dir != 0; Dir = readdir(D))
      {
	 if(strncmp(Dir->d_name, "Translation-", strlen("Translation-")) != 0)
	    continue;
	 string file = Dir->d_name;
	 for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	      c != compressor.end(); ++c)
	 {
	    string fileext = flExtension(file);
	    if (file == fileext)
	       fileext.clear();
	    else if (fileext.empty() == false)
	       fileext = "." + fileext;

	    if (c->Extension == fileext)
	    {
	       if (_config->FindB("Debug::aptcdrom",false) == true)
		  std::clog << "Found translation " << Dir->d_name << " in " << CD << "i18n/" << std::endl;
	       file.erase(file.size() - fileext.size());
	       TransList.push_back(CD + "i18n/" + file);
	       break;
	    }
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
	  strcmp(Dir->d_name,".disk") == 0 ||
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
      {
	 _error->Errno("chdir","Unable to change to %s", CD.c_str());
	 closedir(D);
	 return false;
      }
   };

   closedir(D);
   
   return !_error->PendingError();
}
									/*}}}*/
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
   string cdromPath = _config->FindDir("Acquire::cdrom::mount");
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

   for (unsigned int I = 0; I < List.size(); I++)
   {
      const char *Str = List[I].c_str();
      const char *Start, *End;
      if ((Start = strstr(Str,"/binary-")) == 0)
	 continue;

      // Between Start and End is the architecture
      Start += 8;
      if ((End = strstr(Start,"/")) != 0 && Start != End &&
          APT::Configuration::checkArchitecture(string(Start, End)) == true)
	 continue; // okay, architecture is accepted

      // not accepted -> Erase it
      List.erase(List.begin() + I);
      --I; // the next entry is at the same index after the erase
   }
   
   return true;
}
									/*}}}*/
// DropTranslation - Dump unwanted Translation-<lang> files		/*{{{*/
// ---------------------------------------------------------------------
/* Here we drop everything that is not configured in Acquire::Languages */
bool pkgCdrom::DropTranslation(vector<string> &List)
{
   for (unsigned int I = 0; I < List.size(); I++)
   {
      const char *Start;
      if ((Start = strstr(List[I].c_str(), "/Translation-")) == NULL)
	 continue;
      Start += strlen("/Translation-");

      if (APT::Configuration::checkLanguage(Start, true) == true)
	 continue;

      // not accepted -> Erase it
      List.erase(List.begin() + I);
      --I; // the next entry is at the same index after the erase
   }

   return true;
}
									/*}}}*/
// DropRepeats - Drop repeated files resulting from symlinks		/*{{{*/
// ---------------------------------------------------------------------
/* Here we go and stat every file that we found and strip dup inodes. */
bool pkgCdrom::DropRepeats(vector<string> &List,const char *Name)
{
   bool couldFindAllFiles = true;
   // Get a list of all the inodes
   ino_t *Inodes = new ino_t[List.size()];
   for (unsigned int I = 0; I != List.size(); ++I)
   {
      struct stat Buf;
      bool found = false;

      std::vector<APT::Configuration::Compressor> const compressor = APT::Configuration::getCompressors();
      for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressor.begin();
	   c != compressor.end(); ++c)
      {
	 std::string filename = std::string(List[I]).append(Name).append(c->Extension);
         if (stat(filename.c_str(), &Buf) != 0)
	    continue;
	 Inodes[I] = Buf.st_ino;
	 found = true;
	 break;
      }

      if (found == false)
      {
	 _error->Errno("stat","Failed to stat %s%s",List[I].c_str(), Name);
	 couldFindAllFiles = false;
	 Inodes[I] = 0;
      }
   }

   // Look for dups
   for (unsigned int I = 0; I != List.size(); I++)
   {
      if (Inodes[I] == 0)
	 continue;
      for (unsigned int J = I+1; J < List.size(); J++)
      {
	 // No match
	 if (Inodes[J] == 0 || Inodes[J] != Inodes[I])
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
   delete[] Inodes;

   // Wipe erased entries
   for (unsigned int I = 0; I < List.size();)
   {
      if (List[I].empty() == false)
	 I++;
      else
	 List.erase(List.begin()+I);
   }
   
   return couldFindAllFiles;
}
									/*}}}*/
// ReduceSourceList - Takes the path list and reduces it		/*{{{*/
// ---------------------------------------------------------------------
/* This takes the list of source list expressed entries and collects
   similar ones to form a single entry for each dist */
void pkgCdrom::ReduceSourcelist(string /*CD*/,vector<string> &List)
{
   sort(List.begin(),List.end());
   
   // Collect similar entries
   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
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
      string Component = string(*I,SSpace);
      for (vector<string>::iterator J = List.begin(); J != I; ++J)
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

	 string Component2 = string(*J, SSpace2) + " ";
	 if (Component2.find(Component + " ") == std::string::npos)
	    *J += Component;
	 I->clear();
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

   RemoveFile("WriteDatabase", NewFile);
   ofstream Out(NewFile.c_str());
   if (!Out)
      return _error->Errno("ofstream::ofstream",
			   "Failed to open %s.new",DFile.c_str());
   
   /* Write out all of the configuration directives by walking the
      configuration tree */
   Cnf.Dump(Out, NULL, "%f \"%v\";\n", false);

   Out.close();

   if (FileExists(DFile) == true)
      rename(DFile.c_str(), (DFile + '~').c_str());
   if (rename(NewFile.c_str(),DFile.c_str()) != 0)
      return _error->Errno("rename","Failed to rename %s.new to %s",
			   DFile.c_str(),DFile.c_str());

   return true;
}
									/*}}}*/
// WriteSourceList - Write an updated sourcelist			/*{{{*/
// ---------------------------------------------------------------------
/* This reads the old source list and copies it into the new one. It 
   appends the new CDROM entries just after the first block of comments.
   This places them first in the file. It also removes any old entries
   that were the same. */
bool pkgCdrom::WriteSourceList(string Name,vector<string> &List,bool Source)
{
   if (List.empty() == true)
      return true;

   string File = _config->FindFile("Dir::Etc::sourcelist");

   // Open the stream for reading
   ifstream F((FileExists(File)?File.c_str():"/dev/null"),
	      ios::in );
   if (F.fail() == true)
      return _error->Errno("ifstream::ifstream","Opening %s",File.c_str());

   string NewFile = File + ".new";
   RemoveFile("WriteDatabase", NewFile);
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
	 for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
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
      for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
      {
	 string::size_type Space = (*I).find(' ');
	 if (Space == string::npos)
	    return _error->Error("Internal error");
	 
	 Out << "deb cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
      }
   }
   
   Out.close();

   rename(File.c_str(), (File + '~').c_str());
   if (rename(NewFile.c_str(),File.c_str()) != 0)
      return _error->Errno("rename","Failed to rename %s.new to %s",
			   File.c_str(),File.c_str());
   
   return true;
}
									/*}}}*/
bool pkgCdrom::UnmountCDROM(std::string const &CDROM, pkgCdromStatus * const log)/*{{{*/
{
   if (_config->FindB("APT::CDROM::NoMount",false) == true)
      return true;
   if (log != NULL)
      log->Update(_("Unmounting CD-ROM...\n"), STEP_LAST);
   return UnmountCdrom(CDROM);
}
									/*}}}*/
bool pkgCdrom::MountAndIdentCDROM(Configuration &Database, std::string &CDROM, std::string &ident, pkgCdromStatus * const log, bool const interactive)/*{{{*/
{
   // Startup
   CDROM = _config->FindDir("Acquire::cdrom::mount");
   if (CDROM[0] == '.')
      CDROM= SafeGetCWD() + '/' + CDROM;

   if (log != NULL)
   {
      string msg;
      log->SetTotal(STEP_LAST);
      strprintf(msg, _("Using CD-ROM mount point %s\n"), CDROM.c_str());
      log->Update(msg, STEP_PREPARE);
   }

   // Unmount the CD and get the user to put in the one they want
   if (_config->FindB("APT::CDROM::NoMount", false) == false)
   {
      if (interactive == true)
      {
	 UnmountCDROM(CDROM, log);

	 if(log != NULL)
	 {
	    log->Update(_("Waiting for disc...\n"), STEP_WAIT);
	    if(!log->ChangeCdrom()) {
	       // user aborted
	       return false;
	    }
	 }
      }

      // Mount the new CDROM
      if(log != NULL)
	 log->Update(_("Mounting CD-ROM...\n"), STEP_MOUNT);

      if (MountCdrom(CDROM) == false)
	 return _error->Error("Failed to mount the cdrom.");
   }

   if (IsMounted(CDROM) == false)
      return _error->Error("Failed to mount the cdrom.");

   // Hash the CD to get an ID
   if (log != NULL)
      log->Update(_("Identifying... "), STEP_IDENT);

   if (IdentCdrom(CDROM,ident) == false)
   {
      ident = "";
      if (log != NULL)
	 log->Update("\n");
      UnmountCDROM(CDROM, NULL);
      return false;
   }

   if (log != NULL)
   {
      string msg;
      strprintf(msg, "[%s]\n", ident.c_str());
      log->Update(msg);
   }

   // Read the database
   string DFile = _config->FindFile("Dir::State::cdroms");
   if (FileExists(DFile) == true)
   {
      if (ReadConfigFile(Database,DFile) == false)
      {
	 UnmountCDROM(CDROM, NULL);
	 return _error->Error("Unable to read the cdrom database %s",
			      DFile.c_str());
      }
   }
   return true;
}
									/*}}}*/
bool pkgCdrom::Ident(string &ident, pkgCdromStatus *log)		/*{{{*/
{
   Configuration Database;
   std::string CDROM;
   if (MountAndIdentCDROM(Database, CDROM, ident, log, false) == false)
      return false;

   if (log != NULL)
   {
      string msg;
      strprintf(msg, _("Stored label: %s\n"),
	    Database.Find("CD::"+ident).c_str());
      log->Update(msg);
   }

   // Unmount and finish
   UnmountCDROM(CDROM, log);
   return true;
}
									/*}}}*/
bool pkgCdrom::Add(pkgCdromStatus *log)					/*{{{*/
{
   Configuration Database;
   std::string ID, CDROM;
   if (MountAndIdentCDROM(Database, CDROM, ID, log, true) == false)
      return false;

   if(log != NULL)
      log->Update(_("Scanning disc for index files...\n"),STEP_SCAN);

   // Get the CD structure
   vector<string> List;
   vector<string> SourceList;
   vector<string> SigList;
   vector<string> TransList;
   string StartDir = SafeGetCWD();
   string InfoDir;
   if (FindPackages(CDROM,List,SourceList, SigList,TransList,InfoDir,log) == false)
   {
      if (log != NULL)
	 log->Update("\n");
      UnmountCDROM(CDROM, NULL);
      return false;
   }

   if (chdir(StartDir.c_str()) != 0)
   {
      UnmountCDROM(CDROM, NULL);
      return _error->Errno("chdir","Unable to change to %s", StartDir.c_str());
   }

   if (_config->FindB("Debug::aptcdrom",false) == true)
   {
      cout << "I found (binary):" << endl;
      for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
	 cout << *I << endl;
      cout << "I found (source):" << endl;
      for (vector<string>::iterator I = SourceList.begin(); I != SourceList.end(); ++I)
	 cout << *I << endl;
      cout << "I found (Signatures):" << endl;
      for (vector<string>::iterator I = SigList.begin(); I != SigList.end(); ++I)
	 cout << *I << endl;
   }   

   //log->Update(_("Cleaning package lists..."), STEP_CLEAN);

   // Fix up the list
   DropBinaryArch(List);
   DropRepeats(List,"Packages");
   DropRepeats(SourceList,"Sources");
   // FIXME: We ignore stat() errors here as we usually have only one of those in use
   // This has little potencial to drop 'valid' stat() errors as we know that one of these
   // files need to exist, but it would be better if we would check it here
   _error->PushToStack();
   DropRepeats(SigList,"Release.gpg");
   DropRepeats(SigList,"InRelease");
   _error->RevertToStack();
   DropRepeats(TransList,"");
   if (_config->FindB("APT::CDROM::DropTranslation", true) == true)
      DropTranslation(TransList);
   if(log != NULL) {
      string msg;
      strprintf(msg, _("Found %zu package indexes, %zu source indexes, "
		      "%zu translation indexes and %zu signatures\n"), 
	       List.size(), SourceList.size(), TransList.size(),
	       SigList.size());
      log->Update(msg, STEP_SCAN);
   }

   if (List.empty() == true && SourceList.empty() == true) 
   {
      UnmountCDROM(CDROM, NULL);
      return _error->Error(_("Unable to locate any package files, perhaps this is not a Debian Disc or the wrong architecture?"));
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
	 ifstream F((InfoDir + "/info").c_str());
	 if (F.good() == true)
	    getline(F,Name);

	 if (Name.empty() == false)
	 {
	    // Escape special characters
	    string::iterator J = Name.begin();
	    for (; J != Name.end(); ++J)
	       if (*J == '"' || *J == ']' || *J == '[')
		  *J = '_';
	    
	    if(log != NULL)
	    {
	       string msg;
	       strprintf(msg, _("Found label '%s'\n"), Name.c_str());
	       log->Update(msg);
	    }
	    Database.Set("CD::" + ID + "::Label",Name);
	 }	 
      }
      
      if (_config->FindB("APT::CDROM::Rename",false) == true ||
	  Name.empty() == true)
      {
	 if(log == NULL) 
         {
	    UnmountCDROM(CDROM, NULL);
	    return _error->Error("No disc name found and no way to ask for it");
	 }

	 while(true) {
	    if(!log->AskCdromName(Name)) {
	       // user canceld
	       UnmountCDROM(CDROM, NULL);
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
   for (; J != Name.end(); ++J)
      if (*J == '"' || *J == ']' || *J == '[')
	 *J = '_';
   
   Database.Set("CD::" + ID,Name);
   if(log != NULL)
   {
      string msg;
      strprintf(msg, _("This disc is called: \n'%s'\n"), Name.c_str());
      log->Update(msg);
      log->Update(_("Copying package lists..."), STEP_COPY);
   }

   // check for existence and possibly create state directory for copying
   string const listDir = _config->FindDir("Dir::State::lists");
   string const partialListDir = listDir + "partial/";
   mode_t const mode = umask(S_IWGRP | S_IWOTH);
   bool const creation_fail = (CreateAPTDirectoryIfNeeded(_config->FindDir("Dir::State"), partialListDir) == false &&
	 CreateAPTDirectoryIfNeeded(listDir, partialListDir) == false);
   umask(mode);
   if (creation_fail == true)
   {
      UnmountCDROM(CDROM, NULL);
      return _error->Errno("cdrom", _("List directory %spartial is missing."), listDir.c_str());
   }

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
   {
      UnmountCDROM(CDROM, NULL);
      return false;
   }

   // reduce the List so that it takes less space in sources.list
   ReduceSourcelist(CDROM,List);
   ReduceSourcelist(CDROM,SourceList);

   // Write the database and sourcelist
   if (_config->FindB("APT::cdrom::NoAct",false) == false)
   {
      if (WriteDatabase(Database) == false)
      {
	 UnmountCDROM(CDROM, NULL);
	 return false;
      }

      if(log != NULL)
	 log->Update(_("Writing new source list\n"), STEP_WRITE);
      if (WriteSourceList(Name,List,false) == false ||
	  WriteSourceList(Name,SourceList,true) == false)
      {
	 UnmountCDROM(CDROM, NULL);
	 return false;
      }
   }

   // Print the sourcelist entries
   if(log != NULL)
      log->Update(_("Source list entries for this disc are:\n"));

   for (vector<string>::iterator I = List.begin(); I != List.end(); ++I)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
      {
	 UnmountCDROM(CDROM, NULL);
	 return _error->Error("Internal error");
      }

      if(log != NULL)
      {
	 stringstream msg;
	 msg << "deb cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
	 log->Update(msg.str());
      }
   }

   for (vector<string>::iterator I = SourceList.begin(); I != SourceList.end(); ++I)
   {
      string::size_type Space = (*I).find(' ');
      if (Space == string::npos)
      {
	 UnmountCDROM(CDROM, NULL);
	 return _error->Error("Internal error");
      }

      if(log != NULL) {
	 stringstream msg;
	 msg << "deb-src cdrom:[" << Name << "]/" << string(*I,0,Space) << 
	    " " << string(*I,Space+1) << endl;
	 log->Update(msg.str());
      }
   }

   // Unmount and finish
   UnmountCDROM(CDROM, log);
   return true;
}
									/*}}}*/
pkgUdevCdromDevices::pkgUdevCdromDevices()				/*{{{*/
: d(NULL), libudev_handle(NULL), udev_new(NULL), udev_enumerate_add_match_property(NULL),
   udev_enumerate_scan_devices(NULL), udev_enumerate_get_list_entry(NULL),
   udev_device_new_from_syspath(NULL), udev_enumerate_get_udev(NULL),
   udev_list_entry_get_name(NULL), udev_device_get_devnode(NULL),
   udev_enumerate_new(NULL), udev_list_entry_get_next(NULL),
   udev_device_get_property_value(NULL), udev_enumerate_add_match_sysattr(NULL)
{
}
									/*}}}*/

bool pkgUdevCdromDevices::Dlopen()					/*{{{*/
{
   // alread open
   if(libudev_handle != NULL)
      return true;

   // see if we can get libudev
   void *h = ::dlopen("libudev.so.0", RTLD_LAZY);
   if(h == NULL)
      return false;

   // get the pointers to the udev structs
   libudev_handle = h;
   udev_new = (udev* (*)(void)) dlsym(h, "udev_new");
   udev_enumerate_add_match_property = (int (*)(udev_enumerate*, const char*, const char*))dlsym(h, "udev_enumerate_add_match_property");
   udev_enumerate_add_match_sysattr = (int (*)(udev_enumerate*, const char*, const char*))dlsym(h, "udev_enumerate_add_match_sysattr");
   udev_enumerate_scan_devices = (int (*)(udev_enumerate*))dlsym(h, "udev_enumerate_scan_devices");
   udev_enumerate_get_list_entry = (udev_list_entry* (*)(udev_enumerate*))dlsym(h, "udev_enumerate_get_list_entry");
   udev_device_new_from_syspath = (udev_device* (*)(udev*, const char*))dlsym(h, "udev_device_new_from_syspath");
   udev_enumerate_get_udev = (udev* (*)(udev_enumerate*))dlsym(h, "udev_enumerate_get_udev");
   udev_list_entry_get_name = (const char* (*)(udev_list_entry*))dlsym(h, "udev_list_entry_get_name");
   udev_device_get_devnode = (const char* (*)(udev_device*))dlsym(h, "udev_device_get_devnode");
   udev_enumerate_new = (udev_enumerate* (*)(udev*))dlsym(h, "udev_enumerate_new");
   udev_list_entry_get_next = (udev_list_entry* (*)(udev_list_entry*))dlsym(h, "udev_list_entry_get_next");
   udev_device_get_property_value = (const char* (*)(udev_device *, const char *))dlsym(h, "udev_device_get_property_value");

   return true;
}
									/*}}}*/
// convenience interface, this will just call ScanForRemovable		/*{{{*/
vector<CdromDevice> pkgUdevCdromDevices::Scan()
{
   bool CdromOnly = _config->FindB("APT::cdrom::CdromOnly", true);
   return ScanForRemovable(CdromOnly);
}
									/*}}}*/
vector<CdromDevice> pkgUdevCdromDevices::ScanForRemovable(bool CdromOnly)/*{{{*/
{
   vector<CdromDevice> cdrom_devices;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *l, *devices;
   struct udev *udev_ctx;

   if(libudev_handle == NULL)
      return cdrom_devices;

   udev_ctx = udev_new();
   enumerate = udev_enumerate_new (udev_ctx);
   if (CdromOnly)
      udev_enumerate_add_match_property(enumerate, "ID_CDROM", "1");
   else {
      udev_enumerate_add_match_sysattr(enumerate, "removable", "1");
   }

   udev_enumerate_scan_devices (enumerate);
   devices = udev_enumerate_get_list_entry (enumerate);
   for (l = devices; l != NULL; l = udev_list_entry_get_next (l))
   {
      CdromDevice cdrom;
      struct udev_device *udevice;
      udevice = udev_device_new_from_syspath (udev_enumerate_get_udev (enumerate), udev_list_entry_get_name (l));
      if (udevice == NULL)
	 continue;
      const char* devnode = udev_device_get_devnode(udevice);

      // try fstab_dir first
      string mountpath;
      const char* mp = udev_device_get_property_value(udevice, "FSTAB_DIR");
      if (mp)
         mountpath = string(mp);
      else
         mountpath = FindMountPointForDevice(devnode);

      // fill in the struct
      cdrom.DeviceName = string(devnode);
      if (mountpath != "") {
	 cdrom.MountPath = mountpath;
	 string s = mountpath;
	 cdrom.Mounted = IsMounted(s);
      } else {
	 cdrom.Mounted = false;
	 cdrom.MountPath = "";
      }
      cdrom_devices.push_back(cdrom);
   } 
   return cdrom_devices;
}
									/*}}}*/

pkgUdevCdromDevices::~pkgUdevCdromDevices()                             /*{{{*/
{ 
   if (libudev_handle != NULL)
      dlclose(libudev_handle);
}
									/*}}}*/

pkgCdromStatus::pkgCdromStatus() : d(NULL), totalSteps(0) {}
pkgCdromStatus::~pkgCdromStatus() {}

pkgCdrom::pkgCdrom() : d(NULL) {}
pkgCdrom::~pkgCdrom() {}

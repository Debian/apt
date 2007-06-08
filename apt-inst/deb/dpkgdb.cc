// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgdb.cc,v 1.7.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   DPKGv1 Database Implemenation
   
   This class provides parsers and other implementations for the DPKGv1
   database. It reads the diversion file, the list files and the status
   file to build both the list of currently installed files and the 
   currently installed package list.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/dpkgdb.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/strutl.h>

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <iostream>
#include <apti18n.h>
									/*}}}*/
using namespace std;

// EraseDir - Erase A Directory						/*{{{*/
// ---------------------------------------------------------------------
/* This is necessary to create a new empty sub directory. The caller should
   invoke mkdir after this with the proper permissions and check for 
   error. Maybe stick this in fileutils */
static bool EraseDir(const char *Dir)
{
   // First we try a simple RM
   if (rmdir(Dir) == 0 ||
       errno == ENOENT)
      return true;
   
   // A file? Easy enough..
   if (errno == ENOTDIR)
   {
      if (unlink(Dir) != 0)
	 return _error->Errno("unlink",_("Failed to remove %s"),Dir);
      return true;
   }
   
   // Should not happen
   if (errno != ENOTEMPTY)
      return _error->Errno("rmdir",_("Failed to remove %s"),Dir);
   
   // Purge it using rm
   pid_t Pid = ExecFork();

   // Spawn the subprocess
   if (Pid == 0)
   {
      execlp(_config->Find("Dir::Bin::rm","/bin/rm").c_str(),
	     "rm","-rf","--",Dir,(char *)NULL);
      _exit(100);
   }
   return ExecWait(Pid,_config->Find("dir::bin::rm","/bin/rm").c_str());
}
									/*}}}*/
// DpkgDB::debDpkgDB - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debDpkgDB::debDpkgDB() : CacheMap(0), FileMap(0)
{
   AdminDir = flNotFile(_config->Find("Dir::State::status"));   
   DiverInode = 0;
   DiverTime = 0;
}
									/*}}}*/
// DpkgDB::~debDpkgDB - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
debDpkgDB::~debDpkgDB()
{
   delete Cache;
   Cache = 0;
   delete CacheMap;
   CacheMap = 0;
   
   delete FList;
   FList = 0;
   delete FileMap;
   FileMap = 0;
}
									/*}}}*/
// DpkgDB::InitMetaTmp - Get the temp dir for meta information		/*{{{*/
// ---------------------------------------------------------------------
/* This creats+empties the meta temporary directory /var/lib/dpkg/tmp.ci
   Only one package at a time can be using the returned meta directory. */
bool debDpkgDB::InitMetaTmp(string &Dir)
{
   string Tmp = AdminDir + "tmp.ci/";
   if (EraseDir(Tmp.c_str()) == false)
      return _error->Error(_("Unable to create %s"),Tmp.c_str());
   if (mkdir(Tmp.c_str(),0755) != 0)
      return _error->Errno("mkdir",_("Unable to create %s"),Tmp.c_str());
   
   // Verify it is on the same filesystem as the main info directory
   dev_t Dev;
   struct stat St;
   if (stat((AdminDir + "info").c_str(),&St) != 0)
      return _error->Errno("stat",_("Failed to stat %sinfo"),AdminDir.c_str());
   Dev = St.st_dev;
   if (stat(Tmp.c_str(),&St) != 0)
      return _error->Errno("stat",_("Failed to stat %s"),Tmp.c_str());
   if (Dev != St.st_dev)
      return _error->Error(_("The info and temp directories need to be on the same filesystem"));
   
   // Done
   Dir = Tmp;
   return true;
}
									/*}}}*/
// DpkgDB::ReadyPkgCache - Prepare the cache with the current status	/*{{{*/
// ---------------------------------------------------------------------
/* This reads in the status file into an empty cache. This really needs 
   to be somehow unified with the high level APT notion of the Database
   directory, but there is no clear way on how to do that yet. */
bool debDpkgDB::ReadyPkgCache(OpProgress &Progress)
{
   if (Cache != 0)
   {  
      Progress.OverallProgress(1,1,1,_("Reading package lists"));      
      return true;
   }
   
   if (CacheMap != 0)
   {
      delete CacheMap;
      CacheMap = 0;
   }
   
   if (pkgMakeOnlyStatusCache(Progress,&CacheMap) == false)
      return false;
   Cache->DropProgress();
   
   return true;
}
									/*}}}*/
// DpkgDB::ReadFList - Read the File Listings in 			/*{{{*/
// ---------------------------------------------------------------------
/* This reads the file listing in from the state directory. This is a 
   performance critical routine, as it needs to parse about 50k lines of
   text spread over a hundred or more files. For an initial cold start
   most of the time is spent in reading file inodes and so on, not 
   actually parsing. */
bool debDpkgDB::ReadFList(OpProgress &Progress)
{
   // Count the number of packages we need to read information for
   unsigned long Total = 0;
   pkgCache &Cache = this->Cache->GetCache();
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      // Only not installed packages have no files.
      if (I->CurrentState == pkgCache::State::NotInstalled)
	 continue;
      Total++;
   }

   /* Switch into the admin dir, this prevents useless lookups for the 
      path components */
   string Cwd = SafeGetCWD();
   if (chdir((AdminDir + "info/").c_str()) != 0)
      return _error->Errno("chdir",_("Failed to change to the admin dir %sinfo"),AdminDir.c_str());
   
   // Allocate a buffer. Anything larger than this buffer will be mmaped
   unsigned long BufSize = 32*1024;
   char *Buffer = new char[BufSize];

   // Begin Loading them
   unsigned long Count = 0;
   char Name[300];
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      /* Only not installed packages have no files. ConfFile packages have
         file lists but we don't want to read them in */
      if (I->CurrentState == pkgCache::State::NotInstalled ||
	  I->CurrentState == pkgCache::State::ConfigFiles)
	 continue;

      // Fetch a package handle to associate with the file
      pkgFLCache::PkgIterator FlPkg = FList->GetPkg(I.Name(),0,true);
      if (FlPkg.end() == true)
      {
	 _error->Error(_("Internal error getting a package name"));
	 break;
      }
      
      Progress.OverallProgress(Count,Total,1,_("Reading file listing"));
     
      // Open the list file
      snprintf(Name,sizeof(Name),"%s.list",I.Name());
      int Fd = open(Name,O_RDONLY);
      
      /* Okay this is very strange and bad.. Best thing is to bail and
         instruct the user to look into it. */
      struct stat Stat;
      if (Fd == -1 || fstat(Fd,&Stat) != 0)
      {
	 _error->Errno("open",_("Failed to open the list file '%sinfo/%s'. If you "
		       "cannot restore this file then make it empty "
		       "and immediately re-install the same version of the package!"),
		       AdminDir.c_str(),Name);
	 break;
      }
      
      // Set File to be a memory buffer containing the whole file
      char *File;
      if ((unsigned)Stat.st_size < BufSize)
      {
	 if (read(Fd,Buffer,Stat.st_size) != Stat.st_size)
	 {
	    _error->Errno("read",_("Failed reading the list file %sinfo/%s"),
			  AdminDir.c_str(),Name);
	    close(Fd);
	    break;
	 }
	 File = Buffer;
      }
      else
      {
	 // Use mmap
	 File = (char *)mmap(0,Stat.st_size,PROT_READ,MAP_PRIVATE,Fd,0);
	 if (File == (char *)(-1))
	 {
	    _error->Errno("mmap",_("Failed reading the list file %sinfo/%s"),
			  AdminDir.c_str(),Name);
	    close(Fd);
	    break;
	 }	    
      }
      
      // Parse it
      const char *Start = File;
      const char *End = File;
      const char *Finish = File + Stat.st_size;
      for (; End < Finish; End++)
      {
	 // Not an end of line
	 if (*End != '\n' && End + 1 < Finish)
	    continue;

	 // Skip blank lines
	 if (End - Start > 1)
	 {
	    pkgFLCache::NodeIterator Node = FList->GetNode(Start,End,
 					      FlPkg.Offset(),true,false);
	    if (Node.end() == true)
	    {
	       _error->Error(_("Internal error getting a node"));
	       break;
	    }
	 }
	 
	 // Skip past the end of line
	 for (; *End == '\n' && End < Finish; End++);
	 Start = End;
      }      
      
      close(Fd);
      if ((unsigned)Stat.st_size >= BufSize)
	 munmap((caddr_t)File,Stat.st_size);
      
      // Failed
      if (End < Finish)
	 break;
      
      Count++;
   }

   delete [] Buffer;
   if (chdir(Cwd.c_str()) != 0)
      chdir("/");
   
   return !_error->PendingError();
}
									/*}}}*/
// DpkgDB::ReadDiversions - Load the diversions file			/*{{{*/
// ---------------------------------------------------------------------
/* Read the diversion file in from disk. This is usually invoked by 
   LoadChanges before performing an operation that uses the FLCache. */
bool debDpkgDB::ReadDiversions()
{
   struct stat Stat;
   if (stat((AdminDir + "diversions").c_str(),&Stat) != 0)
      return true;
   
   if (_error->PendingError() == true)
      return false;
   
   FILE *Fd = fopen((AdminDir + "diversions").c_str(),"r");
   if (Fd == 0)
      return _error->Errno("fopen",_("Failed to open the diversions file %sdiversions"),AdminDir.c_str());
	
   FList->BeginDiverLoad();
   while (1)
   {
      char From[300];
      char To[300];
      char Package[100];
   
      // Read the three lines in
      if (fgets(From,sizeof(From),Fd) == 0)
	 break;
      if (fgets(To,sizeof(To),Fd) == 0 ||
	  fgets(Package,sizeof(Package),Fd) == 0)
      {
	 _error->Error(_("The diversion file is corrupted"));
	 break;
      }
      
      // Strip the \ns
      unsigned long Len = strlen(From);
      if (Len < 2 || From[Len-1] != '\n')
	 _error->Error(_("Invalid line in the diversion file: %s"),From);
      else
	 From[Len-1] = 0;
      Len = strlen(To);
      if (Len < 2 || To[Len-1] != '\n')
	 _error->Error(_("Invalid line in the diversion file: %s"),To);
      else
	 To[Len-1] = 0;     
      Len = strlen(Package);
      if (Len < 2 || Package[Len-1] != '\n')
	 _error->Error(_("Invalid line in the diversion file: %s"),Package);
      else
	 Package[Len-1] = 0;
      
      // Make sure the lines were parsed OK
      if (_error->PendingError() == true)
	 break;
      
      // Fetch a package
      if (strcmp(Package,":") == 0)
	 Package[0] = 0;
      pkgFLCache::PkgIterator FlPkg = FList->GetPkg(Package,0,true);
      if (FlPkg.end() == true)
      {
	 _error->Error(_("Internal error getting a package name"));
	 break;
      }
      
      // Install the diversion
      if (FList->AddDiversion(FlPkg,From,To) == false)
      {
	 _error->Error(_("Internal error adding a diversion"));
	 break;
      }
   }
   if (_error->PendingError() == false)
      FList->FinishDiverLoad();
   
   DiverInode = Stat.st_ino;
   DiverTime = Stat.st_mtime;
   
   fclose(Fd);
   return !_error->PendingError();
}
									/*}}}*/
// DpkgDB::ReadFileList - Read the file listing				/*{{{*/
// ---------------------------------------------------------------------
/* Read in the file listing. The file listing is created from three
   sources, *.list, Conffile sections and the Diversion table. */
bool debDpkgDB::ReadyFileList(OpProgress &Progress)
{
   if (Cache == 0)
      return _error->Error(_("The pkg cache must be initialized first"));
   if (FList != 0)
   {
      Progress.OverallProgress(1,1,1,_("Reading file listing"));
      return true;
   }
   
   // Create the cache and read in the file listing
   FileMap = new DynamicMMap(MMap::Public);
   FList = new pkgFLCache(*FileMap);
   if (_error->PendingError() == true || 
       ReadFList(Progress) == false ||
       ReadConfFiles() == false || 
       ReadDiversions() == false)
   {
      delete FList;
      delete FileMap;
      FileMap = 0;
      FList = 0;
      return false;
   }
      
   cout << "Node: " << FList->HeaderP->NodeCount << ',' << FList->HeaderP->UniqNodes << endl;
   cout << "Dir: " << FList->HeaderP->DirCount << endl;
   cout << "Package: " << FList->HeaderP->PackageCount << endl;
   cout << "HashSize: " << FList->HeaderP->HashSize << endl;
   cout << "Size: " << FileMap->Size() << endl;
   cout << endl;

   return true;
}
									/*}}}*/
// DpkgDB::ReadConfFiles - Read the conf file sections from the s-file	/*{{{*/
// ---------------------------------------------------------------------
/* Reading the conf files is done by reparsing the status file. This is
   actually rather fast so it is no big deal. */
bool debDpkgDB::ReadConfFiles()
{
   FileFd File(_config->FindFile("Dir::State::status"),FileFd::ReadOnly);
   pkgTagFile Tags(&File);
   if (_error->PendingError() == true)
      return false;
   
   pkgTagSection Section;   
   while (1)
   {
      // Skip to the next section
      unsigned long Offset = Tags.Offset();
      if (Tags.Step(Section) == false)
	 break;
	 
      // Parse the line
      const char *Start;
      const char *Stop;
      if (Section.Find("Conffiles",Start,Stop) == false)
	 continue;

      const char *PkgStart;
      const char *PkgEnd;
      if (Section.Find("Package",PkgStart,PkgEnd) == false)
	 return _error->Error(_("Failed to find a Package: header, offset %lu"),Offset);

      // Snag a package record for it
      pkgFLCache::PkgIterator FlPkg = FList->GetPkg(PkgStart,PkgEnd,true);
      if (FlPkg.end() == true)
	 return _error->Error(_("Internal error getting a package name"));

      // Parse the conf file lines
      while (1)
      {
	 for (; isspace(*Start) != 0 && Start < Stop; Start++);
	 if (Start == Stop)
	    break;

	 // Split it into words
	 const char *End = Start;
	 for (; isspace(*End) == 0 && End < Stop; End++);
	 const char *StartMd5 = End;
	 for (; isspace(*StartMd5) != 0 && StartMd5 < Stop; StartMd5++);
	 const char *EndMd5 = StartMd5;
	 for (; isspace(*EndMd5) == 0 && EndMd5 < Stop; EndMd5++);
	 if (StartMd5 == EndMd5 || Start == End)
	    return _error->Error(_("Bad ConfFile section in the status file. Offset %lu"),Offset);
	 	 
	 // Insert a new entry
	 unsigned char MD5[16];
	 if (Hex2Num(string(StartMd5,EndMd5-StartMd5),MD5,16) == false)
	    return _error->Error(_("Error parsing MD5. Offset %lu"),Offset);

	 if (FList->AddConfFile(Start,End,FlPkg,MD5) == false)
	    return false;
	 Start = EndMd5;
      }      
   }   
   
   return true;
}
									/*}}}*/
// DpkgDB::LoadChanges - Read in any changed state files		/*{{{*/
// ---------------------------------------------------------------------
/* The only file in the dpkg system that can change while packages are
   unpacking is the diversions file. */
bool debDpkgDB::LoadChanges()
{
   struct stat Stat;
   if (stat((AdminDir + "diversions").c_str(),&Stat) != 0)
      return true;
   if (DiverInode == Stat.st_ino && DiverTime == Stat.st_mtime)
      return true;
   return ReadDiversions();
}
									/*}}}*/

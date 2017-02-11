// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   
   Most of this source is placed in the Public Domain, do with it what 
   you will
   It was originally written by Jason Gunthorpe <jgg@debian.org>.
   FileFd gzip support added by Martin Pitt <martin.pitt@canonical.com>
   
   The exception is RunScripts() it is under the GPLv2

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/select.h>
#include <time.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <glob.h>
#include <pwd.h>
#include <grp.h>

#include <set>
#include <algorithm>
#include <memory>

#ifdef HAVE_ZLIB
	#include <zlib.h>
#endif
#ifdef HAVE_BZ2
	#include <bzlib.h>
#endif
#ifdef HAVE_LZMA
	#include <lzma.h>
#endif
#ifdef HAVE_LZ4
	#include <lz4frame.h>
#endif
#include <endian.h>
#include <stdint.h>

#if __gnu_linux__
#include <sys/prctl.h>
#endif

#include <apti18n.h>
									/*}}}*/

using namespace std;

/* Should be a multiple of the common page size (4096) */
static constexpr unsigned long long APT_BUFFER_SIZE = 64 * 1024;

// RunScripts - Run a set of scripts from a configuration subtree	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RunScripts(const char *Cnf)
{
   Configuration::Item const *Opts = _config->Tree(Cnf);
   if (Opts == 0 || Opts->Child == 0)
      return true;
   Opts = Opts->Child;

   // Fork for running the system calls
   pid_t Child = ExecFork();
   
   // This is the child
   if (Child == 0)
   {
      if (_config->FindDir("DPkg::Chroot-Directory","/") != "/") 
      {
         std::cerr << "Chrooting into " 
                   << _config->FindDir("DPkg::Chroot-Directory") 
                   << std::endl;
         if (chroot(_config->FindDir("DPkg::Chroot-Directory","/").c_str()) != 0)
            _exit(100);
      }

      if (chdir("/tmp/") != 0)
	 _exit(100);
	 
      unsigned int Count = 1;
      for (; Opts != 0; Opts = Opts->Next, Count++)
      {
	 if (Opts->Value.empty() == true)
	    continue;

         if(_config->FindB("Debug::RunScripts", false) == true)
            std::clog << "Running external script: '"
                      << Opts->Value << "'" << std::endl;

	 if (system(Opts->Value.c_str()) != 0)
	    _exit(100+Count);
      }
      _exit(0);
   }      

   // Wait for the child
   int Status = 0;
   while (waitpid(Child,&Status,0) != Child)
   {
      if (errno == EINTR)
	 continue;
      return _error->Errno("waitpid","Couldn't wait for subprocess");
   }

   // Restore sig int/quit
   signal(SIGQUIT,SIG_DFL);
   signal(SIGINT,SIG_DFL);   

   // Check for an error code.
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
   {
      unsigned int Count = WEXITSTATUS(Status);
      if (Count > 100)
      {
	 Count -= 100;
	 for (; Opts != 0 && Count != 1; Opts = Opts->Next, Count--);
	 _error->Error("Problem executing scripts %s '%s'",Cnf,Opts->Value.c_str());
      }
      
      return _error->Error("Sub-process returned an error code");
   }
   
   return true;
}
									/*}}}*/

// CopyFile - Buffered copy of a file					/*{{{*/
// ---------------------------------------------------------------------
/* The caller is expected to set things so that failure causes erasure */
bool CopyFile(FileFd &From,FileFd &To)
{
   if (From.IsOpen() == false || To.IsOpen() == false ||
	 From.Failed() == true || To.Failed() == true)
      return false;

   // Buffered copy between fds
   constexpr size_t BufSize = APT_BUFFER_SIZE;
   std::unique_ptr<unsigned char[]> Buf(new unsigned char[BufSize]);
   unsigned long long ToRead = 0;
   do {
      if (From.Read(Buf.get(),BufSize, &ToRead) == false ||
	  To.Write(Buf.get(),ToRead) == false)
	 return false;
   } while (ToRead != 0);

   return true;
}
									/*}}}*/
bool RemoveFile(char const * const Function, std::string const &FileName)/*{{{*/
{
   if (FileName == "/dev/null")
      return true;
   errno = 0;
   if (unlink(FileName.c_str()) != 0)
   {
      if (errno == ENOENT)
	 return true;

      return _error->WarningE(Function,_("Problem unlinking the file %s"), FileName.c_str());
   }
   return true;
}
									/*}}}*/
// GetLock - Gets a lock file						/*{{{*/
// ---------------------------------------------------------------------
/* This will create an empty file of the given name and lock it. Once this
   is done all other calls to GetLock in any other process will fail with
   -1. The return result is the fd of the file, the call should call
   close at some time. */
int GetLock(string File,bool Errors)
{
   // GetLock() is used in aptitude on directories with public-write access
   // Use O_NOFOLLOW here to prevent symlink traversal attacks
   int FD = open(File.c_str(),O_RDWR | O_CREAT | O_NOFOLLOW,0640);
   if (FD < 0)
   {
      // Read only .. can't have locking problems there.
      if (errno == EROFS)
      {
	 _error->Warning(_("Not using locking for read only lock file %s"),File.c_str());
	 return dup(0);       // Need something for the caller to close
      }
      
      if (Errors == true)
	 _error->Errno("open",_("Could not open lock file %s"),File.c_str());

      // Feh.. We do this to distinguish the lock vs open case..
      errno = EPERM;
      return -1;
   }
   SetCloseExec(FD,true);
      
   // Acquire a write lock
   struct flock fl;
   fl.l_type = F_WRLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start = 0;
   fl.l_len = 0;
   if (fcntl(FD,F_SETLK,&fl) == -1)
   {
      // always close to not leak resources
      int Tmp = errno;
      close(FD);
      errno = Tmp;

      if (errno == ENOLCK)
      {
	 _error->Warning(_("Not using locking for nfs mounted lock file %s"),File.c_str());
	 return dup(0);       // Need something for the caller to close	 
      }
  
      if (Errors == true)
	 _error->Errno("open",_("Could not get lock %s"),File.c_str());
      
      return -1;
   }

   return FD;
}
									/*}}}*/
// FileExists - Check if a file exists					/*{{{*/
// ---------------------------------------------------------------------
/* Beware: Directories are also files! */
bool FileExists(string File)
{
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
      return false;
   return true;
}
									/*}}}*/
// RealFileExists - Check if a file exists and if it is really a file	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RealFileExists(string File)
{
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
      return false;
   return ((Buf.st_mode & S_IFREG) != 0);
}
									/*}}}*/
// DirectoryExists - Check if a directory exists and is really one	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DirectoryExists(string const &Path)
{
   struct stat Buf;
   if (stat(Path.c_str(),&Buf) != 0)
      return false;
   return ((Buf.st_mode & S_IFDIR) != 0);
}
									/*}}}*/
// CreateDirectory - poor man's mkdir -p guarded by a parent directory	/*{{{*/
// ---------------------------------------------------------------------
/* This method will create all directories needed for path in good old
   mkdir -p style but refuses to do this if Parent is not a prefix of
   this Path. Example: /var/cache/ and /var/cache/apt/archives are given,
   so it will create apt/archives if /var/cache exists - on the other
   hand if the parent is /var/lib the creation will fail as this path
   is not a parent of the path to be generated. */
bool CreateDirectory(string const &Parent, string const &Path)
{
   if (Parent.empty() == true || Path.empty() == true)
      return false;

   if (DirectoryExists(Path) == true)
      return true;

   if (DirectoryExists(Parent) == false)
      return false;

   // we are not going to create directories "into the blue"
   if (Path.compare(0, Parent.length(), Parent) != 0)
      return false;

   vector<string> const dirs = VectorizeString(Path.substr(Parent.size()), '/');
   string progress = Parent;
   for (vector<string>::const_iterator d = dirs.begin(); d != dirs.end(); ++d)
   {
      if (d->empty() == true)
	 continue;

      progress.append("/").append(*d);
      if (DirectoryExists(progress) == true)
	 continue;

      if (mkdir(progress.c_str(), 0755) != 0)
	 return false;
   }
   return true;
}
									/*}}}*/
// CreateAPTDirectoryIfNeeded - ensure that the given directory exists		/*{{{*/
// ---------------------------------------------------------------------
/* a small wrapper around CreateDirectory to check if it exists and to
   remove the trailing "/apt/" from the parent directory if needed */
bool CreateAPTDirectoryIfNeeded(string const &Parent, string const &Path)
{
   if (DirectoryExists(Path) == true)
      return true;

   size_t const len = Parent.size();
   if (len > 5 && Parent.find("/apt/", len - 6, 5) == len - 5)
   {
      if (CreateDirectory(Parent.substr(0,len-5), Path) == true)
	 return true;
   }
   else if (CreateDirectory(Parent, Path) == true)
      return true;

   return false;
}
									/*}}}*/
// GetListOfFilesInDir - returns a vector of files in the given dir	/*{{{*/
// ---------------------------------------------------------------------
/* If an extension is given only files with this extension are included
   in the returned vector, otherwise every "normal" file is included. */
std::vector<string> GetListOfFilesInDir(string const &Dir, string const &Ext,
					bool const &SortList, bool const &AllowNoExt)
{
   std::vector<string> ext;
   ext.reserve(2);
   if (Ext.empty() == false)
      ext.push_back(Ext);
   if (AllowNoExt == true && ext.empty() == false)
      ext.push_back("");
   return GetListOfFilesInDir(Dir, ext, SortList);
}
std::vector<string> GetListOfFilesInDir(string const &Dir, std::vector<string> const &Ext,
					bool const &SortList)
{
   // Attention debuggers: need to be set with the environment config file!
   bool const Debug = _config->FindB("Debug::GetListOfFilesInDir", false);
   if (Debug == true)
   {
      std::clog << "Accept in " << Dir << " only files with the following " << Ext.size() << " extensions:" << std::endl;
      if (Ext.empty() == true)
	 std::clog << "\tNO extension" << std::endl;
      else
	 for (std::vector<string>::const_iterator e = Ext.begin();
	      e != Ext.end(); ++e)
	    std::clog << '\t' << (e->empty() == true ? "NO" : *e) << " extension" << std::endl;
   }

   std::vector<string> List;

   if (DirectoryExists(Dir) == false)
   {
      _error->Error(_("List of files can't be created as '%s' is not a directory"), Dir.c_str());
      return List;
   }

   Configuration::MatchAgainstConfig SilentIgnore("Dir::Ignore-Files-Silently");
   DIR *D = opendir(Dir.c_str());
   if (D == 0) 
   {
      _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());
      return List;
   }

   for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D)) 
   {
      // skip "hidden" files
      if (Ent->d_name[0] == '.')
	 continue;

      // Make sure it is a file and not something else
      string const File = flCombine(Dir,Ent->d_name);
#ifdef _DIRENT_HAVE_D_TYPE
      if (Ent->d_type != DT_REG)
#endif
      {
	 if (RealFileExists(File) == false)
	 {
	    // do not show ignoration warnings for directories
	    if (
#ifdef _DIRENT_HAVE_D_TYPE
		Ent->d_type == DT_DIR ||
#endif
		DirectoryExists(File) == true)
	       continue;
	    if (SilentIgnore.Match(Ent->d_name) == false)
	       _error->Notice(_("Ignoring '%s' in directory '%s' as it is not a regular file"), Ent->d_name, Dir.c_str());
	    continue;
	 }
      }

      // check for accepted extension:
      // no extension given -> periods are bad as hell!
      // extensions given -> "" extension allows no extension
      if (Ext.empty() == false)
      {
	 string d_ext = flExtension(Ent->d_name);
	 if (d_ext == Ent->d_name) // no extension
	 {
	    if (std::find(Ext.begin(), Ext.end(), "") == Ext.end())
	    {
	       if (Debug == true)
		  std::clog << "Bad file: " << Ent->d_name << " → no extension" << std::endl;
	       if (SilentIgnore.Match(Ent->d_name) == false)
		  _error->Notice(_("Ignoring file '%s' in directory '%s' as it has no filename extension"), Ent->d_name, Dir.c_str());
	       continue;
	    }
	 }
	 else if (std::find(Ext.begin(), Ext.end(), d_ext) == Ext.end())
	 {
	    if (Debug == true)
	       std::clog << "Bad file: " << Ent->d_name << " → bad extension »" << flExtension(Ent->d_name) << "«" << std::endl;
	    if (SilentIgnore.Match(Ent->d_name) == false)
	       _error->Notice(_("Ignoring file '%s' in directory '%s' as it has an invalid filename extension"), Ent->d_name, Dir.c_str());
	    continue;
	 }
      }

      // Skip bad filenames ala run-parts
      const char *C = Ent->d_name;
      for (; *C != 0; ++C)
	 if (isalpha(*C) == 0 && isdigit(*C) == 0
	     && *C != '_' && *C != '-' && *C != ':') {
	    // no required extension -> dot is a bad character
	    if (*C == '.' && Ext.empty() == false)
	       continue;
	    break;
	 }

      // we don't reach the end of the name -> bad character included
      if (*C != 0)
      {
	 if (Debug == true)
	    std::clog << "Bad file: " << Ent->d_name << " → bad character »"
	       << *C << "« in filename (period allowed: " << (Ext.empty() ? "no" : "yes") << ")" << std::endl;
	 continue;
      }

      // skip filenames which end with a period. These are never valid
      if (*(C - 1) == '.')
      {
	 if (Debug == true)
	    std::clog << "Bad file: " << Ent->d_name << " → Period as last character" << std::endl;
	 continue;
      }

      if (Debug == true)
	 std::clog << "Accept file: " << Ent->d_name << " in " << Dir << std::endl;
      List.push_back(File);
   }
   closedir(D);

   if (SortList == true)
      std::sort(List.begin(),List.end());
   return List;
}
std::vector<string> GetListOfFilesInDir(string const &Dir, bool SortList)
{
   bool const Debug = _config->FindB("Debug::GetListOfFilesInDir", false);
   if (Debug == true)
      std::clog << "Accept in " << Dir << " all regular files" << std::endl;

   std::vector<string> List;

   if (DirectoryExists(Dir) == false)
   {
      _error->Error(_("List of files can't be created as '%s' is not a directory"), Dir.c_str());
      return List;
   }

   DIR *D = opendir(Dir.c_str());
   if (D == 0)
   {
      _error->Errno("opendir",_("Unable to read %s"),Dir.c_str());
      return List;
   }

   for (struct dirent *Ent = readdir(D); Ent != 0; Ent = readdir(D)) 
   {
      // skip "hidden" files
      if (Ent->d_name[0] == '.')
	 continue;

      // Make sure it is a file and not something else
      string const File = flCombine(Dir,Ent->d_name);
#ifdef _DIRENT_HAVE_D_TYPE
      if (Ent->d_type != DT_REG)
#endif
      {
	 if (RealFileExists(File) == false)
	 {
	    if (Debug == true)
	       std::clog << "Bad file: " << Ent->d_name << " → it is not a real file" << std::endl;
	    continue;
	 }
      }

      // Skip bad filenames ala run-parts
      const char *C = Ent->d_name;
      for (; *C != 0; ++C)
	 if (isalpha(*C) == 0 && isdigit(*C) == 0
	     && *C != '_' && *C != '-' && *C != '.')
	    break;

      // we don't reach the end of the name -> bad character included
      if (*C != 0)
      {
	 if (Debug == true)
	    std::clog << "Bad file: " << Ent->d_name << " → bad character »" << *C << "« in filename" << std::endl;
	 continue;
      }

      // skip filenames which end with a period. These are never valid
      if (*(C - 1) == '.')
      {
	 if (Debug == true)
	    std::clog << "Bad file: " << Ent->d_name << " → Period as last character" << std::endl;
	 continue;
      }

      if (Debug == true)
	 std::clog << "Accept file: " << Ent->d_name << " in " << Dir << std::endl;
      List.push_back(File);
   }
   closedir(D);

   if (SortList == true)
      std::sort(List.begin(),List.end());
   return List;
}
									/*}}}*/
// SafeGetCWD - This is a safer getcwd that returns a dynamic string	/*{{{*/
// ---------------------------------------------------------------------
/* We return / on failure. */
string SafeGetCWD()
{
   // Stash the current dir.
   char S[300];
   S[0] = 0;
   if (getcwd(S,sizeof(S)-2) == 0)
      return "/";
   unsigned int Len = strlen(S);
   S[Len] = '/';
   S[Len+1] = 0;
   return S;
}
									/*}}}*/
// GetModificationTime - Get the mtime of the given file or -1 on error /*{{{*/
// ---------------------------------------------------------------------
/* We return / on failure. */
time_t GetModificationTime(string const &Path)
{
   struct stat St;
   if (stat(Path.c_str(), &St) < 0)
      return -1;
   return  St.st_mtime;
}
									/*}}}*/
// flNotDir - Strip the directory from the filename			/*{{{*/
// ---------------------------------------------------------------------
/* */
string flNotDir(string File)
{
   string::size_type Res = File.rfind('/');
   if (Res == string::npos)
      return File;
   Res++;
   return string(File,Res,Res - File.length());
}
									/*}}}*/
// flNotFile - Strip the file from the directory name			/*{{{*/
// ---------------------------------------------------------------------
/* Result ends in a / */
string flNotFile(string File)
{
   string::size_type Res = File.rfind('/');
   if (Res == string::npos)
      return "./";
   Res++;
   return string(File,0,Res);
}
									/*}}}*/
// flExtension - Return the extension for the file			/*{{{*/
// ---------------------------------------------------------------------
/* */
string flExtension(string File)
{
   string::size_type Res = File.rfind('.');
   if (Res == string::npos)
      return File;
   Res++;
   return string(File,Res,Res - File.length());
}
									/*}}}*/
// flNoLink - If file is a symlink then deref it			/*{{{*/
// ---------------------------------------------------------------------
/* If the name is not a link then the returned path is the input. */
string flNoLink(string File)
{
   struct stat St;
   if (lstat(File.c_str(),&St) != 0 || S_ISLNK(St.st_mode) == 0)
      return File;
   if (stat(File.c_str(),&St) != 0)
      return File;
   
   /* Loop resolving the link. There is no need to limit the number of 
      loops because the stat call above ensures that the symlink is not 
      circular */
   char Buffer[1024];
   string NFile = File;
   while (1)
   {
      // Read the link
      ssize_t Res;
      if ((Res = readlink(NFile.c_str(),Buffer,sizeof(Buffer))) <= 0 || 
	  (size_t)Res >= sizeof(Buffer))
	  return File;
      
      // Append or replace the previous path
      Buffer[Res] = 0;
      if (Buffer[0] == '/')
	 NFile = Buffer;
      else
	 NFile = flNotFile(NFile) + Buffer;
      
      // See if we are done
      if (lstat(NFile.c_str(),&St) != 0)
	 return File;
      if (S_ISLNK(St.st_mode) == 0)
	 return NFile;      
   }   
}
									/*}}}*/
// flCombine - Combine a file and a directory				/*{{{*/
// ---------------------------------------------------------------------
/* If the file is an absolute path then it is just returned, otherwise
   the directory is pre-pended to it. */
string flCombine(string Dir,string File)
{
   if (File.empty() == true)
      return string();
   
   if (File[0] == '/' || Dir.empty() == true)
      return File;
   if (File.length() >= 2 && File[0] == '.' && File[1] == '/')
      return File;
   if (Dir[Dir.length()-1] == '/')
      return Dir + File;
   return Dir + '/' + File;
}
									/*}}}*/
// flAbsPath - Return the absolute path of the filename			/*{{{*/
// ---------------------------------------------------------------------
/* */
string flAbsPath(string File)
{
   char *p = realpath(File.c_str(), NULL);
   if (p == NULL)
   {
      _error->Errno("realpath", "flAbsPath on %s failed", File.c_str());
      return "";
   }
   std::string AbsPath(p);
   free(p);
   return AbsPath;
}
									/*}}}*/
std::string flNormalize(std::string file)				/*{{{*/
{
   if (file.empty())
      return file;
   // do some normalisation by removing // and /./ from the path
   size_t found = string::npos;
   while ((found = file.find("/./")) != string::npos)
      file.replace(found, 3, "/");
   while ((found = file.find("//")) != string::npos)
      file.replace(found, 2, "/");

   if (APT::String::Startswith(file, "/dev/null"))
   {
      file.erase(strlen("/dev/null"));
      return file;
   }
   return file;
}
									/*}}}*/
// SetCloseExec - Set the close on exec flag				/*{{{*/
// ---------------------------------------------------------------------
/* */
void SetCloseExec(int Fd,bool Close)
{   
   if (fcntl(Fd,F_SETFD,(Close == false)?0:FD_CLOEXEC) != 0)
   {
      cerr << "FATAL -> Could not set close on exec " << strerror(errno) << endl;
      exit(100);
   }
}
									/*}}}*/
// SetNonBlock - Set the nonblocking flag				/*{{{*/
// ---------------------------------------------------------------------
/* */
void SetNonBlock(int Fd,bool Block)
{   
   int Flags = fcntl(Fd,F_GETFL) & (~O_NONBLOCK);
   if (fcntl(Fd,F_SETFL,Flags | ((Block == false)?0:O_NONBLOCK)) != 0)
   {
      cerr << "FATAL -> Could not set non-blocking flag " << strerror(errno) << endl;
      exit(100);
   }
}
									/*}}}*/
// WaitFd - Wait for a FD to become readable				/*{{{*/
// ---------------------------------------------------------------------
/* This waits for a FD to become readable using select. It is useful for
   applications making use of non-blocking sockets. The timeout is 
   in seconds. */
bool WaitFd(int Fd,bool write,unsigned long timeout)
{
   fd_set Set;
   struct timeval tv;
   FD_ZERO(&Set);
   FD_SET(Fd,&Set);
   tv.tv_sec = timeout;
   tv.tv_usec = 0;
   if (write == true) 
   {      
      int Res;
      do
      {
	 Res = select(Fd+1,0,&Set,0,(timeout != 0?&tv:0));
      }
      while (Res < 0 && errno == EINTR);
      
      if (Res <= 0)
	 return false;
   } 
   else 
   {
      int Res;
      do
      {
	 Res = select(Fd+1,&Set,0,0,(timeout != 0?&tv:0));
      }
      while (Res < 0 && errno == EINTR);
      
      if (Res <= 0)
	 return false;
   }
   
   return true;
}
									/*}}}*/
// MergeKeepFdsFromConfiguration - Merge APT::Keep-Fds configuration	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to merge the APT::Keep-Fds with the provided KeepFDs
 * set.
 */
void MergeKeepFdsFromConfiguration(std::set<int> &KeepFDs)
{
      Configuration::Item const *Opts = _config->Tree("APT::Keep-Fds");
      if (Opts != 0 && Opts->Child != 0)
      {
	 Opts = Opts->Child;
	 for (; Opts != 0; Opts = Opts->Next)
	 {
	    if (Opts->Value.empty() == true)
	       continue;
	    int fd = atoi(Opts->Value.c_str());
	    KeepFDs.insert(fd);
	 }
      }
}
									/*}}}*/
// ExecFork - Magical fork that sanitizes the context before execing	/*{{{*/
// ---------------------------------------------------------------------
/* This is used if you want to cleanse the environment for the forked 
   child, it fixes up the important signals and nukes all of the fds,
   otherwise acts like normal fork. */
pid_t ExecFork()
{
      set<int> KeepFDs;
      // we need to merge the Keep-Fds as external tools like 
      // debconf-apt-progress use it
      MergeKeepFdsFromConfiguration(KeepFDs);
      return ExecFork(KeepFDs);
}

pid_t ExecFork(std::set<int> KeepFDs)
{
   // Fork off the process
   pid_t Process = fork();
   if (Process < 0)
   {
      cerr << "FATAL -> Failed to fork." << endl;
      exit(100);
   }

   // Spawn the subprocess
   if (Process == 0)
   {
      // Setup the signals
      signal(SIGPIPE,SIG_DFL);
      signal(SIGQUIT,SIG_DFL);
      signal(SIGINT,SIG_DFL);
      signal(SIGWINCH,SIG_DFL);
      signal(SIGCONT,SIG_DFL);
      signal(SIGTSTP,SIG_DFL);

      DIR *dir = opendir("/proc/self/fd");
      if (dir != NULL)
      {
	 struct dirent *ent;
	 while ((ent = readdir(dir)))
	 {
	    int fd = atoi(ent->d_name);
	    // If fd > 0, it was a fd number and not . or ..
	    if (fd >= 3 && KeepFDs.find(fd) == KeepFDs.end())
	       fcntl(fd,F_SETFD,FD_CLOEXEC);
	 }
	 closedir(dir);
      } else {
	 long ScOpenMax = sysconf(_SC_OPEN_MAX);
	 // Close all of our FDs - just in case
	 for (int K = 3; K != ScOpenMax; K++)
	 {
	    if(KeepFDs.find(K) == KeepFDs.end())
	       fcntl(K,F_SETFD,FD_CLOEXEC);
	 }
      }
   }
   
   return Process;
}
									/*}}}*/
// ExecWait - Fancy waitpid						/*{{{*/
// ---------------------------------------------------------------------
/* Waits for the given sub process. If Reap is set then no errors are 
   generated. Otherwise a failed subprocess will generate a proper descriptive
   message */
bool ExecWait(pid_t Pid,const char *Name,bool Reap)
{
   if (Pid <= 1)
      return true;
   
   // Wait and collect the error code
   int Status;
   while (waitpid(Pid,&Status,0) != Pid)
   {
      if (errno == EINTR)
	 continue;

      if (Reap == true)
	 return false;
      
      return _error->Error(_("Waited for %s but it wasn't there"),Name);
   }

   
   // Check for an error code.
   if (WIFEXITED(Status) == 0 || WEXITSTATUS(Status) != 0)
   {
      if (Reap == true)
	 return false;
      if (WIFSIGNALED(Status) != 0)
      {
	 if( WTERMSIG(Status) == SIGSEGV)
	    return _error->Error(_("Sub-process %s received a segmentation fault."),Name);
	 else 
	    return _error->Error(_("Sub-process %s received signal %u."),Name, WTERMSIG(Status));
      }

      if (WIFEXITED(Status) != 0)
	 return _error->Error(_("Sub-process %s returned an error code (%u)"),Name,WEXITSTATUS(Status));
      
      return _error->Error(_("Sub-process %s exited unexpectedly"),Name);
   }      
   
   return true;
}
									/*}}}*/
// StartsWithGPGClearTextSignature - Check if a file is Pgp/GPG clearsigned	/*{{{*/
bool StartsWithGPGClearTextSignature(string const &FileName)
{
   static const char* SIGMSG = "-----BEGIN PGP SIGNED MESSAGE-----\n";
   char buffer[strlen(SIGMSG)+1];
   FILE* gpg = fopen(FileName.c_str(), "r");
   if (gpg == NULL)
      return false;

   char const * const test = fgets(buffer, sizeof(buffer), gpg);
   fclose(gpg);
   if (test == NULL || strcmp(buffer, SIGMSG) != 0)
      return false;

   return true;
}
									/*}}}*/
// ChangeOwnerAndPermissionOfFile - set file attributes to requested values /*{{{*/
bool ChangeOwnerAndPermissionOfFile(char const * const requester, char const * const file, char const * const user, char const * const group, mode_t const mode)
{
   if (strcmp(file, "/dev/null") == 0)
      return true;
   bool Res = true;
   if (getuid() == 0 && strlen(user) != 0 && strlen(group) != 0) // if we aren't root, we can't chown, so don't try it
   {
      // ensure the file is owned by root and has good permissions
      struct passwd const * const pw = getpwnam(user);
      struct group const * const gr = getgrnam(group);
      if (pw != NULL && gr != NULL && lchown(file, pw->pw_uid, gr->gr_gid) != 0)
	 Res &= _error->WarningE(requester, "chown to %s:%s of file %s failed", user, group, file);
   }
   struct stat Buf;
   if (lstat(file, &Buf) != 0 || S_ISLNK(Buf.st_mode))
      return Res;
   if (chmod(file, mode) != 0)
      Res &= _error->WarningE(requester, "chmod 0%o of file %s failed", mode, file);
   return Res;
}
									/*}}}*/

struct APT_HIDDEN simple_buffer {							/*{{{*/
   size_t buffersize_max = 0;
   unsigned long long bufferstart = 0;
   unsigned long long bufferend = 0;
   char *buffer = nullptr;

   simple_buffer() {
      reset(4096);
   }
   ~simple_buffer() {
      delete[] buffer;
   }

   const char *get() const { return buffer + bufferstart; }
   char *get() { return buffer + bufferstart; }
   const char *getend() const { return buffer + bufferend; }
   char *getend() { return buffer + bufferend; }
   bool empty() const { return bufferend <= bufferstart; }
   bool full() const { return bufferend == buffersize_max; }
   unsigned long long free() const { return buffersize_max - bufferend; }
   unsigned long long size() const { return bufferend-bufferstart; }
   void reset(size_t size)
   {
      if (size > buffersize_max) {
	 delete[] buffer;
	 buffersize_max = size;
	 buffer = new char[size];
      }
      reset();
   }
   void reset() { bufferend = bufferstart = 0; }
   ssize_t read(void *to, unsigned long long requested_size) APT_MUSTCHECK
   {
      if (size() < requested_size)
	 requested_size = size();
      memcpy(to, buffer + bufferstart, requested_size);
      bufferstart += requested_size;
      if (bufferstart == bufferend)
	 bufferstart = bufferend = 0;
      return requested_size;
   }
   ssize_t write(const void *from, unsigned long long requested_size) APT_MUSTCHECK
   {
      if (free() < requested_size)
	 requested_size = free();
      memcpy(getend(), from, requested_size);
      bufferend += requested_size;
      if (bufferstart == bufferend)
	 bufferstart = bufferend = 0;
      return requested_size;
   }
};
									/*}}}*/

class APT_HIDDEN FileFdPrivate {							/*{{{*/
   friend class BufferedWriteFileFdPrivate;
protected:
   FileFd * const filefd;
   simple_buffer buffer;
   int compressed_fd;
   pid_t compressor_pid;
   bool is_pipe;
   APT::Configuration::Compressor compressor;
   unsigned int openmode;
   unsigned long long seekpos;
public:

   explicit FileFdPrivate(FileFd * const pfilefd) : filefd(pfilefd),
      compressed_fd(-1), compressor_pid(-1), is_pipe(false),
      openmode(0), seekpos(0) {};
   virtual APT::Configuration::Compressor get_compressor() const
   {
      return compressor;
   }
   virtual void set_compressor(APT::Configuration::Compressor const &compressor)
   {
      this->compressor = compressor;
   }
   virtual unsigned int get_openmode() const
   {
      return openmode;
   }
   virtual void set_openmode(unsigned int openmode)
   {
      this->openmode = openmode;
   }
   virtual bool get_is_pipe() const
   {
      return is_pipe;
   }
   virtual void set_is_pipe(bool is_pipe)
   {
      this->is_pipe = is_pipe;
   }
   virtual unsigned long long get_seekpos() const
   {
      return seekpos;
   }
   virtual void set_seekpos(unsigned long long seekpos)
   {
      this->seekpos = seekpos;
   }

   virtual bool InternalOpen(int const iFd, unsigned int const Mode) = 0;
   ssize_t InternalRead(void * To, unsigned long long Size)
   {
      // Drain the buffer if needed.
      if (buffer.empty() == false)
      {
	 return buffer.read(To, Size);
      }
      return InternalUnbufferedRead(To, Size);
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) = 0;
   virtual bool InternalReadError() { return filefd->FileFdErrno("read",_("Read error")); }
   virtual char * InternalReadLine(char * To, unsigned long long Size)
   {
      if (unlikely(Size == 0))
	 return nullptr;
      // Read one byte less than buffer size to have space for trailing 0.
      --Size;

      char * const InitialTo = To;

      while (Size > 0) {
	 if (buffer.empty() == true)
	 {
	    buffer.reset();
	    unsigned long long actualread = 0;
	    if (filefd->Read(buffer.getend(), buffer.free(), &actualread) == false)
	       return nullptr;
	    buffer.bufferend = actualread;
	    if (buffer.size() == 0)
	    {
	       if (To == InitialTo)
		  return nullptr;
	       break;
	    }
	    filefd->Flags &= ~FileFd::HitEof;
	 }

	 unsigned long long const OutputSize = std::min(Size, buffer.size());
	 char const * const newline = static_cast<char const * const>(memchr(buffer.get(), '\n', OutputSize));
	 // Read until end of line or up to Size bytes from the buffer.
	 unsigned long long actualread = buffer.read(To,
						     (newline != nullptr)
						     ? (newline - buffer.get()) + 1
						     : OutputSize);
	 To += actualread;
	 Size -= actualread;
	 if (newline != nullptr)
	    break;
      }
      *To = '\0';
      return InitialTo;
   }
   virtual bool InternalFlush()
   {
      return true;
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) = 0;
   virtual bool InternalWriteError() { return filefd->FileFdErrno("write",_("Write error")); }
   virtual bool InternalSeek(unsigned long long const To)
   {
      // Our poor man seeking is costly, so try to avoid it
      unsigned long long const iseekpos = filefd->Tell();
      if (iseekpos == To)
	 return true;
      else if (iseekpos < To)
	 return filefd->Skip(To - iseekpos);

      if ((openmode & FileFd::ReadOnly) != FileFd::ReadOnly)
	 return filefd->FileFdError("Reopen is only implemented for read-only files!");
      InternalClose(filefd->FileName);
      if (filefd->iFd != -1)
	 close(filefd->iFd);
      filefd->iFd = -1;
      if (filefd->TemporaryFileName.empty() == false)
	 filefd->iFd = open(filefd->TemporaryFileName.c_str(), O_RDONLY);
      else if (filefd->FileName.empty() == false)
	 filefd->iFd = open(filefd->FileName.c_str(), O_RDONLY);
      else
      {
	 if (compressed_fd > 0)
	    if (lseek(compressed_fd, 0, SEEK_SET) != 0)
	       filefd->iFd = compressed_fd;
	 if (filefd->iFd < 0)
	    return filefd->FileFdError("Reopen is not implemented for pipes opened with FileFd::OpenDescriptor()!");
      }

      if (filefd->OpenInternDescriptor(openmode, compressor) == false)
	 return filefd->FileFdError("Seek on file %s because it couldn't be reopened", filefd->FileName.c_str());

      buffer.reset();
      set_seekpos(0);
      if (To != 0)
	 return filefd->Skip(To);

      seekpos = To;
      return true;
   }
   virtual bool InternalSkip(unsigned long long Over)
   {
      unsigned long long constexpr buffersize = 1024;
      char buffer[buffersize];
      while (Over != 0)
      {
	 unsigned long long toread = std::min(buffersize, Over);
	 if (filefd->Read(buffer, toread) == false)
	    return filefd->FileFdError("Unable to seek ahead %llu",Over);
	 Over -= toread;
      }
      return true;
   }
   virtual bool InternalTruncate(unsigned long long const)
   {
      return filefd->FileFdError("Truncating compressed files is not implemented (%s)", filefd->FileName.c_str());
   }
   virtual unsigned long long InternalTell()
   {
      // In theory, we could just return seekpos here always instead of
      // seeking around, but not all users of FileFd use always Seek() and co
      // so d->seekpos isn't always true and we can just use it as a hint if
      // we have nothing else, but not always as an authority…
      return seekpos - buffer.size();
   }
   virtual unsigned long long InternalSize()
   {
      unsigned long long size = 0;
      unsigned long long const oldSeek = filefd->Tell();
      unsigned long long constexpr ignoresize = 1024;
      char ignore[ignoresize];
      unsigned long long read = 0;
      do {
	 if (filefd->Read(ignore, ignoresize, &read) == false)
	 {
	    filefd->Seek(oldSeek);
	    return 0;
	 }
      } while(read != 0);
      size = filefd->Tell();
      filefd->Seek(oldSeek);
      return size;
   }
   virtual bool InternalClose(std::string const &FileName) = 0;
   virtual bool InternalStream() const { return false; }
   virtual bool InternalAlwaysAutoClose() const { return true; }

   virtual ~FileFdPrivate() {}
};
									/*}}}*/
class APT_HIDDEN BufferedWriteFileFdPrivate : public FileFdPrivate {	/*{{{*/
protected:
   FileFdPrivate *wrapped;
   simple_buffer writebuffer;

public:

   explicit BufferedWriteFileFdPrivate(FileFdPrivate *Priv) :
      FileFdPrivate(Priv->filefd), wrapped(Priv) {};

   virtual APT::Configuration::Compressor get_compressor() const APT_OVERRIDE
   {
      return wrapped->get_compressor();
   }
   virtual void set_compressor(APT::Configuration::Compressor const &compressor)  APT_OVERRIDE
   {
      return wrapped->set_compressor(compressor);
   }
   virtual unsigned int get_openmode() const  APT_OVERRIDE
   {
      return wrapped->get_openmode();
   }
   virtual void set_openmode(unsigned int openmode)  APT_OVERRIDE
   {
      return wrapped->set_openmode(openmode);
   }
   virtual bool get_is_pipe() const  APT_OVERRIDE
   {
      return wrapped->get_is_pipe();
   }
   virtual void set_is_pipe(bool is_pipe) APT_OVERRIDE
   {
      FileFdPrivate::set_is_pipe(is_pipe);
      wrapped->set_is_pipe(is_pipe);
   }
   virtual unsigned long long get_seekpos() const APT_OVERRIDE
   {
      return wrapped->get_seekpos();
   }
   virtual void set_seekpos(unsigned long long seekpos) APT_OVERRIDE
   {
      return wrapped->set_seekpos(seekpos);
   }
   virtual bool InternalOpen(int const iFd, unsigned int const Mode) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return false;
      return wrapped->InternalOpen(iFd, Mode);
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return -1;
      return wrapped->InternalUnbufferedRead(To, Size);

   }
   virtual bool InternalReadError() APT_OVERRIDE
   {
      return wrapped->InternalReadError();
   }
   virtual char * InternalReadLine(char * To, unsigned long long Size) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return nullptr;
      return wrapped->InternalReadLine(To, Size);
   }
   virtual bool InternalFlush() APT_OVERRIDE
   {
      while (writebuffer.empty() == false) {
	 auto written = wrapped->InternalWrite(writebuffer.get(),
					       writebuffer.size());
	 // Ignore interrupted syscalls
	 if (written < 0 && errno == EINTR)
	    continue;
	 if (written < 0)
	    return wrapped->InternalWriteError();

	 writebuffer.bufferstart += written;
      }
      writebuffer.reset();
      return wrapped->InternalFlush();
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      // Optimisation: If the buffer is empty and we have more to write than
      // would fit in the buffer (or equal number of bytes), write directly.
      if (writebuffer.empty() == true && Size >= writebuffer.free())
	 return wrapped->InternalWrite(From, Size);

      // Write as much into the buffer as possible and then flush if needed
      auto written = writebuffer.write(From, Size);

      if (writebuffer.full() && InternalFlush() == false)
	 return -1;

      return written;
   }
   virtual bool InternalWriteError() APT_OVERRIDE
   {
      return wrapped->InternalWriteError();
   }
   virtual bool InternalSeek(unsigned long long const To) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return false;
      return wrapped->InternalSeek(To);
   }
   virtual bool InternalSkip(unsigned long long Over) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return false;
      return wrapped->InternalSkip(Over);
   }
   virtual bool InternalTruncate(unsigned long long const Size) APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return false;
      return wrapped->InternalTruncate(Size);
   }
   virtual unsigned long long InternalTell() APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return -1;
      return wrapped->InternalTell();
   }
   virtual unsigned long long InternalSize() APT_OVERRIDE
   {
      if (InternalFlush() == false)
	 return -1;
      return wrapped->InternalSize();
   }
   virtual bool InternalClose(std::string const &FileName) APT_OVERRIDE
   {
      return wrapped->InternalClose(FileName);
   }
   virtual bool InternalAlwaysAutoClose() const APT_OVERRIDE
   {
      return wrapped->InternalAlwaysAutoClose();
   }
   virtual ~BufferedWriteFileFdPrivate()
   {
      delete wrapped;
   }
};
									/*}}}*/
class APT_HIDDEN GzipFileFdPrivate: public FileFdPrivate {				/*{{{*/
#ifdef HAVE_ZLIB
public:
   gzFile gz;
   virtual bool InternalOpen(int const iFd, unsigned int const Mode) APT_OVERRIDE
   {
      if ((Mode & FileFd::ReadWrite) == FileFd::ReadWrite)
	 gz = gzdopen(iFd, "r+");
      else if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly)
	 gz = gzdopen(iFd, "w");
      else
	 gz = gzdopen(iFd, "r");
      filefd->Flags |= FileFd::Compressed;
      return gz != nullptr;
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      return gzread(gz, To, Size);
   }
   virtual bool InternalReadError() APT_OVERRIDE
   {
      int err;
      char const * const errmsg = gzerror(gz, &err);
      if (err != Z_ERRNO)
	 return filefd->FileFdError("gzread: %s (%d: %s)", _("Read error"), err, errmsg);
      return FileFdPrivate::InternalReadError();
   }
   virtual char * InternalReadLine(char * To, unsigned long long Size) APT_OVERRIDE
   {
      return gzgets(gz, To, Size);
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      return gzwrite(gz,From,Size);
   }
   virtual bool InternalWriteError() APT_OVERRIDE
   {
      int err;
      char const * const errmsg = gzerror(gz, &err);
      if (err != Z_ERRNO)
	 return filefd->FileFdError("gzwrite: %s (%d: %s)", _("Write error"), err, errmsg);
      return FileFdPrivate::InternalWriteError();
   }
   virtual bool InternalSeek(unsigned long long const To) APT_OVERRIDE
   {
      off_t const res = gzseek(gz, To, SEEK_SET);
      if (res != (off_t)To)
	 return filefd->FileFdError("Unable to seek to %llu", To);
      seekpos = To;
      buffer.reset();
      return true;
   }
   virtual bool InternalSkip(unsigned long long Over) APT_OVERRIDE
   {
      if (Over >= buffer.size())
      {
	 Over -= buffer.size();
	 buffer.reset();
      }
      else
      {
	 buffer.bufferstart += Over;
	 return true;
      }
      if (Over == 0)
	 return true;
      off_t const res = gzseek(gz, Over, SEEK_CUR);
      if (res < 0)
	 return filefd->FileFdError("Unable to seek ahead %llu",Over);
      seekpos = res;
      return true;
   }
   virtual unsigned long long InternalTell() APT_OVERRIDE
   {
      return gztell(gz) - buffer.size();
   }
   virtual unsigned long long InternalSize() APT_OVERRIDE
   {
      unsigned long long filesize = FileFdPrivate::InternalSize();
      // only check gzsize if we are actually a gzip file, just checking for
      // "gz" is not sufficient as uncompressed files could be opened with
      // gzopen in "direct" mode as well
      if (filesize == 0 || gzdirect(gz))
	 return filesize;

      off_t const oldPos = lseek(filefd->iFd, 0, SEEK_CUR);
      /* unfortunately zlib.h doesn't provide a gzsize(), so we have to do
       * this ourselves; the original (uncompressed) file size is the last 32
       * bits of the file */
      // FIXME: Size for gz-files is limited by 32bit… no largefile support
      if (lseek(filefd->iFd, -4, SEEK_END) < 0)
      {
	 filefd->FileFdErrno("lseek","Unable to seek to end of gzipped file");
	 return 0;
      }
      uint32_t size = 0;
      if (read(filefd->iFd, &size, 4) != 4)
      {
	 filefd->FileFdErrno("read","Unable to read original size of gzipped file");
	 return 0;
      }
      size = le32toh(size);

      if (lseek(filefd->iFd, oldPos, SEEK_SET) < 0)
      {
	 filefd->FileFdErrno("lseek","Unable to seek in gzipped file");
	 return 0;
      }
      return size;
   }
   virtual bool InternalClose(std::string const &FileName) APT_OVERRIDE
   {
      if (gz == nullptr)
	 return true;
      int const e = gzclose(gz);
      gz = nullptr;
      // gzdclose() on empty files always fails with "buffer error" here, ignore that
      if (e != 0 && e != Z_BUF_ERROR)
	 return _error->Errno("close",_("Problem closing the gzip file %s"), FileName.c_str());
      return true;
   }

   explicit GzipFileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd), gz(nullptr) {}
   virtual ~GzipFileFdPrivate() { InternalClose(""); }
#endif
};
									/*}}}*/
class APT_HIDDEN Bz2FileFdPrivate: public FileFdPrivate {				/*{{{*/
#ifdef HAVE_BZ2
   BZFILE* bz2;
public:
   virtual bool InternalOpen(int const iFd, unsigned int const Mode) APT_OVERRIDE
   {
      if ((Mode & FileFd::ReadWrite) == FileFd::ReadWrite)
	 bz2 = BZ2_bzdopen(iFd, "r+");
      else if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly)
	 bz2 = BZ2_bzdopen(iFd, "w");
      else
	 bz2 = BZ2_bzdopen(iFd, "r");
      filefd->Flags |= FileFd::Compressed;
      return bz2 != nullptr;
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      return BZ2_bzread(bz2, To, Size);
   }
   virtual bool InternalReadError() APT_OVERRIDE
   {
      int err;
      char const * const errmsg = BZ2_bzerror(bz2, &err);
      if (err != BZ_IO_ERROR)
	 return filefd->FileFdError("BZ2_bzread: %s %s (%d: %s)", filefd->FileName.c_str(), _("Read error"), err, errmsg);
      return FileFdPrivate::InternalReadError();
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      return BZ2_bzwrite(bz2, (void*)From, Size);
   }
   virtual bool InternalWriteError() APT_OVERRIDE
   {
      int err;
      char const * const errmsg = BZ2_bzerror(bz2, &err);
      if (err != BZ_IO_ERROR)
	 return filefd->FileFdError("BZ2_bzwrite: %s %s (%d: %s)", filefd->FileName.c_str(), _("Write error"), err, errmsg);
      return FileFdPrivate::InternalWriteError();
   }
   virtual bool InternalStream() const APT_OVERRIDE { return true; }
   virtual bool InternalClose(std::string const &) APT_OVERRIDE
   {
      if (bz2 == nullptr)
	 return true;
      BZ2_bzclose(bz2);
      bz2 = nullptr;
      return true;
   }

   explicit Bz2FileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd), bz2(nullptr) {}
   virtual ~Bz2FileFdPrivate() { InternalClose(""); }
#endif
};
									/*}}}*/
class APT_HIDDEN Lz4FileFdPrivate: public FileFdPrivate {				/*{{{*/
   static constexpr unsigned long long LZ4_HEADER_SIZE = 19;
   static constexpr unsigned long long LZ4_FOOTER_SIZE = 4;
#ifdef HAVE_LZ4
   LZ4F_decompressionContext_t dctx;
   LZ4F_compressionContext_t cctx;
   LZ4F_errorCode_t res;
   FileFd backend;
   simple_buffer lz4_buffer;
   // Count of bytes that the decompressor expects to read next, or buffer size.
   size_t next_to_load = APT_BUFFER_SIZE;
public:
   virtual bool InternalOpen(int const iFd, unsigned int const Mode) APT_OVERRIDE
   {
      if ((Mode & FileFd::ReadWrite) == FileFd::ReadWrite)
	 return _error->Error("lz4 only supports write or read mode");

      if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly) {
	 res = LZ4F_createCompressionContext(&cctx, LZ4F_VERSION);
	 lz4_buffer.reset(LZ4F_compressBound(APT_BUFFER_SIZE, nullptr)
			  + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE);
      } else {
	 res = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
	 lz4_buffer.reset(APT_BUFFER_SIZE);
      }

      filefd->Flags |= FileFd::Compressed;

      if (LZ4F_isError(res))
	 return false;

      unsigned int flags = (Mode & (FileFd::WriteOnly|FileFd::ReadOnly));
      if (backend.OpenDescriptor(iFd, flags, FileFd::None, true) == false)
	 return false;

      // Write the file header
      if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly)
      {
	 res = LZ4F_compressBegin(cctx, lz4_buffer.buffer, lz4_buffer.buffersize_max, nullptr);
	 if (LZ4F_isError(res) || backend.Write(lz4_buffer.buffer, res) == false)
	    return false;
      }

      return true;
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      /* Keep reading as long as the compressor still wants to read */
      while (next_to_load) {
	 // Fill compressed buffer;
	 if (lz4_buffer.empty()) {
	    unsigned long long read;
	    /* Reset - if LZ4 decompressor wants to read more, allocate more */
	    lz4_buffer.reset(next_to_load);
	    if (backend.Read(lz4_buffer.getend(), lz4_buffer.free(), &read) == false)
	       return -1;
	    lz4_buffer.bufferend += read;

	    /* Expected EOF */
	    if (read == 0) {
	       res = -1;
	       return filefd->FileFdError("LZ4F: %s %s",
					  filefd->FileName.c_str(),
					  _("Unexpected end of file")), -1;
	    }
	 }
	 // Drain compressed buffer as far as possible.
	 size_t in = lz4_buffer.size();
	 size_t out = Size;

	 res = LZ4F_decompress(dctx, To, &out, lz4_buffer.get(), &in, nullptr);
	 if (LZ4F_isError(res))
	       return -1;

	 next_to_load = res;
	 lz4_buffer.bufferstart += in;

	 if (out != 0)
	    return out;
      }

      return 0;
   }
   virtual bool InternalReadError() APT_OVERRIDE
   {
      char const * const errmsg = LZ4F_getErrorName(res);

      return filefd->FileFdError("LZ4F: %s %s (%zu: %s)", filefd->FileName.c_str(), _("Read error"), res, errmsg);
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      unsigned long long const towrite = std::min(APT_BUFFER_SIZE, Size);

      res = LZ4F_compressUpdate(cctx,
				lz4_buffer.buffer, lz4_buffer.buffersize_max,
				From, towrite, nullptr);

      if (LZ4F_isError(res) || backend.Write(lz4_buffer.buffer, res) == false)
	 return -1;

      return towrite;
   }
   virtual bool InternalWriteError() APT_OVERRIDE
   {
      char const * const errmsg = LZ4F_getErrorName(res);

      return filefd->FileFdError("LZ4F: %s %s (%zu: %s)", filefd->FileName.c_str(), _("Write error"), res, errmsg);
   }
   virtual bool InternalStream() const APT_OVERRIDE { return true; }

   virtual bool InternalFlush() APT_OVERRIDE
   {
      return backend.Flush();
   }

   virtual bool InternalClose(std::string const &) APT_OVERRIDE
   {
      /* Reset variables */
      res = 0;
      next_to_load = APT_BUFFER_SIZE;

      if (cctx != nullptr)
      {
	 if (filefd->Failed() == false)
	 {
	    res = LZ4F_compressEnd(cctx, lz4_buffer.buffer, lz4_buffer.buffersize_max, nullptr);
	    if (LZ4F_isError(res) || backend.Write(lz4_buffer.buffer, res) == false)
	       return false;
	    if (!backend.Flush())
	       return false;
	 }
	 if (!backend.Close())
	    return false;

	 res = LZ4F_freeCompressionContext(cctx);
	 cctx = nullptr;
      }

      if (dctx != nullptr)
      {
	 res = LZ4F_freeDecompressionContext(dctx);
	 dctx = nullptr;
      }
      if (backend.IsOpen())
      {
	 backend.Close();
	 filefd->iFd = -1;
      }

      return LZ4F_isError(res) == false;
   }

   explicit Lz4FileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd), dctx(nullptr), cctx(nullptr) {}
   virtual ~Lz4FileFdPrivate() {
      InternalClose("");
   }
#endif
};
									/*}}}*/
class APT_HIDDEN LzmaFileFdPrivate: public FileFdPrivate {				/*{{{*/
#ifdef HAVE_LZMA
   struct LZMAFILE {
      FILE* file;
      FileFd * const filefd;
      uint8_t buffer[4096];
      lzma_stream stream;
      lzma_ret err;
      bool eof;
      bool compressing;

      LZMAFILE(FileFd * const fd) : file(nullptr), filefd(fd), eof(false), compressing(false) { buffer[0] = '\0'; }
      ~LZMAFILE()
      {
	 if (compressing == true && filefd->Failed() == false)
	 {
	    size_t constexpr buffersize = sizeof(buffer)/sizeof(buffer[0]);
	    while(true)
	    {
	       stream.avail_out = buffersize;
	       stream.next_out = buffer;
	       err = lzma_code(&stream, LZMA_FINISH);
	       if (err != LZMA_OK && err != LZMA_STREAM_END)
	       {
		  _error->Error("~LZMAFILE: Compress finalisation failed");
		  break;
	       }
	       size_t const n =  buffersize - stream.avail_out;
	       if (n && fwrite(buffer, 1, n, file) != n)
	       {
		  _error->Errno("~LZMAFILE",_("Write error"));
		  break;
	       }
	       if (err == LZMA_STREAM_END)
		  break;
	    }
	 }
	 lzma_end(&stream);
	 fclose(file);
      }
   };
   LZMAFILE* lzma;
   static uint32_t findXZlevel(std::vector<std::string> const &Args)
   {
      for (auto a = Args.rbegin(); a != Args.rend(); ++a)
	 if (a->empty() == false && (*a)[0] == '-' && (*a)[1] != '-')
	 {
	    auto const number = a->find_last_of("0123456789");
	    if (number == std::string::npos)
	       continue;
	    auto const extreme = a->find("e", number);
	    uint32_t level = (extreme != std::string::npos) ? LZMA_PRESET_EXTREME : 0;
	    switch ((*a)[number])
	    {
	       case '0': return level | 0;
	       case '1': return level | 1;
	       case '2': return level | 2;
	       case '3': return level | 3;
	       case '4': return level | 4;
	       case '5': return level | 5;
	       case '6': return level | 6;
	       case '7': return level | 7;
	       case '8': return level | 8;
	       case '9': return level | 9;
	    }
	 }
      return 6;
   }
public:
   virtual bool InternalOpen(int const iFd, unsigned int const Mode) APT_OVERRIDE
   {
      if ((Mode & FileFd::ReadWrite) == FileFd::ReadWrite)
	 return filefd->FileFdError("ReadWrite mode is not supported for lzma/xz files %s", filefd->FileName.c_str());

      if (lzma == nullptr)
	 lzma = new LzmaFileFdPrivate::LZMAFILE(filefd);
      if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly)
	 lzma->file = fdopen(iFd, "w");
      else
	 lzma->file = fdopen(iFd, "r");
      filefd->Flags |= FileFd::Compressed;
      if (lzma->file == nullptr)
	 return false;

      lzma_stream tmp_stream = LZMA_STREAM_INIT;
      lzma->stream = tmp_stream;

      if ((Mode & FileFd::WriteOnly) == FileFd::WriteOnly)
      {
	 uint32_t const xzlevel = findXZlevel(compressor.CompressArgs);
	 if (compressor.Name == "xz")
	 {
	    if (lzma_easy_encoder(&lzma->stream, xzlevel, LZMA_CHECK_CRC64) != LZMA_OK)
	       return false;
	 }
	 else
	 {
	    lzma_options_lzma options;
	    lzma_lzma_preset(&options, xzlevel);
	    if (lzma_alone_encoder(&lzma->stream, &options) != LZMA_OK)
	       return false;
	 }
	 lzma->compressing = true;
      }
      else
      {
	 uint64_t const memlimit = UINT64_MAX;
	 if (compressor.Name == "xz")
	 {
	    if (lzma_auto_decoder(&lzma->stream, memlimit, 0) != LZMA_OK)
	       return false;
	 }
	 else
	 {
	    if (lzma_alone_decoder(&lzma->stream, memlimit) != LZMA_OK)
	       return false;
	 }
	 lzma->compressing = false;
      }
      return true;
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      ssize_t Res;
      if (lzma->eof == true)
	 return 0;

      lzma->stream.next_out = (uint8_t *) To;
      lzma->stream.avail_out = Size;
      if (lzma->stream.avail_in == 0)
      {
	 lzma->stream.next_in = lzma->buffer;
	 lzma->stream.avail_in = fread(lzma->buffer, 1, sizeof(lzma->buffer)/sizeof(lzma->buffer[0]), lzma->file);
      }
      lzma->err = lzma_code(&lzma->stream, LZMA_RUN);
      if (lzma->err == LZMA_STREAM_END)
      {
	 lzma->eof = true;
	 Res = Size - lzma->stream.avail_out;
      }
      else if (lzma->err != LZMA_OK)
      {
	 Res = -1;
	 errno = 0;
      }
      else
      {
	 Res = Size - lzma->stream.avail_out;
	 if (Res == 0)
	 {
	    // lzma run was okay, but produced no output…
	    Res = -1;
	    errno = EINTR;
	 }
      }
      return Res;
   }
   virtual bool InternalReadError() APT_OVERRIDE
   {
      return filefd->FileFdError("lzma_read: %s (%d)", _("Read error"), lzma->err);
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      ssize_t Res;
      lzma->stream.next_in = (uint8_t *)From;
      lzma->stream.avail_in = Size;
      lzma->stream.next_out = lzma->buffer;
      lzma->stream.avail_out = sizeof(lzma->buffer)/sizeof(lzma->buffer[0]);
      lzma->err = lzma_code(&lzma->stream, LZMA_RUN);
      if (lzma->err != LZMA_OK)
	 return -1;
      size_t const n = sizeof(lzma->buffer)/sizeof(lzma->buffer[0]) - lzma->stream.avail_out;
      size_t const m = (n == 0) ? 0 : fwrite(lzma->buffer, 1, n, lzma->file);
      if (m != n)
      {
	 Res = -1;
	 errno = 0;
      }
      else
      {
	 Res = Size - lzma->stream.avail_in;
	 if (Res == 0)
	 {
	    // lzma run was okay, but produced no output…
	    Res = -1;
	    errno = EINTR;
	 }
      }
      return Res;
   }
   virtual bool InternalWriteError() APT_OVERRIDE
   {
      return filefd->FileFdError("lzma_write: %s (%d)", _("Write error"), lzma->err);
   }
   virtual bool InternalStream() const APT_OVERRIDE { return true; }
   virtual bool InternalClose(std::string const &) APT_OVERRIDE
   {
      delete lzma;
      lzma = nullptr;
      return true;
   }

   explicit LzmaFileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd), lzma(nullptr) {}
   virtual ~LzmaFileFdPrivate() { InternalClose(""); }
#endif
};
									/*}}}*/
class APT_HIDDEN PipedFileFdPrivate: public FileFdPrivate				/*{{{*/
/* if we don't have a specific class dealing with library calls, we (un)compress
   by executing a specified binary and pipe in/out what we need */
{
public:
   virtual bool InternalOpen(int const, unsigned int const Mode) APT_OVERRIDE
   {
      // collect zombies here in case we reopen
      if (compressor_pid > 0)
	 ExecWait(compressor_pid, "FileFdCompressor", true);

      if ((Mode & FileFd::ReadWrite) == FileFd::ReadWrite)
	 return filefd->FileFdError("ReadWrite mode is not supported for file %s", filefd->FileName.c_str());
      if (compressor.Binary == "false")
	 return filefd->FileFdError("libapt has inbuilt support for the %s compression,"
	       " but was forced to ignore it in favor of an external binary – which isn't installed.", compressor.Name.c_str());

      bool const Comp = (Mode & FileFd::WriteOnly) == FileFd::WriteOnly;
      if (Comp == false && filefd->iFd != -1)
      {
	 // Handle 'decompression' of empty files
	 struct stat Buf;
	 if (fstat(filefd->iFd, &Buf) != 0)
	    return filefd->FileFdErrno("fstat", "Could not stat fd %d for file %s", filefd->iFd, filefd->FileName.c_str());
	 if (Buf.st_size == 0 && S_ISFIFO(Buf.st_mode) == false)
	    return true;

	 // We don't need the file open - instead let the compressor open it
	 // as he properly knows better how to efficiently read from 'his' file
	 if (filefd->FileName.empty() == false)
	 {
	    close(filefd->iFd);
	    filefd->iFd = -1;
	 }
      }

      // Create a data pipe
      int Pipe[2] = {-1,-1};
      if (pipe(Pipe) != 0)
	 return filefd->FileFdErrno("pipe",_("Failed to create subprocess IPC"));
      for (int J = 0; J != 2; J++)
	 SetCloseExec(Pipe[J],true);

      compressed_fd = filefd->iFd;
      set_is_pipe(true);

      if (Comp == true)
	 filefd->iFd = Pipe[1];
      else
	 filefd->iFd = Pipe[0];

      // The child..
      compressor_pid = ExecFork();
      if (compressor_pid == 0)
      {
	 if (Comp == true)
	 {
	    dup2(compressed_fd,STDOUT_FILENO);
	    dup2(Pipe[0],STDIN_FILENO);
	 }
	 else
	 {
	    if (compressed_fd != -1)
	       dup2(compressed_fd,STDIN_FILENO);
	    dup2(Pipe[1],STDOUT_FILENO);
	 }
	 int const nullfd = open("/dev/null", O_WRONLY);
	 if (nullfd != -1)
	 {
	    dup2(nullfd,STDERR_FILENO);
	    close(nullfd);
	 }

	 SetCloseExec(STDOUT_FILENO,false);
	 SetCloseExec(STDIN_FILENO,false);

	 std::vector<char const*> Args;
	 Args.push_back(compressor.Binary.c_str());
	 std::vector<std::string> const * const addArgs =
	    (Comp == true) ? &(compressor.CompressArgs) : &(compressor.UncompressArgs);
	 for (std::vector<std::string>::const_iterator a = addArgs->begin();
	       a != addArgs->end(); ++a)
	    Args.push_back(a->c_str());
	 if (Comp == false && filefd->FileName.empty() == false)
	 {
	    // commands not needing arguments, do not need to be told about using standard output
	    // in reality, only testcases with tools like cat, rev, rot13, … are able to trigger this
	    if (compressor.CompressArgs.empty() == false && compressor.UncompressArgs.empty() == false)
	       Args.push_back("--stdout");
	    if (filefd->TemporaryFileName.empty() == false)
	       Args.push_back(filefd->TemporaryFileName.c_str());
	    else
	       Args.push_back(filefd->FileName.c_str());
	 }
	 Args.push_back(NULL);

	 execvp(Args[0],(char **)&Args[0]);
	 cerr << _("Failed to exec compressor ") << Args[0] << endl;
	 _exit(100);
      }
      if (Comp == true)
	 close(Pipe[0]);
      else
	 close(Pipe[1]);

      return true;
   }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      return read(filefd->iFd, To, Size);
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      return write(filefd->iFd, From, Size);
   }
   virtual bool InternalClose(std::string const &) APT_OVERRIDE
   {
      bool Ret = true;
      if (filefd->iFd != -1)
      {
	 close(filefd->iFd);
	 filefd->iFd = -1;
      }
      if (compressor_pid > 0)
	 Ret &= ExecWait(compressor_pid, "FileFdCompressor", true);
      compressor_pid = -1;
      return Ret;
   }
   explicit PipedFileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd) {}
   virtual ~PipedFileFdPrivate() { InternalClose(""); }
};
									/*}}}*/
class APT_HIDDEN DirectFileFdPrivate: public FileFdPrivate				/*{{{*/
{
public:
   virtual bool InternalOpen(int const, unsigned int const) APT_OVERRIDE { return true; }
   virtual ssize_t InternalUnbufferedRead(void * const To, unsigned long long const Size) APT_OVERRIDE
   {
      return read(filefd->iFd, To, Size);
   }
   virtual ssize_t InternalWrite(void const * const From, unsigned long long const Size) APT_OVERRIDE
   {
      // files opened read+write are strange and only really "supported" for direct files
      if (buffer.size() != 0)
      {
	 lseek(filefd->iFd, -buffer.size(), SEEK_CUR);
	 buffer.reset();
      }
      return write(filefd->iFd, From, Size);
   }
   virtual bool InternalSeek(unsigned long long const To) APT_OVERRIDE
   {
      off_t const res = lseek(filefd->iFd, To, SEEK_SET);
      if (res != (off_t)To)
	 return filefd->FileFdError("Unable to seek to %llu", To);
      seekpos = To;
      buffer.reset();
      return true;
   }
   virtual bool InternalSkip(unsigned long long Over) APT_OVERRIDE
   {
      if (Over >= buffer.size())
      {
	 Over -= buffer.size();
	 buffer.reset();
      }
      else
      {
	 buffer.bufferstart += Over;
	 return true;
      }
      if (Over == 0)
	 return true;
      off_t const res = lseek(filefd->iFd, Over, SEEK_CUR);
      if (res < 0)
	 return filefd->FileFdError("Unable to seek ahead %llu",Over);
      seekpos = res;
      return true;
   }
   virtual bool InternalTruncate(unsigned long long const To) APT_OVERRIDE
   {
      if (buffer.size() != 0)
      {
	 unsigned long long const seekpos = lseek(filefd->iFd, 0, SEEK_CUR);
	 if ((seekpos - buffer.size()) >= To)
	    buffer.reset();
	 else if (seekpos >= To)
	    buffer.bufferend = (To - seekpos) + buffer.bufferstart;
	 else
	    buffer.reset();
      }
      if (ftruncate(filefd->iFd, To) != 0)
	 return filefd->FileFdError("Unable to truncate to %llu",To);
      return true;
   }
   virtual unsigned long long InternalTell() APT_OVERRIDE
   {
      return lseek(filefd->iFd,0,SEEK_CUR) - buffer.size();
   }
   virtual unsigned long long InternalSize() APT_OVERRIDE
   {
      return filefd->FileSize();
   }
   virtual bool InternalClose(std::string const &) APT_OVERRIDE { return true; }
   virtual bool InternalAlwaysAutoClose() const APT_OVERRIDE { return false; }

   explicit DirectFileFdPrivate(FileFd * const filefd) : FileFdPrivate(filefd) {}
   virtual ~DirectFileFdPrivate() { InternalClose(""); }
};
									/*}}}*/
// FileFd Constructors							/*{{{*/
FileFd::FileFd(std::string FileName,unsigned int const Mode,unsigned long AccessMode) : iFd(-1), Flags(0), d(NULL)
{
   Open(FileName,Mode, None, AccessMode);
}
FileFd::FileFd(std::string FileName,unsigned int const Mode, CompressMode Compress, unsigned long AccessMode) : iFd(-1), Flags(0), d(NULL)
{
   Open(FileName,Mode, Compress, AccessMode);
}
FileFd::FileFd() : iFd(-1), Flags(AutoClose), d(NULL) {}
FileFd::FileFd(int const Fd, unsigned int const Mode, CompressMode Compress) : iFd(-1), Flags(0), d(NULL)
{
   OpenDescriptor(Fd, Mode, Compress);
}
FileFd::FileFd(int const Fd, bool const AutoClose) : iFd(-1), Flags(0), d(NULL)
{
   OpenDescriptor(Fd, ReadWrite, None, AutoClose);
}
									/*}}}*/
// FileFd::Open - Open a file						/*{{{*/
// ---------------------------------------------------------------------
/* The most commonly used open mode combinations are given with Mode */
bool FileFd::Open(string FileName,unsigned int const Mode,CompressMode Compress, unsigned long const AccessMode)
{
   if (Mode == ReadOnlyGzip)
      return Open(FileName, ReadOnly, Gzip, AccessMode);

   if (Compress == Auto && (Mode & WriteOnly) == WriteOnly)
      return FileFdError("Autodetection on %s only works in ReadOnly openmode!", FileName.c_str());

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   if (Compress == Auto)
   {
      for (; compressor != compressors.end(); ++compressor)
      {
	 std::string file = FileName + compressor->Extension;
	 if (FileExists(file) == false)
	    continue;
	 FileName = file;
	 break;
      }
   }
   else if (Compress == Extension)
   {
      std::string::size_type const found = FileName.find_last_of('.');
      std::string ext;
      if (found != std::string::npos)
      {
	 ext = FileName.substr(found);
	 if (ext == ".new" || ext == ".bak")
	 {
	    std::string::size_type const found2 = FileName.find_last_of('.', found - 1);
	    if (found2 != std::string::npos)
	       ext = FileName.substr(found2, found - found2);
	    else
	       ext.clear();
	 }
      }
      for (; compressor != compressors.end(); ++compressor)
	 if (ext == compressor->Extension)
	    break;
      // no matching extension - assume uncompressed (imagine files like 'example.org_Packages')
      if (compressor == compressors.end())
	 for (compressor = compressors.begin(); compressor != compressors.end(); ++compressor)
	    if (compressor->Name == ".")
	       break;
   }
   else
   {
      std::string name;
      switch (Compress)
      {
      case None: name = "."; break;
      case Gzip: name = "gzip"; break;
      case Bzip2: name = "bzip2"; break;
      case Lzma: name = "lzma"; break;
      case Xz: name = "xz"; break;
      case Lz4: name = "lz4"; break;
      case Auto:
      case Extension:
	 // Unreachable
	 return FileFdError("Opening File %s in None, Auto or Extension should be already handled?!?", FileName.c_str());
      }
      for (; compressor != compressors.end(); ++compressor)
	 if (compressor->Name == name)
	    break;
      if (compressor == compressors.end())
	 return FileFdError("Can't find a configured compressor %s for file %s", name.c_str(), FileName.c_str());
   }

   if (compressor == compressors.end())
      return FileFdError("Can't find a match for specified compressor mode for file %s", FileName.c_str());
   return Open(FileName, Mode, *compressor, AccessMode);
}
bool FileFd::Open(string FileName,unsigned int const Mode,APT::Configuration::Compressor const &compressor, unsigned long const AccessMode)
{
   Close();
   Flags = AutoClose;

   if ((Mode & WriteOnly) != WriteOnly && (Mode & (Atomic | Create | Empty | Exclusive)) != 0)
      return FileFdError("ReadOnly mode for %s doesn't accept additional flags!", FileName.c_str());
   if ((Mode & ReadWrite) == 0)
      return FileFdError("No openmode provided in FileFd::Open for %s", FileName.c_str());

   unsigned int OpenMode = Mode;
   if (FileName == "/dev/null")
      OpenMode = OpenMode & ~(Atomic | Exclusive | Create | Empty);

   if ((OpenMode & Atomic) == Atomic)
   {
      Flags |= Replace;
   }
   else if ((OpenMode & (Exclusive | Create)) == (Exclusive | Create))
   {
      // for atomic, this will be done by rename in Close()
      RemoveFile("FileFd::Open", FileName);
   }
   if ((OpenMode & Empty) == Empty)
   {
      struct stat Buf;
      if (lstat(FileName.c_str(),&Buf) == 0 && S_ISLNK(Buf.st_mode))
	 RemoveFile("FileFd::Open", FileName);
   }

   int fileflags = 0;
   #define if_FLAGGED_SET(FLAG, MODE) if ((OpenMode & FLAG) == FLAG) fileflags |= MODE
   if_FLAGGED_SET(ReadWrite, O_RDWR);
   else if_FLAGGED_SET(ReadOnly, O_RDONLY);
   else if_FLAGGED_SET(WriteOnly, O_WRONLY);

   if_FLAGGED_SET(Create, O_CREAT);
   if_FLAGGED_SET(Empty, O_TRUNC);
   if_FLAGGED_SET(Exclusive, O_EXCL);
   #undef if_FLAGGED_SET

   if ((OpenMode & Atomic) == Atomic)
   {
      char *name = strdup((FileName + ".XXXXXX").c_str());

      if((iFd = mkstemp(name)) == -1)
      {
          free(name);
          return FileFdErrno("mkstemp", "Could not create temporary file for %s", FileName.c_str());
      }

      TemporaryFileName = string(name);
      free(name);

      // umask() will always set the umask and return the previous value, so
      // we first set the umask and then reset it to the old value
      mode_t const CurrentUmask = umask(0);
      umask(CurrentUmask);
      // calculate the actual file permissions (just like open/creat)
      mode_t const FilePermissions = (AccessMode & ~CurrentUmask);

      if(fchmod(iFd, FilePermissions) == -1)
          return FileFdErrno("fchmod", "Could not change permissions for temporary file %s", TemporaryFileName.c_str());
   }
   else
      iFd = open(FileName.c_str(), fileflags, AccessMode);

   this->FileName = FileName;
   if (iFd == -1 || OpenInternDescriptor(OpenMode, compressor) == false)
   {
      if (iFd != -1)
      {
	 close (iFd);
	 iFd = -1;
      }
      return FileFdErrno("open",_("Could not open file %s"), FileName.c_str());
   }

   SetCloseExec(iFd,true);
   return true;
}
									/*}}}*/
// FileFd::OpenDescriptor - Open a filedescriptor			/*{{{*/
bool FileFd::OpenDescriptor(int Fd, unsigned int const Mode, CompressMode Compress, bool AutoClose)
{
   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   std::string name;

   // compat with the old API
   if (Mode == ReadOnlyGzip && Compress == None)
      Compress = Gzip;

   switch (Compress)
   {
   case None: name = "."; break;
   case Gzip: name = "gzip"; break;
   case Bzip2: name = "bzip2"; break;
   case Lzma: name = "lzma"; break;
   case Xz: name = "xz"; break;
   case Lz4: name = "lz4"; break;
   case Auto:
   case Extension:
      if (AutoClose == true && Fd != -1)
	 close(Fd);
      return FileFdError("Opening Fd %d in Auto or Extension compression mode is not supported", Fd);
   }
   for (; compressor != compressors.end(); ++compressor)
      if (compressor->Name == name)
	 break;
   if (compressor == compressors.end())
   {
      if (AutoClose == true && Fd != -1)
	 close(Fd);
      return FileFdError("Can't find a configured compressor %s for file %s", name.c_str(), FileName.c_str());
   }
   return OpenDescriptor(Fd, Mode, *compressor, AutoClose);
}
bool FileFd::OpenDescriptor(int Fd, unsigned int const Mode, APT::Configuration::Compressor const &compressor, bool AutoClose)
{
   Close();
   Flags = (AutoClose) ? FileFd::AutoClose : 0;
   iFd = Fd;
   this->FileName = "";
   if (OpenInternDescriptor(Mode, compressor) == false)
   {
      if (iFd != -1 && (
	(Flags & Compressed) == Compressed ||
	AutoClose == true))
      {
	 close (iFd);
	 iFd = -1;
      }
      return FileFdError(_("Could not open file descriptor %d"), Fd);
   }
   return true;
}
bool FileFd::OpenInternDescriptor(unsigned int const Mode, APT::Configuration::Compressor const &compressor)
{
   if (iFd == -1)
      return false;

   if (d != nullptr)
      d->InternalClose(FileName);

   if (d == nullptr)
   {
      if (false)
	 /* dummy so that the rest can be 'else if's */;
#define APT_COMPRESS_INIT(NAME, CONSTRUCTOR) \
      else if (compressor.Name == NAME) \
	 d = new CONSTRUCTOR(this)
#ifdef HAVE_ZLIB
      APT_COMPRESS_INIT("gzip", GzipFileFdPrivate);
#endif
#ifdef HAVE_BZ2
      APT_COMPRESS_INIT("bzip2", Bz2FileFdPrivate);
#endif
#ifdef HAVE_LZMA
      APT_COMPRESS_INIT("xz", LzmaFileFdPrivate);
      APT_COMPRESS_INIT("lzma", LzmaFileFdPrivate);
#endif
#ifdef HAVE_LZ4
      APT_COMPRESS_INIT("lz4", Lz4FileFdPrivate);
#endif
#undef APT_COMPRESS_INIT
      else if (compressor.Name == "." || compressor.Binary.empty() == true)
	 d = new DirectFileFdPrivate(this);
      else
	 d = new PipedFileFdPrivate(this);

      if (Mode & BufferedWrite)
	 d = new BufferedWriteFileFdPrivate(d);

      d->set_openmode(Mode);
      d->set_compressor(compressor);
      if ((Flags & AutoClose) != AutoClose && d->InternalAlwaysAutoClose())
      {
	 // Need to duplicate fd here or gz/bz2 close for cleanup will close the fd as well
	 int const internFd = dup(iFd);
	 if (internFd == -1)
	    return FileFdErrno("OpenInternDescriptor", _("Could not open file descriptor %d"), iFd);
	 iFd = internFd;
      }
   }
   return d->InternalOpen(iFd, Mode);
}
									/*}}}*/
// FileFd::~File - Closes the file					/*{{{*/
// ---------------------------------------------------------------------
/* If the proper modes are selected then we close the Fd and possibly
   unlink the file on error. */
FileFd::~FileFd()
{
   Close();
   if (d != NULL)
      d->InternalClose(FileName);
   delete d;
   d = NULL;
}
									/*}}}*/
// FileFd::Read - Read a bit of the file				/*{{{*/
// ---------------------------------------------------------------------
/* We are careful to handle interruption by a signal while reading
   gracefully. */
bool FileFd::Read(void *To,unsigned long long Size,unsigned long long *Actual)
{
   if (d == nullptr || Failed())
      return false;
   ssize_t Res = 1;
   errno = 0;
   if (Actual != 0)
      *Actual = 0;
   *((char *)To) = '\0';
   while (Res > 0 && Size > 0)
   {
      Res = d->InternalRead(To, Size);

      if (Res < 0)
      {
	 if (errno == EINTR)
	 {
	    // trick the while-loop into running again
	    Res = 1;
	    errno = 0;
	    continue;
	 }
	 return d->InternalReadError();
      }
      
      To = (char *)To + Res;
      Size -= Res;
      if (d != NULL)
	 d->set_seekpos(d->get_seekpos() + Res);
      if (Actual != 0)
	 *Actual += Res;
   }
   
   if (Size == 0)
      return true;
   
   // Eof handling
   if (Actual != 0)
   {
      Flags |= HitEof;
      return true;
   }

   return FileFdError(_("read, still have %llu to read but none left"), Size);
}
bool FileFd::Read(int const Fd, void *To, unsigned long long Size, unsigned long long * const Actual)
{
   ssize_t Res = 1;
   errno = 0;
   if (Actual != nullptr)
      *Actual = 0;
   *static_cast<char *>(To) = '\0';
   while (Res > 0 && Size > 0)
   {
      Res = read(Fd, To, Size);
      if (Res < 0)
      {
	 if (errno == EINTR)
	 {
	    Res = 1;
	    errno = 0;
	    continue;
	 }
	 return _error->Errno("read", _("Read error"));
      }
      To = static_cast<char *>(To) + Res;
      Size -= Res;
      if (Actual != 0)
	 *Actual += Res;
   }
   if (Size == 0)
      return true;
   if (Actual != nullptr)
      return true;
   return _error->Error(_("read, still have %llu to read but none left"), Size);
}
									/*}}}*/
// FileFd::ReadLine - Read a complete line from the file		/*{{{*/
// ---------------------------------------------------------------------
/* Beware: This method can be quite slow for big buffers on UNcompressed
   files because of the naive implementation! */
char* FileFd::ReadLine(char *To, unsigned long long const Size)
{
   *To = '\0';
   if (d == nullptr || Failed())
      return nullptr;
   return d->InternalReadLine(To, Size);
}
									/*}}}*/
// FileFd::Flush - Flush the file  					/*{{{*/
bool FileFd::Flush()
{
   if (Failed())
      return false;
   if (d == nullptr)
      return true;

   return d->InternalFlush();
}
									/*}}}*/
// FileFd::Write - Write to the file					/*{{{*/
bool FileFd::Write(const void *From,unsigned long long Size)
{
   if (d == nullptr || Failed())
      return false;
   ssize_t Res = 1;
   errno = 0;
   while (Res > 0 && Size > 0)
   {
      Res = d->InternalWrite(From, Size);

      if (Res < 0)
      {
	 if (errno == EINTR)
	 {
	    // trick the while-loop into running again
	    Res = 1;
	    errno = 0;
	    continue;
	 }
	 return d->InternalWriteError();
      }

      From = (char const *)From + Res;
      Size -= Res;
      if (d != NULL)
	 d->set_seekpos(d->get_seekpos() + Res);
   }

   if (Size == 0)
      return true;

   return FileFdError(_("write, still have %llu to write but couldn't"), Size);
}
bool FileFd::Write(int Fd, const void *From, unsigned long long Size)
{
   ssize_t Res = 1;
   errno = 0;
   while (Res > 0 && Size > 0)
   {
      Res = write(Fd,From,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
	 return _error->Errno("write",_("Write error"));

      From = (char const *)From + Res;
      Size -= Res;
   }

   if (Size == 0)
      return true;

   return _error->Error(_("write, still have %llu to write but couldn't"), Size);
}
									/*}}}*/
// FileFd::Seek - Seek in the file					/*{{{*/
bool FileFd::Seek(unsigned long long To)
{
   if (d == nullptr || Failed())
      return false;
   Flags &= ~HitEof;
   return d->InternalSeek(To);
}
									/*}}}*/
// FileFd::Skip - Skip over data in the file				/*{{{*/
bool FileFd::Skip(unsigned long long Over)
{
   if (d == nullptr || Failed())
      return false;
   return d->InternalSkip(Over);
}
									/*}}}*/
// FileFd::Truncate - Truncate the file					/*{{{*/
bool FileFd::Truncate(unsigned long long To)
{
   if (d == nullptr || Failed())
      return false;
   // truncating /dev/null is always successful - as we get an error otherwise
   if (To == 0 && FileName == "/dev/null")
      return true;
   return d->InternalTruncate(To);
}
									/*}}}*/
// FileFd::Tell - Current seek position					/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::Tell()
{
   if (d == nullptr || Failed())
      return false;
   off_t const Res = d->InternalTell();
   if (Res == (off_t)-1)
      FileFdErrno("lseek","Failed to determine the current file position");
   d->set_seekpos(Res);
   return Res;
}
									/*}}}*/
static bool StatFileFd(char const * const msg, int const iFd, std::string const &FileName, struct stat &Buf, FileFdPrivate * const d) /*{{{*/
{
   bool ispipe = (d != NULL && d->get_is_pipe() == true);
   if (ispipe == false)
   {
      if (fstat(iFd,&Buf) != 0)
	 // higher-level code will generate more meaningful messages,
	 // even translated this would be meaningless for users
	 return _error->Errno("fstat", "Unable to determine %s for fd %i", msg, iFd);
      if (FileName.empty() == false)
	 ispipe = S_ISFIFO(Buf.st_mode);
   }

   // for compressor pipes st_size is undefined and at 'best' zero
   if (ispipe == true)
   {
      // we set it here, too, as we get the info here for free
      // in theory the Open-methods should take care of it already
      if (d != NULL)
	 d->set_is_pipe(true);
      if (stat(FileName.c_str(), &Buf) != 0)
	 return _error->Errno("fstat", "Unable to determine %s for file %s", msg, FileName.c_str());
   }
   return true;
}
									/*}}}*/
// FileFd::FileSize - Return the size of the file			/*{{{*/
unsigned long long FileFd::FileSize()
{
   struct stat Buf;
   if (StatFileFd("file size", iFd, FileName, Buf, d) == false)
   {
      Flags |= Fail;
      return 0;
   }
   return Buf.st_size;
}
									/*}}}*/
// FileFd::ModificationTime - Return the time of last touch		/*{{{*/
time_t FileFd::ModificationTime()
{
   struct stat Buf;
   if (StatFileFd("modification time", iFd, FileName, Buf, d) == false)
   {
      Flags |= Fail;
      return 0;
   }
   return Buf.st_mtime;
}
									/*}}}*/
// FileFd::Size - Return the size of the content in the file		/*{{{*/
unsigned long long FileFd::Size()
{
   if (d == nullptr)
      return 0;
   return d->InternalSize();
}
									/*}}}*/
// FileFd::Close - Close the file if the close flag is set		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Close()
{
   if (Failed() == false && Flush() == false)
      return false;
   if (iFd == -1)
      return true;

   bool Res = true;
   if ((Flags & AutoClose) == AutoClose)
   {
      if ((Flags & Compressed) != Compressed && iFd > 0 && close(iFd) != 0)
	 Res &= _error->Errno("close",_("Problem closing the file %s"), FileName.c_str());
   }

   if (d != NULL)
   {
      Res &= d->InternalClose(FileName);
      delete d;
      d = NULL;
   }

   if ((Flags & Replace) == Replace) {
      if (Failed() == false && rename(TemporaryFileName.c_str(), FileName.c_str()) != 0)
	 Res &= _error->Errno("rename",_("Problem renaming the file %s to %s"), TemporaryFileName.c_str(), FileName.c_str());

      FileName = TemporaryFileName; // for the unlink() below.
      TemporaryFileName.clear();
   }

   iFd = -1;

   if ((Flags & Fail) == Fail && (Flags & DelOnFail) == DelOnFail &&
       FileName.empty() == false)
      Res &= RemoveFile("FileFd::Close", FileName);

   if (Res == false)
      Flags |= Fail;
   return Res;
}
									/*}}}*/
// FileFd::Sync - Sync the file						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Sync()
{
   if (fsync(iFd) != 0)
      return FileFdErrno("sync",_("Problem syncing the file"));
   return true;
}
									/*}}}*/
// FileFd::FileFdErrno - set Fail and call _error->Errno		*{{{*/
bool FileFd::FileFdErrno(const char *Function, const char *Description,...)
{
   Flags |= Fail;
   va_list args;
   size_t msgSize = 400;
   int const errsv = errno;
   bool retry;
   do {
      va_start(args,Description);
      retry = _error->InsertErrno(GlobalError::ERROR, Function, Description, args, errsv, msgSize);
      va_end(args);
   } while (retry);
   return false;
}
									/*}}}*/
// FileFd::FileFdError - set Fail and call _error->Error		*{{{*/
bool FileFd::FileFdError(const char *Description,...) {
   Flags |= Fail;
   va_list args;
   size_t msgSize = 400;
   bool retry;
   do {
      va_start(args,Description);
      retry = _error->Insert(GlobalError::ERROR, Description, args, msgSize);
      va_end(args);
   } while (retry);
   return false;
}
									/*}}}*/
gzFile FileFd::gzFd() {							/*{{{*/
#ifdef HAVE_ZLIB
   GzipFileFdPrivate * const gzipd = dynamic_cast<GzipFileFdPrivate*>(d);
   if (gzipd == nullptr)
      return nullptr;
   else
      return gzipd->gz;
#else
   return nullptr;
#endif
}
									/*}}}*/

// Glob - wrapper around "glob()"					/*{{{*/
std::vector<std::string> Glob(std::string const &pattern, int flags)
{
   std::vector<std::string> result;
   glob_t globbuf;
   int glob_res;
   unsigned int i;

   glob_res = glob(pattern.c_str(),  flags, NULL, &globbuf);

   if (glob_res != 0)
   {
      if(glob_res != GLOB_NOMATCH) {
         _error->Errno("glob", "Problem with glob");
         return result;
      }
   }

   // append results
   for(i=0;i<globbuf.gl_pathc;i++)
      result.push_back(string(globbuf.gl_pathv[i]));

   globfree(&globbuf);
   return result;
}
									/*}}}*/
static std::string APT_NONNULL(1) GetTempDirEnv(char const * const env)	/*{{{*/
{
   const char *tmpdir = getenv(env);

#ifdef P_tmpdir
   if (!tmpdir)
      tmpdir = P_tmpdir;
#endif

   struct stat st;
   if (!tmpdir || strlen(tmpdir) == 0 || // tmpdir is set
	 stat(tmpdir, &st) != 0 || (st.st_mode & S_IFDIR) == 0) // exists and is directory
      tmpdir = "/tmp";
   else if (geteuid() != 0 && // root can do everything anyway
	 faccessat(AT_FDCWD, tmpdir, R_OK | W_OK | X_OK, AT_EACCESS) != 0) // current user has rwx access to directory
      tmpdir = "/tmp";

   return string(tmpdir);
}
									/*}}}*/
std::string GetTempDir()						/*{{{*/
{
   return GetTempDirEnv("TMPDIR");
}
std::string GetTempDir(std::string const &User)
{
   // no need/possibility to drop privs
   if(getuid() != 0 || User.empty() || User == "root")
      return GetTempDir();

   struct passwd const * const pw = getpwnam(User.c_str());
   if (pw == NULL)
      return GetTempDir();

   gid_t const old_euid = geteuid();
   gid_t const old_egid = getegid();
   if (setegid(pw->pw_gid) != 0)
      _error->Errno("setegid", "setegid %u failed", pw->pw_gid);
   if (seteuid(pw->pw_uid) != 0)
      _error->Errno("seteuid", "seteuid %u failed", pw->pw_uid);

   std::string const tmp = GetTempDir();

   if (seteuid(old_euid) != 0)
      _error->Errno("seteuid", "seteuid %u failed", old_euid);
   if (setegid(old_egid) != 0)
      _error->Errno("setegid", "setegid %u failed", old_egid);

   return tmp;
}
									/*}}}*/
FileFd* GetTempFile(std::string const &Prefix, bool ImmediateUnlink, FileFd * const TmpFd)	/*{{{*/
{
   char fn[512];
   FileFd * const Fd = TmpFd == NULL ? new FileFd() : TmpFd;

   std::string const tempdir = GetTempDir();
   snprintf(fn, sizeof(fn), "%s/%s.XXXXXX",
            tempdir.c_str(), Prefix.c_str());
   int const fd = mkstemp(fn);
   if(ImmediateUnlink)
      unlink(fn);
   if (fd < 0)
   {
      _error->Errno("GetTempFile",_("Unable to mkstemp %s"), fn);
      return NULL;
   }
   if (!Fd->OpenDescriptor(fd, FileFd::ReadWrite, FileFd::None, true))
   {
      _error->Errno("GetTempFile",_("Unable to write to %s"),fn);
      return NULL;
   }
   return Fd;
}
									/*}}}*/
bool Rename(std::string From, std::string To)				/*{{{*/
{
   if (rename(From.c_str(),To.c_str()) != 0)
   {
      _error->Error(_("rename failed, %s (%s -> %s)."),strerror(errno),
                    From.c_str(),To.c_str());
      return false;
   }
   return true;
}
									/*}}}*/
bool Popen(const char* Args[], FileFd &Fd, pid_t &Child, FileFd::OpenMode Mode)/*{{{*/
{
   return Popen(Args, Fd, Child, Mode, true);
}
									/*}}}*/
bool Popen(const char* Args[], FileFd &Fd, pid_t &Child, FileFd::OpenMode Mode, bool CaptureStderr)/*{{{*/
{
   int fd;
   if (Mode != FileFd::ReadOnly && Mode != FileFd::WriteOnly)
      return _error->Error("Popen supports ReadOnly (x)or WriteOnly mode only");

   int Pipe[2] = {-1, -1};
   if(pipe(Pipe) != 0)
      return _error->Errno("pipe", _("Failed to create subprocess IPC"));

   std::set<int> keep_fds;
   keep_fds.insert(Pipe[0]);
   keep_fds.insert(Pipe[1]);
   Child = ExecFork(keep_fds);
   if(Child < 0)
      return _error->Errno("fork", "Failed to fork");
   if(Child == 0)
   {
      if(Mode == FileFd::ReadOnly)
      {
         close(Pipe[0]);
         fd = Pipe[1];
      }
      else if(Mode == FileFd::WriteOnly)
      {
         close(Pipe[1]);
         fd = Pipe[0];
      }

      if(Mode == FileFd::ReadOnly)
      {
         dup2(fd, 1);
	 if (CaptureStderr == true)
	    dup2(fd, 2);
      } else if(Mode == FileFd::WriteOnly)
         dup2(fd, 0);

      execv(Args[0], (char**)Args);
      _exit(100);
   }
   if(Mode == FileFd::ReadOnly)
   {
      close(Pipe[1]);
      fd = Pipe[0];
   }
   else if(Mode == FileFd::WriteOnly)
   {
      close(Pipe[0]);
      fd = Pipe[1];
   }
   else
      return _error->Error("Popen supports ReadOnly (x)or WriteOnly mode only");
   Fd.OpenDescriptor(fd, Mode, FileFd::None, true);

   return true;
}
									/*}}}*/
bool DropPrivileges()							/*{{{*/
{
   if(_config->FindB("Debug::NoDropPrivs", false) == true)
      return true;

#if __gnu_linux__
#if defined(PR_SET_NO_NEW_PRIVS) && ( PR_SET_NO_NEW_PRIVS != 38 )
#error "PR_SET_NO_NEW_PRIVS is defined, but with a different value than expected!"
#endif
   // see prctl(2), needs linux3.5 at runtime - magic constant to avoid it at buildtime
   int ret = prctl(38, 1, 0, 0, 0);
   // ignore EINVAL - kernel is too old to understand the option
   if(ret < 0 && errno != EINVAL)
      _error->Warning("PR_SET_NO_NEW_PRIVS failed with %i", ret);
#endif

   // empty setting disables privilege dropping - this also ensures
   // backward compatibility, see bug #764506
   const std::string toUser = _config->Find("APT::Sandbox::User");
   if (toUser.empty() || toUser == "root")
      return true;

   // a lot can go wrong trying to drop privileges completely,
   // so ideally we would like to verify that we have done it –
   // but the verify asks for too much in case of fakeroot (and alike)
   // [Specific checks can be overridden with dedicated options]
   bool const VerifySandboxing = _config->FindB("APT::Sandbox::Verify", false);

   // uid will be 0 in the end, but gid might be different anyway
   uid_t const old_uid = getuid();
   gid_t const old_gid = getgid();

   if (old_uid != 0)
      return true;

   struct passwd *pw = getpwnam(toUser.c_str());
   if (pw == NULL)
      return _error->Error("No user %s, can not drop rights", toUser.c_str());

   // Do not change the order here, it might break things
   // Get rid of all our supplementary groups first
   if (setgroups(1, &pw->pw_gid))
      return _error->Errno("setgroups", "Failed to setgroups");

   // Now change the group ids to the new user
#ifdef HAVE_SETRESGID
   if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) != 0)
      return _error->Errno("setresgid", "Failed to set new group ids");
#else
   if (setegid(pw->pw_gid) != 0)
      return _error->Errno("setegid", "Failed to setegid");

   if (setgid(pw->pw_gid) != 0)
      return _error->Errno("setgid", "Failed to setgid");
#endif

   // Change the user ids to the new user
#ifdef HAVE_SETRESUID
   if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) != 0)
      return _error->Errno("setresuid", "Failed to set new user ids");
#else
   if (setuid(pw->pw_uid) != 0)
      return _error->Errno("setuid", "Failed to setuid");
   if (seteuid(pw->pw_uid) != 0)
      return _error->Errno("seteuid", "Failed to seteuid");
#endif

   // disabled by default as fakeroot doesn't implement getgroups currently (#806521)
   if (VerifySandboxing == true || _config->FindB("APT::Sandbox::Verify::Groups", false) == true)
   {
      // Verify that the user isn't still in any supplementary groups
      long const ngroups_max = sysconf(_SC_NGROUPS_MAX);
      std::unique_ptr<gid_t[]> gidlist(new gid_t[ngroups_max]);
      if (unlikely(gidlist == NULL))
	 return _error->Error("Allocation of a list of size %lu for getgroups failed", ngroups_max);
      ssize_t gidlist_nr;
      if ((gidlist_nr = getgroups(ngroups_max, gidlist.get())) < 0)
	 return _error->Errno("getgroups", "Could not get new groups (%lu)", ngroups_max);
      for (ssize_t i = 0; i < gidlist_nr; ++i)
	 if (gidlist[i] != pw->pw_gid)
	    return _error->Error("Could not switch group, user %s is still in group %d", toUser.c_str(), gidlist[i]);
   }

   // enabled by default as all fakeroot-lookalikes should fake that accordingly
   if (VerifySandboxing == true || _config->FindB("APT::Sandbox::Verify::IDs", true) == true)
   {
      // Verify that gid, egid, uid, and euid changed
      if (getgid() != pw->pw_gid)
	 return _error->Error("Could not switch group");
      if (getegid() != pw->pw_gid)
	 return _error->Error("Could not switch effective group");
      if (getuid() != pw->pw_uid)
	 return _error->Error("Could not switch user");
      if (geteuid() != pw->pw_uid)
	 return _error->Error("Could not switch effective user");

#ifdef HAVE_GETRESUID
      // verify that the saved set-user-id was changed as well
      uid_t ruid = 0;
      uid_t euid = 0;
      uid_t suid = 0;
      if (getresuid(&ruid, &euid, &suid))
	 return _error->Errno("getresuid", "Could not get saved set-user-ID");
      if (suid != pw->pw_uid)
	 return _error->Error("Could not switch saved set-user-ID");
#endif

#ifdef HAVE_GETRESGID
      // verify that the saved set-group-id was changed as well
      gid_t rgid = 0;
      gid_t egid = 0;
      gid_t sgid = 0;
      if (getresgid(&rgid, &egid, &sgid))
	 return _error->Errno("getresuid", "Could not get saved set-group-ID");
      if (sgid != pw->pw_gid)
	 return _error->Error("Could not switch saved set-group-ID");
#endif
   }

   // disabled as fakeroot doesn't forbid (by design) (re)gaining root from unprivileged
   if (VerifySandboxing == true || _config->FindB("APT::Sandbox::Verify::Regain", false) == true)
   {
      // Check that uid and gid changes do not work anymore
      if (pw->pw_gid != old_gid && (setgid(old_gid) != -1 || setegid(old_gid) != -1))
	 return _error->Error("Could restore a gid to root, privilege dropping did not work");

      if (pw->pw_uid != old_uid && (setuid(old_uid) != -1 || seteuid(old_uid) != -1))
	 return _error->Error("Could restore a uid to root, privilege dropping did not work");
   }

   if (_config->FindB("APT::Sandbox::ResetEnvironment", true))
   {
      setenv("HOME", pw->pw_dir, 1);
      setenv("USER", pw->pw_name, 1);
      setenv("USERNAME", pw->pw_name, 1);
      setenv("LOGNAME", pw->pw_name, 1);
      auto const shell = flNotDir(pw->pw_shell);
      if (shell == "false" || shell == "nologin")
	 setenv("SHELL", "/bin/sh", 1);
      else
	 setenv("SHELL", pw->pw_shell, 1);
      auto const apt_setenv_tmp = [](char const * const env) {
	 auto const tmpdir = getenv(env);
	 if (tmpdir != nullptr)
	 {
	    auto const ourtmpdir = GetTempDirEnv(env);
	    if (ourtmpdir != tmpdir)
	       setenv(env, ourtmpdir.c_str(), 1);
	 }
      };
      apt_setenv_tmp("TMPDIR");
      apt_setenv_tmp("TEMPDIR");
      apt_setenv_tmp("TMP");
      apt_setenv_tmp("TEMP");
   }

   return true;
}
									/*}}}*/

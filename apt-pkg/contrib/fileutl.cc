// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.cc,v 1.42 2002/09/14 05:29:22 jgg Exp $
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

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <set>
#include <algorithm>

#ifdef HAVE_ZLIB
	#include <zlib.h>
#endif
#ifdef HAVE_BZ2
	#include <bzlib.h>
#endif

#ifdef WORDS_BIGENDIAN
#include <inttypes.h>
#endif

#include <apti18n.h>
									/*}}}*/

using namespace std;

class FileFdPrivate {
	public:
#ifdef HAVE_ZLIB
	gzFile gz;
#else
	void* gz;
#endif
#ifdef HAVE_BZ2
	BZFILE* bz2;
#else
	void* bz2;
#endif
	int compressed_fd;
	pid_t compressor_pid;
	bool pipe;
	APT::Configuration::Compressor compressor;
	unsigned int openmode;
	unsigned long long seekpos;
	FileFdPrivate() : gz(NULL), bz2(NULL),
			  compressed_fd(-1), compressor_pid(-1), pipe(false),
			  openmode(0), seekpos(0) {};
	bool CloseDown(std::string const &FileName)
	{
	   bool Res = true;
#ifdef HAVE_ZLIB
	   if (gz != NULL) {
	      int const e = gzclose(gz);
	      gz = NULL;
	      // gzdclose() on empty files always fails with "buffer error" here, ignore that
	      if (e != 0 && e != Z_BUF_ERROR)
		 Res &= _error->Errno("close",_("Problem closing the gzip file %s"), FileName.c_str());
	   }
#endif
#ifdef HAVE_BZ2
	   if (bz2 != NULL) {
	      BZ2_bzclose(bz2);
	      bz2 = NULL;
	   }
#endif
	   if (compressor_pid > 0)
	      ExecWait(compressor_pid, "FileFdCompressor", true);
	   compressor_pid = -1;

	   return Res;
	}
	~FileFdPrivate() { CloseDown(""); }
};

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
   SPtrArray<unsigned char> Buf = new unsigned char[64000];
   unsigned long long Size = From.Size();
   while (Size != 0)
   {
      unsigned long long ToRead = Size;
      if (Size > 64000)
	 ToRead = 64000;
      
      if (From.Read(Buf,ToRead) == false || 
	  To.Write(Buf,ToRead) == false)
	 return false;
      
      Size -= ToRead;
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
      // Read only .. cant have locking problems there.
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
      
   // Aquire a write lock
   struct flock fl;
   fl.l_type = F_WRLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start = 0;
   fl.l_len = 0;
   if (fcntl(FD,F_SETLK,&fl) == -1)
   {
      if (errno == ENOLCK)
      {
	 _error->Warning(_("Not using locking for nfs mounted lock file %s"),File.c_str());
	 return dup(0);       // Need something for the caller to close	 
      }      
      if (Errors == true)
	 _error->Errno("open",_("Could not get lock %s"),File.c_str());
      
      int Tmp = errno;
      close(FD);
      errno = Tmp;
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
   if (Path.find(Parent, 0) != 0)
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
	     && *C != '_' && *C != '-') {
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
      int Res;
      if ((Res = readlink(NFile.c_str(),Buffer,sizeof(Buffer))) <= 0 || 
	  (unsigned)Res >= sizeof(Buffer))
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
// ExecFork - Magical fork that sanitizes the context before execing	/*{{{*/
// ---------------------------------------------------------------------
/* This is used if you want to cleanse the environment for the forked 
   child, it fixes up the important signals and nukes all of the fds,
   otherwise acts like normal fork. */
pid_t ExecFork()
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

      set<int> KeepFDs;
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

      // Close all of our FDs - just in case
      for (int K = 3; K != 40; K++)
      {
	 if(KeepFDs.find(K) == KeepFDs.end())
	    fcntl(K,F_SETFD,FD_CLOEXEC);
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

// FileFd::Open - Open a file						/*{{{*/
// ---------------------------------------------------------------------
/* The most commonly used open mode combinations are given with Mode */
bool FileFd::Open(string FileName,unsigned int const Mode,CompressMode Compress, unsigned long const Perms)
{
   if (Mode == ReadOnlyGzip)
      return Open(FileName, ReadOnly, Gzip, Perms);

   if (Compress == Auto && (Mode & WriteOnly) == WriteOnly)
      return FileFdError("Autodetection on %s only works in ReadOnly openmode!", FileName.c_str());

   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   if (Compress == Auto)
   {
      for (; compressor != compressors.end(); ++compressor)
      {
	 std::string file = std::string(FileName).append(compressor->Extension);
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
   return Open(FileName, Mode, *compressor, Perms);
}
bool FileFd::Open(string FileName,unsigned int const Mode,APT::Configuration::Compressor const &compressor, unsigned long const Perms)
{
   Close();
   Flags = AutoClose;

   if ((Mode & WriteOnly) != WriteOnly && (Mode & (Atomic | Create | Empty | Exclusive)) != 0)
      return FileFdError("ReadOnly mode for %s doesn't accept additional flags!", FileName.c_str());
   if ((Mode & ReadWrite) == 0)
      return FileFdError("No openmode provided in FileFd::Open for %s", FileName.c_str());

   if ((Mode & Atomic) == Atomic)
   {
      Flags |= Replace;
      char *name = strdup((FileName + ".XXXXXX").c_str());
      TemporaryFileName = string(mktemp(name));
      free(name);
   }
   else if ((Mode & (Exclusive | Create)) == (Exclusive | Create))
   {
      // for atomic, this will be done by rename in Close()
      unlink(FileName.c_str());
   }
   if ((Mode & Empty) == Empty)
   {
      struct stat Buf;
      if (lstat(FileName.c_str(),&Buf) == 0 && S_ISLNK(Buf.st_mode))
	 unlink(FileName.c_str());
   }

   int fileflags = 0;
   #define if_FLAGGED_SET(FLAG, MODE) if ((Mode & FLAG) == FLAG) fileflags |= MODE
   if_FLAGGED_SET(ReadWrite, O_RDWR);
   else if_FLAGGED_SET(ReadOnly, O_RDONLY);
   else if_FLAGGED_SET(WriteOnly, O_WRONLY);

   if_FLAGGED_SET(Create, O_CREAT);
   if_FLAGGED_SET(Empty, O_TRUNC);
   if_FLAGGED_SET(Exclusive, O_EXCL);
   else if_FLAGGED_SET(Atomic, O_EXCL);
   #undef if_FLAGGED_SET

   if (TemporaryFileName.empty() == false)
      iFd = open(TemporaryFileName.c_str(), fileflags, Perms);
   else
      iFd = open(FileName.c_str(), fileflags, Perms);

   this->FileName = FileName;
   if (iFd == -1 || OpenInternDescriptor(Mode, compressor) == false)
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
// ---------------------------------------------------------------------
/* */
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
   if (AutoClose == false && (
#ifdef HAVE_ZLIB
	compressor.Name == "gzip" ||
#endif
#ifdef HAVE_BZ2
	compressor.Name == "bzip2" ||
#endif
	false))
   {
      // Need to duplicate fd here or gzclose for cleanup will close the fd as well
      iFd = dup(Fd);
   }
   else
      iFd = Fd;
   this->FileName = "";
   if (Fd == -1 || OpenInternDescriptor(Mode, compressor) == false)
   {
      if (iFd != -1 && (
#ifdef HAVE_ZLIB
	compressor.Name == "gzip" ||
#endif
#ifdef HAVE_BZ2
	compressor.Name == "bzip2" ||
#endif
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
   if (compressor.Name == "." || compressor.Binary.empty() == true)
      return true;

   if (d == NULL)
   {
      d = new FileFdPrivate();
      d->openmode = Mode;
      d->compressor = compressor;
   }

#ifdef HAVE_ZLIB
   if (compressor.Name == "gzip")
   {
      if (d->gz != NULL)
      {
	 gzclose(d->gz);
	 d->gz = NULL;
      }
      if ((Mode & ReadWrite) == ReadWrite)
	 d->gz = gzdopen(iFd, "r+");
      else if ((Mode & WriteOnly) == WriteOnly)
	 d->gz = gzdopen(iFd, "w");
      else
	 d->gz = gzdopen(iFd, "r");
      if (d->gz == NULL)
	 return false;
      Flags |= Compressed;
      return true;
   }
#endif
#ifdef HAVE_BZ2
   if (compressor.Name == "bzip2")
   {
      if (d->bz2 != NULL)
      {
	 BZ2_bzclose(d->bz2);
	 d->bz2 = NULL;
      }
      if ((Mode & ReadWrite) == ReadWrite)
	 d->bz2 = BZ2_bzdopen(iFd, "r+");
      else if ((Mode & WriteOnly) == WriteOnly)
	 d->bz2 = BZ2_bzdopen(iFd, "w");
      else
	 d->bz2 = BZ2_bzdopen(iFd, "r");
      if (d->bz2 == NULL)
	 return false;
      Flags |= Compressed;
      return true;
   }
#endif

   // collect zombies here in case we reopen
   if (d->compressor_pid > 0)
      ExecWait(d->compressor_pid, "FileFdCompressor", true);

   if ((Mode & ReadWrite) == ReadWrite)
      return FileFdError("ReadWrite mode is not supported for file %s", FileName.c_str());

   bool const Comp = (Mode & WriteOnly) == WriteOnly;
   if (Comp == false)
   {
      // Handle 'decompression' of empty files
      struct stat Buf;
      fstat(iFd, &Buf);
      if (Buf.st_size == 0 && S_ISFIFO(Buf.st_mode) == false)
	 return true;

      // We don't need the file open - instead let the compressor open it
      // as he properly knows better how to efficiently read from 'his' file
      if (FileName.empty() == false)
      {
	 close(iFd);
	 iFd = -1;
      }
   }

   // Create a data pipe
   int Pipe[2] = {-1,-1};
   if (pipe(Pipe) != 0)
      return FileFdErrno("pipe",_("Failed to create subprocess IPC"));
   for (int J = 0; J != 2; J++)
      SetCloseExec(Pipe[J],true);

   d->compressed_fd = iFd;
   d->pipe = true;

   if (Comp == true)
      iFd = Pipe[1];
   else
      iFd = Pipe[0];

   // The child..
   d->compressor_pid = ExecFork();
   if (d->compressor_pid == 0)
   {
      if (Comp == true)
      {
	 dup2(d->compressed_fd,STDOUT_FILENO);
	 dup2(Pipe[0],STDIN_FILENO);
      }
      else
      {
	 if (FileName.empty() == true)
	    dup2(d->compressed_fd,STDIN_FILENO);
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
      if (Comp == false && FileName.empty() == false)
      {
	 Args.push_back("--stdout");
	 if (TemporaryFileName.empty() == false)
	    Args.push_back(TemporaryFileName.c_str());
	 else
	    Args.push_back(FileName.c_str());
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
									/*}}}*/
// FileFd::~File - Closes the file					/*{{{*/
// ---------------------------------------------------------------------
/* If the proper modes are selected then we close the Fd and possibly
   unlink the file on error. */
FileFd::~FileFd()
{
   Close();
   if (d != NULL)
   {
      d->CloseDown(FileName);
      delete d;
      d = NULL;
   }
}
									/*}}}*/
// FileFd::Read - Read a bit of the file				/*{{{*/
// ---------------------------------------------------------------------
/* We are carefull to handle interruption by a signal while reading 
   gracefully. */
bool FileFd::Read(void *To,unsigned long long Size,unsigned long long *Actual)
{
   int Res;
   errno = 0;
   if (Actual != 0)
      *Actual = 0;
   *((char *)To) = '\0';
   do
   {
#ifdef HAVE_ZLIB
      if (d != NULL && d->gz != NULL)
	 Res = gzread(d->gz,To,Size);
      else
#endif
#ifdef HAVE_BZ2
      if (d != NULL && d->bz2 != NULL)
	 Res = BZ2_bzread(d->bz2,To,Size);
      else
#endif
         Res = read(iFd,To,Size);

      if (Res < 0)
      {
	 if (errno == EINTR)
	    continue;
#ifdef HAVE_ZLIB
	 if (d != NULL && d->gz != NULL)
	 {
	    int err;
	    char const * const errmsg = gzerror(d->gz, &err);
	    if (err != Z_ERRNO)
	       return FileFdError("gzread: %s (%d: %s)", _("Read error"), err, errmsg);
	 }
#endif
#ifdef HAVE_BZ2
	 if (d != NULL && d->bz2 != NULL)
	 {
	    int err;
	    char const * const errmsg = BZ2_bzerror(d->bz2, &err);
	    if (err != BZ_IO_ERROR)
	       return FileFdError("BZ2_bzread: %s (%d: %s)", _("Read error"), err, errmsg);
	 }
#endif
	 return FileFdErrno("read",_("Read error"));
      }
      
      To = (char *)To + Res;
      Size -= Res;
      if (d != NULL)
	 d->seekpos += Res;
      if (Actual != 0)
	 *Actual += Res;
   }
   while (Res > 0 && Size > 0);
   
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
									/*}}}*/
// FileFd::ReadLine - Read a complete line from the file		/*{{{*/
// ---------------------------------------------------------------------
/* Beware: This method can be quiet slow for big buffers on UNcompressed
   files because of the naive implementation! */
char* FileFd::ReadLine(char *To, unsigned long long const Size)
{
   *To = '\0';
#ifdef HAVE_ZLIB
   if (d != NULL && d->gz != NULL)
      return gzgets(d->gz, To, Size);
#endif

   unsigned long long read = 0;
   while ((Size - 1) != read)
   {
      unsigned long long done = 0;
      if (Read(To + read, 1, &done) == false)
	 return NULL;
      if (done == 0)
	 break;
      if (To[read++] == '\n')
	 break;
   }
   if (read == 0)
      return NULL;
   To[read] = '\0';
   return To;
}
									/*}}}*/
// FileFd::Write - Write to the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Write(const void *From,unsigned long long Size)
{
   int Res;
   errno = 0;
   do
   {
#ifdef HAVE_ZLIB
      if (d != NULL && d->gz != NULL)
         Res = gzwrite(d->gz,From,Size);
      else
#endif
#ifdef HAVE_BZ2
      if (d != NULL && d->bz2 != NULL)
         Res = BZ2_bzwrite(d->bz2,(void*)From,Size);
      else
#endif
         Res = write(iFd,From,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
      {
#ifdef HAVE_ZLIB
	 if (d != NULL && d->gz != NULL)
	 {
	    int err;
	    char const * const errmsg = gzerror(d->gz, &err);
	    if (err != Z_ERRNO)
	       return FileFdError("gzwrite: %s (%d: %s)", _("Write error"), err, errmsg);
	 }
#endif
#ifdef HAVE_BZ2
	 if (d != NULL && d->bz2 != NULL)
	 {
	    int err;
	    char const * const errmsg = BZ2_bzerror(d->bz2, &err);
	    if (err != BZ_IO_ERROR)
	       return FileFdError("BZ2_bzwrite: %s (%d: %s)", _("Write error"), err, errmsg);
	 }
#endif
	 return FileFdErrno("write",_("Write error"));
      }
      
      From = (char *)From + Res;
      Size -= Res;
      if (d != NULL)
	 d->seekpos += Res;
   }
   while (Res > 0 && Size > 0);
   
   if (Size == 0)
      return true;

   return FileFdError(_("write, still have %llu to write but couldn't"), Size);
}
bool FileFd::Write(int Fd, const void *From, unsigned long long Size)
{
   int Res;
   errno = 0;
   do
   {
      Res = write(Fd,From,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
	 return _error->Errno("write",_("Write error"));

      From = (char *)From + Res;
      Size -= Res;
   }
   while (Res > 0 && Size > 0);

   if (Size == 0)
      return true;

   return _error->Error(_("write, still have %llu to write but couldn't"), Size);
}
									/*}}}*/
// FileFd::Seek - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Seek(unsigned long long To)
{
   if (d != NULL && (d->pipe == true
#ifdef HAVE_BZ2
			|| d->bz2 != NULL
#endif
	))
   {
      // Our poor man seeking in pipes is costly, so try to avoid it
      unsigned long long seekpos = Tell();
      if (seekpos == To)
	 return true;
      else if (seekpos < To)
	 return Skip(To - seekpos);

      if ((d->openmode & ReadOnly) != ReadOnly)
	 return FileFdError("Reopen is only implemented for read-only files!");
#ifdef HAVE_BZ2
     if (d->bz2 != NULL) 
     {
	BZ2_bzclose(d->bz2);
	d->bz2 = NULL;
     }
#endif
      if (iFd != -1)
	 close(iFd);
      iFd = -1;
      if (TemporaryFileName.empty() == false)
	 iFd = open(TemporaryFileName.c_str(), O_RDONLY);
      else if (FileName.empty() == false)
	 iFd = open(FileName.c_str(), O_RDONLY);
      else
      {
	 if (d->compressed_fd > 0)
	    if (lseek(d->compressed_fd, 0, SEEK_SET) != 0)
	       iFd = d->compressed_fd;
	 if (iFd < 0)
	    return FileFdError("Reopen is not implemented for pipes opened with FileFd::OpenDescriptor()!");
      }

      if (OpenInternDescriptor(d->openmode, d->compressor) == false)
	 return FileFdError("Seek on file %s because it couldn't be reopened", FileName.c_str());

      if (To != 0)
	 return Skip(To);

      d->seekpos = To;
      return true;
   }
   int res;
#ifdef HAVE_ZLIB
   if (d != NULL && d->gz)
      res = gzseek(d->gz,To,SEEK_SET);
   else
#endif
      res = lseek(iFd,To,SEEK_SET);
   if (res != (signed)To)
      return FileFdError("Unable to seek to %llu", To);

   if (d != NULL)
      d->seekpos = To;
   return true;
}
									/*}}}*/
// FileFd::Skip - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Skip(unsigned long long Over)
{
   if (d != NULL && (d->pipe == true
#ifdef HAVE_BZ2
			|| d->bz2 != NULL
#endif
	))
   {
      d->seekpos += Over;
      char buffer[1024];
      while (Over != 0)
      {
	 unsigned long long toread = std::min((unsigned long long) sizeof(buffer), Over);
	 if (Read(buffer, toread) == false)
	    return FileFdError("Unable to seek ahead %llu",Over);
	 Over -= toread;
      }
      return true;
   }

   int res;
#ifdef HAVE_ZLIB
   if (d != NULL && d->gz != NULL)
      res = gzseek(d->gz,Over,SEEK_CUR);
   else
#endif
      res = lseek(iFd,Over,SEEK_CUR);
   if (res < 0)
      return FileFdError("Unable to seek ahead %llu",Over);
   if (d != NULL)
      d->seekpos = res;

   return true;
}
									/*}}}*/
// FileFd::Truncate - Truncate the file 				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Truncate(unsigned long long To)
{
#if defined HAVE_ZLIB || defined HAVE_BZ2
   if (d != NULL && (d->gz != NULL || d->bz2 != NULL))
      return FileFdError("Truncating compressed files is not implemented (%s)", FileName.c_str());
#endif
   if (ftruncate(iFd,To) != 0)
      return FileFdError("Unable to truncate to %llu",To);

   return true;
}
									/*}}}*/
// FileFd::Tell - Current seek position					/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::Tell()
{
   // In theory, we could just return seekpos here always instead of
   // seeking around, but not all users of FileFd use always Seek() and co
   // so d->seekpos isn't always true and we can just use it as a hint if
   // we have nothing else, but not always as an authority…
   if (d != NULL && (d->pipe == true
#ifdef HAVE_BZ2
			|| d->bz2 != NULL
#endif
	))
      return d->seekpos;

   off_t Res;
#ifdef HAVE_ZLIB
   if (d != NULL && d->gz != NULL)
     Res = gztell(d->gz);
   else
#endif
     Res = lseek(iFd,0,SEEK_CUR);
   if (Res == (off_t)-1)
      FileFdErrno("lseek","Failed to determine the current file position");
   if (d != NULL)
      d->seekpos = Res;
   return Res;
}
									/*}}}*/
// FileFd::FileSize - Return the size of the file			/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::FileSize()
{
   struct stat Buf;
   if ((d == NULL || d->pipe == false) && fstat(iFd,&Buf) != 0)
      return FileFdErrno("fstat","Unable to determine the file size");

   // for compressor pipes st_size is undefined and at 'best' zero
   if ((d != NULL && d->pipe == true) || S_ISFIFO(Buf.st_mode))
   {
      // we set it here, too, as we get the info here for free
      // in theory the Open-methods should take care of it already
      if (d != NULL)
	 d->pipe = true;
      if (stat(FileName.c_str(), &Buf) != 0)
	 return FileFdErrno("stat","Unable to determine the file size");
   }

   return Buf.st_size;
}
									/*}}}*/
// FileFd::Size - Return the size of the content in the file		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::Size()
{
   unsigned long long size = FileSize();

   // for compressor pipes st_size is undefined and at 'best' zero,
   // so we 'read' the content and 'seek' back - see there
   if (d != NULL && (d->pipe == true
#ifdef HAVE_BZ2
			|| (d->bz2 && size > 0)
#endif
	))
   {
      unsigned long long const oldSeek = Tell();
      char ignore[1000];
      unsigned long long read = 0;
      do {
	 Read(ignore, sizeof(ignore), &read);
      } while(read != 0);
      size = Tell();
      Seek(oldSeek);
   }
#ifdef HAVE_ZLIB
   // only check gzsize if we are actually a gzip file, just checking for
   // "gz" is not sufficient as uncompressed files could be opened with
   // gzopen in "direct" mode as well
   else if (d != NULL && d->gz && !gzdirect(d->gz) && size > 0)
   {
       off_t const oldPos = lseek(iFd,0,SEEK_CUR);
       /* unfortunately zlib.h doesn't provide a gzsize(), so we have to do
	* this ourselves; the original (uncompressed) file size is the last 32
	* bits of the file */
       // FIXME: Size for gz-files is limited by 32bit… no largefile support
       if (lseek(iFd, -4, SEEK_END) < 0)
	  return FileFdErrno("lseek","Unable to seek to end of gzipped file");
       size = 0L;
       if (read(iFd, &size, 4) != 4)
	  return FileFdErrno("read","Unable to read original size of gzipped file");

#ifdef WORDS_BIGENDIAN
       uint32_t tmp_size = size;
       uint8_t const * const p = (uint8_t const * const) &tmp_size;
       tmp_size = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
       size = tmp_size;
#endif

       if (lseek(iFd, oldPos, SEEK_SET) < 0)
	  return FileFdErrno("lseek","Unable to seek in gzipped file");

       return size;
   }
#endif

   return size;
}
									/*}}}*/
// FileFd::ModificationTime - Return the time of last touch		/*{{{*/
// ---------------------------------------------------------------------
/* */
time_t FileFd::ModificationTime()
{
   struct stat Buf;
   if ((d == NULL || d->pipe == false) && fstat(iFd,&Buf) != 0)
   {
      FileFdErrno("fstat","Unable to determine the modification time of file %s", FileName.c_str());
      return 0;
   }

   // for compressor pipes st_size is undefined and at 'best' zero
   if ((d != NULL && d->pipe == true) || S_ISFIFO(Buf.st_mode))
   {
      // we set it here, too, as we get the info here for free
      // in theory the Open-methods should take care of it already
      if (d != NULL)
	 d->pipe = true;
      if (stat(FileName.c_str(), &Buf) != 0)
      {
	 FileFdErrno("fstat","Unable to determine the modification time of file %s", FileName.c_str());
	 return 0;
      }
   }

   return Buf.st_mtime;
}
									/*}}}*/
// FileFd::Close - Close the file if the close flag is set		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Close()
{
   if (iFd == -1)
      return true;

   bool Res = true;
   if ((Flags & AutoClose) == AutoClose)
   {
      if ((Flags & Compressed) != Compressed && iFd > 0 && close(iFd) != 0)
	 Res &= _error->Errno("close",_("Problem closing the file %s"), FileName.c_str());

      if (d != NULL)
      {
	 Res &= d->CloseDown(FileName);
	 delete d;
	 d = NULL;
      }
   }

   if ((Flags & Replace) == Replace) {
      if (rename(TemporaryFileName.c_str(), FileName.c_str()) != 0)
	 Res &= _error->Errno("rename",_("Problem renaming the file %s to %s"), TemporaryFileName.c_str(), FileName.c_str());

      FileName = TemporaryFileName; // for the unlink() below.
      TemporaryFileName.clear();
   }

   iFd = -1;

   if ((Flags & Fail) == Fail && (Flags & DelOnFail) == DelOnFail &&
       FileName.empty() == false)
      if (unlink(FileName.c_str()) != 0)
	 Res &= _error->WarningE("unlnk",_("Problem unlinking the file %s"), FileName.c_str());

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
   while (true)
   {
      va_start(args,Description);
      if (_error->InsertErrno(GlobalError::ERROR, Function, Description, args, errsv, msgSize) == false)
	 break;
      va_end(args);
   }
   return false;
}
									/*}}}*/
// FileFd::FileFdError - set Fail and call _error->Error		*{{{*/
bool FileFd::FileFdError(const char *Description,...) {
   Flags |= Fail;
   va_list args;
   size_t msgSize = 400;
   while (true)
   {
      va_start(args,Description);
      if (_error->Insert(GlobalError::ERROR, Description, args, msgSize) == false)
	 break;
      va_end(args);
   }
   return false;
}
									/*}}}*/

gzFile FileFd::gzFd() { return (gzFile) d->gz; }

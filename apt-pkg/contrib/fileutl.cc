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

#include <zlib.h>

#ifdef WORDS_BIGENDIAN
#include <inttypes.h>
#endif

#include <apti18n.h>
									/*}}}*/

using namespace std;

class FileFdPrivate {
	public:
	gzFile gz;
	FileFdPrivate() : gz(NULL) {};
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
   if (From.IsOpen() == false || To.IsOpen() == false)
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

   if (DirectoryExists(Dir.c_str()) == false)
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
	 if (RealFileExists(File.c_str()) == false)
	 {
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
bool FileFd::Open(string FileName,OpenMode Mode,CompressMode Compress, unsigned long const Perms)
{
   if (Mode == ReadOnlyGzip)
      return Open(FileName, ReadOnly, Gzip, Perms);
   Close();
   d = new FileFdPrivate;
   Flags = AutoClose;

   if (Compress == Auto && (Mode & WriteOnly) == WriteOnly)
      return _error->Error("Autodetection on %s only works in ReadOnly openmode!", FileName.c_str());
   if ((Mode & WriteOnly) != WriteOnly && (Mode & (Atomic | Create | Empty | Exclusive)) != 0)
      return _error->Error("ReadOnly mode for %s doesn't accept additional flags!", FileName.c_str());

   int fileflags = 0;
#define if_FLAGGED_SET(FLAG, MODE) if ((Mode & FLAG) == FLAG) fileflags |= MODE
   if_FLAGGED_SET(ReadWrite, O_RDWR);
   else if_FLAGGED_SET(ReadOnly, O_RDONLY);
   else if_FLAGGED_SET(WriteOnly, O_WRONLY);
   else return _error->Error("No openmode provided in FileFd::Open for %s", FileName.c_str());

   if_FLAGGED_SET(Create, O_CREAT);
   if_FLAGGED_SET(Exclusive, O_EXCL);
   else if_FLAGGED_SET(Atomic, O_EXCL);
   if_FLAGGED_SET(Empty, O_TRUNC);
#undef if_FLAGGED_SET

   // FIXME: Denote inbuilt compressors somehow - as we don't need to have the binaries for them
   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   std::vector<APT::Configuration::Compressor>::const_iterator compressor = compressors.begin();
   if (Compress == Auto)
   {
      Compress = None;
      for (; compressor != compressors.end(); ++compressor)
      {
	 std::string file = std::string(FileName).append(compressor->Extension);
	 if (FileExists(file) == false)
	    continue;
	 FileName = file;
	 if (compressor->Binary == ".")
	    Compress = None;
	 else
	    Compress = Extension;
	 break;
      }
   }
   else if (Compress == Extension)
   {
      Compress = None;
      std::string ext = flExtension(FileName);
      if (ext != FileName)
      {
	 ext = "." + ext;
	 for (; compressor != compressors.end(); ++compressor)
	    if (ext == compressor->Extension)
	       break;
      }
   }
   else if (Compress != None)
   {
      std::string name;
      switch (Compress)
      {
      case Gzip: name = "gzip"; break;
      case Bzip2: name = "bzip2"; break;
      case Lzma: name = "lzma"; break;
      case Xz: name = "xz"; break;
      default: return _error->Error("Can't find a match for specified compressor mode for file %s", FileName.c_str());
      }
      for (; compressor != compressors.end(); ++compressor)
	 if (compressor->Name == name)
	    break;
      if (compressor == compressors.end() && name != "gzip")
	 return _error->Error("Can't find a configured compressor %s for file %s", name.c_str(), FileName.c_str());
   }

   // if we have them, use inbuilt compressors instead of forking
   if (compressor != compressors.end())
   {
      if (compressor->Name == "gzip")
      {
	 Compress = Gzip;
	 compressor = compressors.end();
      }
      else if (compressor->Name == "." || Compress == None)
      {
	 Compress = None;
	 compressor = compressors.end();
      }
   }

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

   if (compressor != compressors.end())
   {
      if ((Mode & ReadWrite) == ReadWrite)
	 _error->Error("External compressors like %s do not support readwrite mode for file %s", compressor->Name.c_str(), FileName.c_str());

      _error->Error("Forking external compressor %s is not implemented for %s", compressor->Name.c_str(), FileName.c_str());
   }
   else
   {
      if (TemporaryFileName.empty() == false)
	 iFd = open(TemporaryFileName.c_str(), fileflags, Perms);
      else
	 iFd = open(FileName.c_str(), fileflags, Perms);

      if (iFd != -1)
      {
	 if (OpenInternDescriptor(Mode, Compress) == false)
	 {
	    close (iFd);
	    iFd = -1;
	 }
      }
   }

   if (iFd == -1)
      return _error->Errno("open",_("Could not open file %s"),FileName.c_str());

   this->FileName = FileName;
   SetCloseExec(iFd,true);
   return true;
}
									/*}}}*/
// FileFd::OpenDescriptor - Open a filedescriptor			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::OpenDescriptor(int Fd, OpenMode Mode, CompressMode Compress, bool AutoClose)
{
   Close();
   d = new FileFdPrivate;
   Flags = (AutoClose) ? FileFd::AutoClose : 0;
   iFd = Fd;
   if (OpenInternDescriptor(Mode, Compress) == false)
   {
      if (AutoClose)
	 close (iFd);
      return _error->Errno("gzdopen",_("Could not open file descriptor %d"), Fd);
   }
   this->FileName = "";
   return true;
}
bool FileFd::OpenInternDescriptor(OpenMode Mode, CompressMode Compress)
{
   if (Compress == None)
      return true;
   else if (Compress == Gzip)
   {
      if ((Mode & ReadWrite) == ReadWrite)
	 d->gz = gzdopen(iFd, "r+");
      else if ((Mode & WriteOnly) == WriteOnly)
	 d->gz = gzdopen(iFd, "w");
      else
	 d->gz = gzdopen (iFd, "r");
      if (d->gz == NULL)
	 return false;
      Flags |= Compressed;
   }
   else
      return false;
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
   
   do
   {
      if (d->gz != NULL)
         Res = gzread(d->gz,To,Size);
      else
         Res = read(iFd,To,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
      {
	 Flags |= Fail;
	 return _error->Errno("read",_("Read error"));
      }
      
      To = (char *)To + Res;
      Size -= Res;
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
   
   Flags |= Fail;
   return _error->Error(_("read, still have %llu to read but none left"), Size);
}
									/*}}}*/
// FileFd::ReadLine - Read a complete line from the file		/*{{{*/
// ---------------------------------------------------------------------
/* Beware: This method can be quiet slow for big buffers on UNcompressed
   files because of the naive implementation! */
char* FileFd::ReadLine(char *To, unsigned long long const Size)
{
   if (d->gz != NULL)
      return gzgets(d->gz, To, Size);

   unsigned long long read = 0;
   if (Read(To, Size, &read) == false)
      return NULL;
   char* c = To;
   for (; *c != '\n' && *c != '\0' && read != 0; --read, ++c)
      ; // find the end of the line
   if (*c != '\0')
      *c = '\0';
   if (read != 0)
      Seek(Tell() - read);
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
      if (d->gz != NULL)
         Res = gzwrite(d->gz,From,Size);
      else
         Res = write(iFd,From,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
      {
	 Flags |= Fail;
	 return _error->Errno("write",_("Write error"));
      }
      
      From = (char *)From + Res;
      Size -= Res;
   }
   while (Res > 0 && Size > 0);
   
   if (Size == 0)
      return true;
   
   Flags |= Fail;
   return _error->Error(_("write, still have %llu to write but couldn't"), Size);
}
									/*}}}*/
// FileFd::Seek - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Seek(unsigned long long To)
{
   int res;
   if (d->gz)
      res = gzseek(d->gz,To,SEEK_SET);
   else
      res = lseek(iFd,To,SEEK_SET);
   if (res != (signed)To)
   {
      Flags |= Fail;
      return _error->Error("Unable to seek to %llu", To);
   }
   
   return true;
}
									/*}}}*/
// FileFd::Skip - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Skip(unsigned long long Over)
{
   int res;
   if (d->gz != NULL)
      res = gzseek(d->gz,Over,SEEK_CUR);
   else
      res = lseek(iFd,Over,SEEK_CUR);
   if (res < 0)
   {
      Flags |= Fail;
      return _error->Error("Unable to seek ahead %llu",Over);
   }
   
   return true;
}
									/*}}}*/
// FileFd::Truncate - Truncate the file 				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Truncate(unsigned long long To)
{
   if (d->gz != NULL)
   {
      Flags |= Fail;
      return _error->Error("Truncating gzipped files is not implemented (%s)", FileName.c_str());
   }
   if (ftruncate(iFd,To) != 0)
   {
      Flags |= Fail;
      return _error->Error("Unable to truncate to %llu",To);
   }
   
   return true;
}
									/*}}}*/
// FileFd::Tell - Current seek position					/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::Tell()
{
   off_t Res;
   if (d->gz != NULL)
     Res = gztell(d->gz);
   else
     Res = lseek(iFd,0,SEEK_CUR);
   if (Res == (off_t)-1)
      _error->Errno("lseek","Failed to determine the current file position");
   return Res;
}
									/*}}}*/
// FileFd::FileSize - Return the size of the file			/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::FileSize()
{
   struct stat Buf;

   if (fstat(iFd,&Buf) != 0)
      return _error->Errno("fstat","Unable to determine the file size");
   return Buf.st_size;
}
									/*}}}*/
// FileFd::Size - Return the size of the content in the file		/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long long FileFd::Size()
{
   unsigned long long size = FileSize();

   // only check gzsize if we are actually a gzip file, just checking for
   // "gz" is not sufficient as uncompressed files could be opened with
   // gzopen in "direct" mode as well
   if (d->gz && !gzdirect(d->gz) && size > 0)
   {
       /* unfortunately zlib.h doesn't provide a gzsize(), so we have to do
	* this ourselves; the original (uncompressed) file size is the last 32
	* bits of the file */
       // FIXME: Size for gz-files is limited by 32bit… no largefile support
       off_t orig_pos = lseek(iFd, 0, SEEK_CUR);
       if (lseek(iFd, -4, SEEK_END) < 0)
	   return _error->Errno("lseek","Unable to seek to end of gzipped file");
       size = 0L;
       if (read(iFd, &size, 4) != 4)
	   return _error->Errno("read","Unable to read original size of gzipped file");

#ifdef WORDS_BIGENDIAN
       uint32_t tmp_size = size;
       uint8_t const * const p = (uint8_t const * const) &tmp_size;
       tmp_size = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
       size = tmp_size;
#endif

       if (lseek(iFd, orig_pos, SEEK_SET) < 0)
	   return _error->Errno("lseek","Unable to seek in gzipped file");
       return size;
   }

   return size;
}
									/*}}}*/
// FileFd::ModificationTime - Return the time of last touch		/*{{{*/
// ---------------------------------------------------------------------
/* */
time_t FileFd::ModificationTime()
{
   struct stat Buf;
   if (fstat(iFd,&Buf) != 0)
   {
      _error->Errno("fstat","Unable to determine the modification time of file %s", FileName.c_str());
      return 0;
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
      if (d != NULL && d->gz != NULL) {
	 int const e = gzclose(d->gz);
	 // gzdopen() on empty files always fails with "buffer error" here, ignore that
	 if (e != 0 && e != Z_BUF_ERROR)
	    Res &= _error->Errno("close",_("Problem closing the gzip file %s"), FileName.c_str());
      } else
	 if (iFd > 0 && close(iFd) != 0)
	    Res &= _error->Errno("close",_("Problem closing the file %s"), FileName.c_str());
   }

   if ((Flags & Replace) == Replace && iFd >= 0) {
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

   if (d != NULL)
   {
      delete d;
      d = NULL;
   }

   return Res;
}
									/*}}}*/
// FileFd::Sync - Sync the file						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Sync()
{
#ifdef _POSIX_SYNCHRONIZED_IO
   if (fsync(iFd) != 0)
      return _error->Errno("sync",_("Problem syncing the file"));
#endif
   return true;
}
									/*}}}*/
gzFile FileFd::gzFd() {return d->gz;};

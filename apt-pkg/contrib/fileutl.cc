// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.cc,v 1.28 1999/07/11 22:42:32 jgg Exp $
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/fileutl.h"
#endif 
#include <apt-pkg/fileutl.h>
#include <apt-pkg/error.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
									/*}}}*/

// CopyFile - Buffered copy of a file					/*{{{*/
// ---------------------------------------------------------------------
/* The caller is expected to set things so that failure causes erasure */
bool CopyFile(FileFd &From,FileFd &To)
{
   if (From.IsOpen() == false || To.IsOpen() == false)
      return false;
   
   // Buffered copy between fds
   unsigned char *Buf = new unsigned char[64000];
   unsigned long Size = From.Size();
   while (Size != 0)
   {
      unsigned long ToRead = Size;
      if (Size > 64000)
	 ToRead = 64000;
      
      if (From.Read(Buf,ToRead) == false || 
	  To.Write(Buf,ToRead) == false)
      {
	 delete [] Buf;
	 return false;
      }
      
      Size -= ToRead;
   }

   delete [] Buf;
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
   int FD = open(File.c_str(),O_RDWR | O_CREAT | O_TRUNC,0640);
   if (FD < 0)
   {
      if (Errors == true)
	 _error->Errno("open","Could not open lock file %s",File.c_str());
      return -1;
   }
   
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
	 _error->Warning("Not using locking for nfs mounted lock file %s",File.c_str());
	 return true;
      }      
      if (Errors == true)
	 _error->Errno("open","Could not get lock %s",File.c_str());
      close(FD);
      return -1;
   }

   return FD;
}
									/*}}}*/
// FileExists - Check if a file exists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileExists(string File)
{
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
      return false;
   return true;
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
/* */
string flNotFile(string File)
{
   string::size_type Res = File.rfind('/');
   if (Res == string::npos)
      return File;
   Res++;
   return string(File,0,Res);
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
/* This waits for a FD to become readable using select. It is usefull for
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
int ExecFork()
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
      
      // Close all of our FDs - just in case
      for (int K = 3; K != 40; K++)
	 fcntl(K,F_SETFD,FD_CLOEXEC);
   }
   
   return Process;
}
									/*}}}*/

// FileFd::Open - Open a file						/*{{{*/
// ---------------------------------------------------------------------
/* The most commonly used open mode combinations are given with Mode */
bool FileFd::Open(string FileName,OpenMode Mode, unsigned long Perms)
{
   Close();
   Flags = AutoClose;
   switch (Mode)
   {
      case ReadOnly:
      iFd = open(FileName.c_str(),O_RDONLY);
      break;
      
      case WriteEmpty:
      {
	 struct stat Buf;
	 if (stat(FileName.c_str(),&Buf) == 0 && S_ISLNK(Buf.st_mode))
	    unlink(FileName.c_str());
	 iFd = open(FileName.c_str(),O_RDWR | O_CREAT | O_TRUNC,Perms);
	 break;
      }
      
      case WriteExists:
      iFd = open(FileName.c_str(),O_RDWR);
      break;

      case WriteAny:
      iFd = open(FileName.c_str(),O_RDWR | O_CREAT,Perms);
      break;      
   }  

   if (iFd < 0)
      return _error->Errno("open","Could not open file %s",FileName.c_str());
   
   this->FileName = FileName;
   SetCloseExec(iFd,true);
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
bool FileFd::Read(void *To,unsigned long Size)
{
   int Res;
   errno = 0;
   do
   {
      Res = read(iFd,To,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
      {
	 Flags |= Fail;
	 return _error->Errno("read","Read error");
      }
      
      To = (char *)To + Res;
      Size -= Res;
   }
   while (Res > 0 && Size > 0);
   
   if (Size == 0)
      return true;
   
   Flags |= Fail;
   return _error->Error("read, still have %u to read but none left",Size);
}
									/*}}}*/
// FileFd::Write - Write to the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Write(const void *From,unsigned long Size)
{
   int Res;
   errno = 0;
   do
   {
      Res = write(iFd,From,Size);
      if (Res < 0 && errno == EINTR)
	 continue;
      if (Res < 0)
      {
	 Flags |= Fail;
	 return _error->Errno("write","Write error");
      }
      
      From = (char *)From + Res;
      Size -= Res;
   }
   while (Res > 0 && Size > 0);
   
   if (Size == 0)
      return true;
   
   Flags |= Fail;
   return _error->Error("write, still have %u to write but couldn't",Size);
}
									/*}}}*/
// FileFd::Seek - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Seek(unsigned long To)
{
   if (lseek(iFd,To,SEEK_SET) != (signed)To)
   {
      Flags |= Fail;
      return _error->Error("Unable to seek to %u",To);
   }
   
   return true;
}
									/*}}}*/
// FileFd::Truncate - Truncate the file 				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Truncate(unsigned long To)
{
   if (ftruncate(iFd,To) != 0)
   {
      Flags |= Fail;
      return _error->Error("Unable to truncate to %u",To);
   }
   
   return true;
}
									/*}}}*/
// FileFd::Tell - Current seek position					/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long FileFd::Tell()
{
   off_t Res = lseek(iFd,0,SEEK_CUR);
   if (Res == (off_t)-1)
      _error->Errno("lseek","Failed to determine the current file position");
   return Res;
}
									/*}}}*/
// FileFd::Size - Return the size of the file				/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long FileFd::Size()
{
   struct stat Buf;
   if (fstat(iFd,&Buf) != 0)
      return _error->Errno("fstat","Unable to determine the file size");
   return Buf.st_size;
}
									/*}}}*/
// FileFd::Close - Close the file if the close flag is set		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileFd::Close()
{
   bool Res = true;
   if ((Flags & AutoClose) == AutoClose)
      if (iFd >= 0 && close(iFd) != 0)
	 Res &= _error->Errno("close","Problem closing the file");
   iFd = -1;
   
   if ((Flags & Fail) == Fail && (Flags & DelOnFail) == DelOnFail &&
       FileName.empty() == false)
      if (unlink(FileName.c_str()) != 0)
	 Res &= _error->Warning("unlnk","Problem unlinking the file");
   return Res;
}
									/*}}}*/

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.h,v 1.26 2001/05/07 05:06:52 jgg Exp $
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   FileExists - Returns true if the file exists
   SafeGetCWD - Returns the CWD in a string with overrun protection 
   
   The file class is a handy abstraction for various functions+classes
   that need to accept filenames.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_FILEUTL_H
#define PKGLIB_FILEUTL_H


#include <string>

using std::string;

class FileFd
{
   protected:
   int iFd;
 
   enum LocalFlags {AutoClose = (1<<0),Fail = (1<<1),DelOnFail = (1<<2),
                    HitEof = (1<<3)};
   unsigned long Flags;
   string FileName;
   
   public:
   enum OpenMode {ReadOnly,WriteEmpty,WriteExists,WriteAny,WriteTemp};
   
   inline bool Read(void *To,unsigned long Size,bool AllowEof)
   {
      unsigned long Jnk;
      if (AllowEof)
	 return Read(To,Size,&Jnk);
      return Read(To,Size);
   }   
   bool Read(void *To,unsigned long Size,unsigned long *Actual = 0);
   bool Write(const void *From,unsigned long Size);
   bool Seek(unsigned long To);
   bool Skip(unsigned long To);
   bool Truncate(unsigned long To);
   unsigned long Tell();
   unsigned long Size();
   bool Open(string FileName,OpenMode Mode,unsigned long Perms = 0666);
   bool Close();
   bool Sync();
   
   // Simple manipulators
   inline int Fd() {return iFd;};
   inline void Fd(int fd) {iFd = fd;};
   inline bool IsOpen() {return iFd >= 0;};
   inline bool Failed() {return (Flags & Fail) == Fail;};
   inline void EraseOnFailure() {Flags |= DelOnFail;};
   inline void OpFail() {Flags |= Fail;};
   inline bool Eof() {return (Flags & HitEof) == HitEof;};
   inline string &Name() {return FileName;};
   
   FileFd(string FileName,OpenMode Mode,unsigned long Perms = 0666) : iFd(-1), 
            Flags(0) 
   {
      Open(FileName,Mode,Perms);
   };
   FileFd(int Fd = -1) : iFd(Fd), Flags(AutoClose) {};
   FileFd(int Fd,bool) : iFd(Fd), Flags(0) {};
   virtual ~FileFd();
};

bool RunScripts(const char *Cnf);
bool CopyFile(FileFd &From,FileFd &To);
int GetLock(string File,bool Errors = true);
bool FileExists(string File);
string SafeGetCWD();
void SetCloseExec(int Fd,bool Close);
void SetNonBlock(int Fd,bool Block);
bool WaitFd(int Fd,bool write = false,unsigned long timeout = 0);
pid_t ExecFork();
bool ExecWait(pid_t Pid,const char *Name,bool Reap = false);

// File string manipulators
string flNotDir(string File);
string flNotFile(string File);
string flNoLink(string File);
string flExtension(string File);
string flCombine(string Dir,string File);

#endif

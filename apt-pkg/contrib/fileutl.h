// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.h,v 1.3 1998/07/12 23:58:49 jgg Exp $
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
// Header section: pkglib
#ifndef PKGLIB_FILEUTL_H
#define PKGLIB_FILEUTL_H

#ifdef __GNUG__
#pragma interface "apt-pkg/fileutl.h"
#endif 

#include <string>

class File
{
   protected:
   int iFd;
 
   enum LocalFlags {AutoClose = (1<<0),Fail = (1<<1),DelOnFail = (1<<2)};
   unsigned long Flags;
   string FileName;
   
   public:
   enum OpenMode {ReadOnly,WriteEmpty,WriteExists};
   
   bool Read(void *To,unsigned long Size);
   bool Write(void *From,unsigned long Size);
   bool Seek(unsigned long To);
   unsigned long Size();
   bool Close();

   // Simple manipulators
   inline int Fd() {return iFd;};
   inline bool IsOpen() {return iFd >= 0;};
   inline bool Failed() {return (Flags & Fail) == Fail;};
   inline void EraseOnFailure() {Flags |= DelOnFail;};
   inline void OpFail() {Flags |= Fail;};
      
   File(string FileName,OpenMode Mode,unsigned long Perms = 0666);
   File(int Fd) : iFd(Fd), Flags(AutoClose) {};
   File(int Fd,bool) : iFd(Fd), Flags(0) {};
   virtual ~File();
};

bool CopyFile(string From,string To);
int GetLock(string File,bool Errors = true);
bool FileExists(string File);
string SafeGetCWD();

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Directory Stream 

   When unpacking the contents of the archive are passed into a directory
   stream class for analysis and processing. The class controls all aspects
   of actually writing the directory stream from disk. The low level
   archive handlers are only responsible for decoding the archive format
   and sending events (via method calls) to the specified directory
   stream.
   
   When unpacking a real file the archive handler is passed back a file 
   handle to write the data to, this is to support strange 
   archives+unpacking methods. If that fd is -1 then the file data is 
   simply ignored.
   
   The provided defaults do the 'Right Thing' for a normal unpacking
   process (ie 'tar')
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DIRSTREAM_H
#define PKGLIB_DIRSTREAM_H

#include <apt-pkg/macros.h>

class APT_PUBLIC pkgDirStream
{ 
   public:

   // All possible information about a component
   struct Item
   {
      enum Type_t {File, HardLink, SymbolicLink, CharDevice, BlockDevice,
	           Directory, FIFO} Type;
      char *Name;
      char *LinkTarget;
      unsigned long Mode;
      unsigned long UID;
      unsigned long GID;
      unsigned long long Size;
      unsigned long MTime;
      unsigned long Major;
      unsigned long Minor;
   };
   
   virtual bool DoItem(Item &Itm,int &Fd);
   virtual bool Fail(Item &Itm,int Fd);
   virtual bool FinishedFile(Item &Itm,int Fd);
   virtual bool Process(Item &/*Itm*/,const unsigned char * /*Data*/,
			unsigned long long /*Size*/,unsigned long long /*Pos*/) {return true;};
   virtual ~pkgDirStream() {};   
};

#endif

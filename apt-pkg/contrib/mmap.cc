// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: mmap.cc,v 1.12 1999/03/18 03:20:24 doogie Exp $
/* ######################################################################
   
   MMap Class - Provides 'real' mmap or a faked mmap using read().

   MMap cover class.

   Some broken versions of glibc2 (libc6) have a broken definition
   of mmap that accepts a char * -- all other systems (and libc5) use
   void *. We can't safely do anything here that would be portable, so
   libc6 generates warnings -- which should be errors, g++ isn't properly
   strict.
   
   The configure test notes that some OS's have broken private mmap's
   so on those OS's we can't use mmap. This means we have to use
   configure to test mmap and can't rely on the POSIX
   _POSIX_MAPPED_FILES test.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/mmap.h"
#endif 

#define _BSD_SOURCE
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
   									/*}}}*/

// MMap::MMap - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::MMap(FileFd &F,unsigned long Flags) : Fd(F), Flags(Flags), iSize(0),
                     Base(0)
{
   if ((Flags & NoImmMap) != NoImmMap)
      Map();
}
									/*}}}*/
// MMap::~MMap - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::~MMap()
{
   Close(true);
}
									/*}}}*/
// MMap::Map - Perform the mapping					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Map()
{
   iSize = Fd.Size();
   
   // Set the permissions.
   int Prot = PROT_READ;
   int Map = MAP_SHARED;
   if ((Flags & ReadOnly) != ReadOnly)
      Prot |= PROT_WRITE;
   if ((Flags & Public) != Public)
      Map = MAP_PRIVATE;
   
   if (iSize == 0)
      return _error->Error("Can't mmap an empty file");
   
   // Map it.
   Base = mmap(0,iSize,Prot,Map,Fd.Fd(),0);
   if (Base == (void *)-1)
      return _error->Errno("mmap","Couldn't make mmap of %u bytes",iSize);

   return true;
}
									/*}}}*/
// MMap::Close - Close the map						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Close(bool DoClose, bool DoSync)
{
   if (Fd.IsOpen() == false)
      return true;

   if (DoSync == true)
      Sync();
   
   if (munmap((char *)Base,iSize) != 0)
      _error->Warning("Unable to munmap");
   
   iSize = 0;
   if (DoClose == true)
      Fd.Close();
   return true;
}
									/*}}}*/
// MMap::Sync - Syncronize the map with the disk			/*{{{*/
// ---------------------------------------------------------------------
/* This is done in syncronous mode - the docs indicate that this will 
   not return till all IO is complete */
bool MMap::Sync()
{   
#ifdef _POSIX_SYNCHRONIZED_IO   
   if ((Flags & ReadOnly) != ReadOnly)
      if (msync((char *)Base,iSize,MS_SYNC) != 0)
	 return _error->Error("msync","Unable to write mmap");
#endif   
   return true;
}
									/*}}}*/
// MMap::Sync - Syncronize a section of the file to disk		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Sync(unsigned long Start,unsigned long Stop)
{
	static int PAGE_SIZE = getpagesize();

#ifdef _POSIX_SYNCHRONIZED_IO   
   if ((Flags & ReadOnly) != ReadOnly)
      if (msync((char *)Base+(int)(Start/PAGE_SIZE)*PAGE_SIZE,Stop - Start,MS_SYNC) != 0)
	 return _error->Error("msync","Unable to write mmap");
#endif   
   return true;
}
									/*}}}*/

// DynamicMMap::DynamicMMap - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
DynamicMMap::DynamicMMap(FileFd &F,unsigned long Flags,unsigned long WorkSpace) : 
             MMap(F,Flags | NoImmMap), WorkSpace(WorkSpace)
{
   if (_error->PendingError() == true)
      return;
   
   unsigned long EndOfFile = Fd.Size();
   Fd.Seek(WorkSpace);
   char C = 0;
   Fd.Write(&C,sizeof(C));
   Map();
   iSize = EndOfFile;
}
									/*}}}*/
// DynamicMMap::~DynamicMMap - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* We truncate the file to the size of the memory data set */
DynamicMMap::~DynamicMMap()
{
   unsigned long EndOfFile = iSize;
   Sync();
   iSize = WorkSpace;
   Close(false,false);
   ftruncate(Fd.Fd(),EndOfFile);
   Fd.Close();
}  
									/*}}}*/
// DynamicMMap::RawAllocate - Allocate a raw chunk of unaligned space	/*{{{*/
// ---------------------------------------------------------------------
/* This allocates a block of memory aligned to the given size */
unsigned long DynamicMMap::RawAllocate(unsigned long Size,unsigned long Aln)
{
   unsigned long Result = iSize;
   if (Aln != 0)
      Result += Aln - (iSize%Aln);
   
   iSize = Result + Size;
   
   // Just in case error check
   if (Result + Size > WorkSpace)
   {
      _error->Error("Dynamic MMap ran out of room");
      return 0;
   }
   return Result;
}
									/*}}}*/
// DynamicMMap::Allocate - Pooled aligned allocation			/*{{{*/
// ---------------------------------------------------------------------
/* This allocates an Item of size ItemSize so that it is aligned to its
   size in the file. */
unsigned long DynamicMMap::Allocate(unsigned long ItemSize)
{   
   // Look for a matching pool entry
   Pool *I;
   Pool *Empty = 0;
   for (I = Pools; I != Pools + PoolCount; I++)
   {
      if (I->ItemSize == 0)
	 Empty = I;
      if (I->ItemSize == ItemSize)
	 break;
   }

   // No pool is allocated, use an unallocated one
   if (I == Pools + PoolCount)
   {
      // Woops, we ran out, the calling code should allocate more.
      if (Empty == 0)
      {
	 _error->Error("Ran out of allocation pools");
	 return 0;
      }
      
      I = Empty;
      I->ItemSize = ItemSize;
      I->Count = 0;
   }
   
   // Out of space, allocate some more
   if (I->Count == 0)
   {
      I->Count = 20*1024/ItemSize;
      I->Start = RawAllocate(I->Count*ItemSize,ItemSize);
   }   

   I->Count--;
   unsigned long Result = I->Start;
   I->Start += ItemSize;
   return Result/ItemSize;
}
									/*}}}*/
// DynamicMMap::WriteString - Write a string to the file		/*{{{*/
// ---------------------------------------------------------------------
/* Strings are not aligned to anything */
unsigned long DynamicMMap::WriteString(const char *String,
				       unsigned long Len)
{
   unsigned long Result = iSize;
   // Just in case error check
   if (Result > WorkSpace)
   {
      _error->Error("Dynamic MMap ran out of room");
      return 0;
   }   
   
   if (Len == 0)
      Len = strlen(String);
   iSize += Len + 1;
   memcpy((char *)Base + Result,String,Len);
   ((char *)Base)[Result + Len] = 0;
   return Result;
}
									/*}}}*/

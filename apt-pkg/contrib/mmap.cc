// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: mmap.cc,v 1.22 2001/05/27 05:19:30 jgg Exp $
/* ######################################################################
   
   MMap Class - Provides 'real' mmap or a faked mmap using read().

   MMap cover class.

   Some broken versions of glibc2 (libc6) have a broken definition
   of mmap that accepts a char * -- all other systems (and libc5) use
   void *. We can't safely do anything here that would be portable, so
   libc6 generates warnings -- which should be errors, g++ isn't properly
   strict.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#define _BSD_SOURCE
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>

#include <apti18n.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <cstring>
   									/*}}}*/

// MMap::MMap - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::MMap(FileFd &F,unsigned long Flags) : Flags(Flags), iSize(0),
                     Base(0)
{
   if ((Flags & NoImmMap) != NoImmMap)
      Map(F);
}
									/*}}}*/
// MMap::MMap - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::MMap(unsigned long Flags) : Flags(Flags), iSize(0),
                     Base(0)
{
}
									/*}}}*/
// MMap::~MMap - Destructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::~MMap()
{
   Close();
}
									/*}}}*/
// MMap::Map - Perform the mapping					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Map(FileFd &Fd)
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
      return _error->Error(_("Can't mmap an empty file"));
   
   // Map it.
   Base = mmap(0,iSize,Prot,Map,Fd.Fd(),0);
   if (Base == (void *)-1)
      return _error->Errno("mmap",_("Couldn't make mmap of %lu bytes"),iSize);

   return true;
}
									/*}}}*/
// MMap::Close - Close the map						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Close(bool DoSync)
{
   if ((Flags & UnMapped) == UnMapped || Base == 0 || iSize == 0)
      return true;
   
   if (DoSync == true)
      Sync();
   
   if (munmap((char *)Base,iSize) != 0)
      _error->Warning("Unable to munmap");
   
   iSize = 0;
   Base = 0;
   return true;
}
									/*}}}*/
// MMap::Sync - Syncronize the map with the disk			/*{{{*/
// ---------------------------------------------------------------------
/* This is done in syncronous mode - the docs indicate that this will 
   not return till all IO is complete */
bool MMap::Sync()
{   
   if ((Flags & UnMapped) == UnMapped)
      return true;
   
#ifdef _POSIX_SYNCHRONIZED_IO   
   if ((Flags & ReadOnly) != ReadOnly)
      if (msync((char *)Base,iSize,MS_SYNC) < 0)
	 return _error->Errno("msync","Unable to write mmap");
#endif   
   return true;
}
									/*}}}*/
// MMap::Sync - Syncronize a section of the file to disk		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Sync(unsigned long Start,unsigned long Stop)
{
   if ((Flags & UnMapped) == UnMapped)
      return true;
   
#ifdef _POSIX_SYNCHRONIZED_IO
   unsigned long PSize = sysconf(_SC_PAGESIZE);
   if ((Flags & ReadOnly) != ReadOnly)
      if (msync((char *)Base+(int)(Start/PSize)*PSize,Stop - Start,MS_SYNC) < 0)
	 return _error->Errno("msync","Unable to write mmap");
#endif   
   return true;
}
									/*}}}*/

// DynamicMMap::DynamicMMap - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
DynamicMMap::DynamicMMap(FileFd &F,unsigned long Flags,unsigned long WorkSpace) :
             MMap(F,Flags | NoImmMap), Fd(&F), WorkSpace(WorkSpace)
{
   if (_error->PendingError() == true)
      return;
   
   unsigned long EndOfFile = Fd->Size();
   if (EndOfFile > WorkSpace)
      WorkSpace = EndOfFile;
   else if(WorkSpace > 0)
   {
      Fd->Seek(WorkSpace - 1);
      char C = 0;
      Fd->Write(&C,sizeof(C));
   }
   
   Map(F);
   iSize = EndOfFile;
}
									/*}}}*/
// DynamicMMap::DynamicMMap - Constructor for a non-file backed map	/*{{{*/
// ---------------------------------------------------------------------
/* We try here to use mmap to reserve some space - this is much more
   cooler than the fallback solution to simply allocate a char array
   and could come in handy later than we are able to grow such an mmap */
DynamicMMap::DynamicMMap(unsigned long Flags,unsigned long WorkSpace) :
             MMap(Flags | NoImmMap | UnMapped), Fd(0), WorkSpace(WorkSpace)
{
   if (_error->PendingError() == true)
      return;

#ifdef _POSIX_MAPPED_FILES
   // Set the permissions.
   int Prot = PROT_READ;
   int Map = MAP_PRIVATE | MAP_ANONYMOUS;
   if ((Flags & ReadOnly) != ReadOnly)
      Prot |= PROT_WRITE;
   if ((Flags & Public) == Public)
      Map = MAP_SHARED | MAP_ANONYMOUS;

   // use anonymous mmap() to get the memory
   Base = (unsigned char*) mmap(0, WorkSpace, Prot, Map, -1, 0);

   if(Base == MAP_FAILED)
      _error->Errno("DynamicMMap",_("Couldn't make mmap of %lu bytes"),WorkSpace);
#else
   // fallback to a static allocated space
   Base = new unsigned char[WorkSpace];
   memset(Base,0,WorkSpace);
#endif
   iSize = 0;
}
									/*}}}*/
// DynamicMMap::~DynamicMMap - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* We truncate the file to the size of the memory data set */
DynamicMMap::~DynamicMMap()
{
   if (Fd == 0)
   {
#ifdef _POSIX_MAPPED_FILES
      munmap(Base, WorkSpace);
#else
      delete [] (unsigned char *)Base;
#endif
      return;
   }
   
   unsigned long EndOfFile = iSize;
   iSize = WorkSpace;
   Close(false);
   if(ftruncate(Fd->Fd(),EndOfFile) < 0)
      _error->Errno("ftruncate", _("Failed to truncate file"));
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

   // try to grow the buffer
   while(Result + Size > WorkSpace)
   {
      if(!Grow())
      {
	 _error->Error(_("Dynamic MMap ran out of room. Please increase the size "
			 "of APT::Cache-Limit. Current value: %lu. (man 5 apt.conf)"), WorkSpace);
	 return 0;
      }
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

   unsigned long Result = 0;
   // Out of space, allocate some more
   if (I->Count == 0)
   {
      const unsigned long size = 20*1024;
      I->Count = size/ItemSize;
      Result = RawAllocate(size,ItemSize);
      // Does the allocation failed ?
      if (Result == 0 && _error->PendingError())
	 return 0;
      I->Start = Result;
   }
   else
      Result = I->Start;

   I->Count--;
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
   if (Len == (unsigned long)-1)
      Len = strlen(String);

   unsigned long Result = RawAllocate(Len+1,0);

   if (Result == 0 && _error->PendingError())
      return 0;

   memcpy((char *)Base + Result,String,Len);
   ((char *)Base)[Result + Len] = 0;
   return Result;
}
									/*}}}*/
// DynamicMMap::Grow - Grow the mmap					/*{{{*/
// ---------------------------------------------------------------------
/* This method will try to grow the mmap we currently use. This doesn't
   work most of the time because we can't move the mmap around in the
   memory for now as this would require to adjust quite a lot of pointers
   but why we should not at least try to grow it before we give up? */
bool DynamicMMap::Grow()
{
#if defined(_POSIX_MAPPED_FILES) && defined(__linux__)
   unsigned long newSize = WorkSpace + 1024*1024;

   if(Fd != 0)
   {
      Fd->Seek(newSize - 1);
      char C = 0;
      Fd->Write(&C,sizeof(C));
   }

   Base = mremap(Base, WorkSpace, newSize, 0);
   if(Base == MAP_FAILED)
      return false;

   WorkSpace = newSize;
   return true;
#else
   return false;
#endif
}
									/*}}}*/

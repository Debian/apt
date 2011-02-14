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
#include <errno.h>

#include <cstring>
   									/*}}}*/

// MMap::MMap - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::MMap(FileFd &F,unsigned long Flags) : Flags(Flags), iSize(0),
                     Base(0), SyncToFd(NULL)
{
   if ((Flags & NoImmMap) != NoImmMap)
      Map(F);
}
									/*}}}*/
// MMap::MMap - Constructor						/*{{{*/
// ---------------------------------------------------------------------
/* */
MMap::MMap(unsigned long Flags) : Flags(Flags), iSize(0),
                     Base(0), SyncToFd(NULL)
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
   {
      if (errno == ENODEV || errno == EINVAL)
      {
	 // The filesystem doesn't support this particular kind of mmap.
	 // So we allocate a buffer and read the whole file into it.
	 int const dupped_fd = dup(Fd.Fd());
	 if (dupped_fd == -1)
	    return _error->Errno("mmap", _("Couldn't duplicate file descriptor %i"), Fd.Fd());

	 Base = new unsigned char[iSize];
	 SyncToFd = new FileFd (dupped_fd);
	 if (!SyncToFd->Seek(0L) || !SyncToFd->Read(Base, iSize))
	    return false;
      }
      else
	 return _error->Errno("mmap",_("Couldn't make mmap of %lu bytes"),
	                      iSize);
     }

   return true;
}
									/*}}}*/
// MMap::Close - Close the map						/*{{{*/
// ---------------------------------------------------------------------
/* */
bool MMap::Close(bool DoSync)
{
   if ((Flags & UnMapped) == UnMapped || validData() == false || iSize == 0)
      return true;
   
   if (DoSync == true)
      Sync();

   if (SyncToFd != NULL)
   {
      delete[] (char *)Base;
      delete SyncToFd;
      SyncToFd = NULL;
   }
   else
   {
      if (munmap((char *)Base, iSize) != 0)
	 _error->WarningE("mmap", _("Unable to close mmap"));
   }

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
   {
      if (SyncToFd != NULL)
      {
	 if (!SyncToFd->Seek(0) || !SyncToFd->Write(Base, iSize))
	    return false;
      }
      else
      {
	 if (msync((char *)Base, iSize, MS_SYNC) < 0)
	    return _error->Errno("msync", _("Unable to synchronize mmap"));
      }
   }
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
   {
      if (SyncToFd != 0)
      {
	 if (!SyncToFd->Seek(0) ||
	     !SyncToFd->Write (((char *)Base)+Start, Stop-Start))
	    return false;
      }
      else
      {
	 if (msync((char *)Base+(int)(Start/PSize)*PSize,Stop - Start,MS_SYNC) < 0)
	    return _error->Errno("msync", _("Unable to synchronize mmap"));
      }
   }
#endif   
   return true;
}
									/*}}}*/

// DynamicMMap::DynamicMMap - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
DynamicMMap::DynamicMMap(FileFd &F,unsigned long Flags,unsigned long const &Workspace,
			 unsigned long const &Grow, unsigned long const &Limit) :
		MMap(F,Flags | NoImmMap), Fd(&F), WorkSpace(Workspace),
		GrowFactor(Grow), Limit(Limit)
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
DynamicMMap::DynamicMMap(unsigned long Flags,unsigned long const &WorkSpace,
			 unsigned long const &Grow, unsigned long const &Limit) :
		MMap(Flags | NoImmMap | UnMapped), Fd(0), WorkSpace(WorkSpace),
		GrowFactor(Grow), Limit(Limit)
{
	if (_error->PendingError() == true)
		return;

	// disable Moveable if we don't grow
	if (Grow == 0)
		this->Flags &= ~Moveable;

#ifndef __linux__
	// kfreebsd doesn't have mremap, so we use the fallback
	if ((this->Flags & Moveable) == Moveable)
		this->Flags |= Fallback;
#endif

#ifdef _POSIX_MAPPED_FILES
	if ((this->Flags & Fallback) != Fallback) {
		// Set the permissions.
		int Prot = PROT_READ;
		int Map = MAP_PRIVATE | MAP_ANONYMOUS;
		if ((this->Flags & ReadOnly) != ReadOnly)
			Prot |= PROT_WRITE;
		if ((this->Flags & Public) == Public)
			Map = MAP_SHARED | MAP_ANONYMOUS;

		// use anonymous mmap() to get the memory
		Base = (unsigned char*) mmap(0, WorkSpace, Prot, Map, -1, 0);

		if(Base == MAP_FAILED)
			_error->Errno("DynamicMMap",_("Couldn't make mmap of %lu bytes"),WorkSpace);

		iSize = 0;
		return;
	}
#endif
	// fallback to a static allocated space
	Base = new unsigned char[WorkSpace];
	memset(Base,0,WorkSpace);
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
      if (validData() == false)
	 return;
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
	 _error->Fatal(_("Dynamic MMap ran out of room. Please increase the size "
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
   for (I = Pools; I != Pools + PoolCount; ++I)
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
      Pool* oldPools = Pools;
      Result = RawAllocate(size,ItemSize);
      if (Pools != oldPools)
	 I += Pools - oldPools;

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

   unsigned long const Result = RawAllocate(Len+1,0);

   if (Result == 0 && _error->PendingError())
      return 0;

   memcpy((char *)Base + Result,String,Len);
   ((char *)Base)[Result + Len] = 0;
   return Result;
}
									/*}}}*/
// DynamicMMap::Grow - Grow the mmap					/*{{{*/
// ---------------------------------------------------------------------
/* This method is a wrapper around different methods to (try to) grow
   a mmap (or our char[]-fallback). Encounterable environments:
   1. Moveable + !Fallback + linux -> mremap with MREMAP_MAYMOVE
   2. Moveable + !Fallback + !linux -> not possible (forbidden by constructor)
   3. Moveable + Fallback -> realloc
   4. !Moveable + !Fallback + linux -> mremap alone - which will fail in 99,9%
   5. !Moveable + !Fallback + !linux -> not possible (forbidden by constructor)
   6. !Moveable + Fallback -> not possible
   [ While Moveable and Fallback stands for the equally named flags and
     "linux" indicates a linux kernel instead of a freebsd kernel. ]
   So what you can see here is, that a MMAP which want to be growable need
   to be moveable to have a real chance but that this method will at least try
   the nearly impossible 4 to grow it before it finally give up: Never say never. */
bool DynamicMMap::Grow() {
	if (Limit != 0 && WorkSpace >= Limit)
		return _error->Error(_("Unable to increase the size of the MMap as the "
		                       "limit of %lu bytes is already reached."), Limit);
	if (GrowFactor <= 0)
		return _error->Error(_("Unable to increase size of the MMap as automatic growing is disabled by user."));

	unsigned long const newSize = WorkSpace + GrowFactor;

	if(Fd != 0) {
		Fd->Seek(newSize - 1);
		char C = 0;
		Fd->Write(&C,sizeof(C));
	}

	unsigned long const poolOffset = Pools - ((Pool*) Base);

	if ((Flags & Fallback) != Fallback) {
#if defined(_POSIX_MAPPED_FILES) && defined(__linux__)
   #ifdef MREMAP_MAYMOVE

		if ((Flags & Moveable) == Moveable)
			Base = mremap(Base, WorkSpace, newSize, MREMAP_MAYMOVE);
		else
   #endif
			Base = mremap(Base, WorkSpace, newSize, 0);

		if(Base == MAP_FAILED)
			return false;
#else
		return false;
#endif
	} else {
		if ((Flags & Moveable) != Moveable)
			return false;

		Base = realloc(Base, newSize);
		if (Base == NULL)
			return false;
	}

	Pools =(Pool*) Base + poolOffset;
	WorkSpace = newSize;
	return true;
}
									/*}}}*/

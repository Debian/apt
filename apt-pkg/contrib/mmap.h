// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: mmap.h,v 1.12 2001/05/14 05:16:43 jgg Exp $
/* ######################################################################
   
   MMap Class - Provides 'real' mmap or a faked mmap using read().

   The purpose of this code is to provide a generic way for clients to
   access the mmap function. In enviroments that do not support mmap
   from file fd's this function will use read and normal allocated 
   memory.
   
   Writing to a public mmap will always fully comit all changes when the 
   class is deleted. Ie it will rewrite the file, unless it is readonly

   The DynamicMMap class is used to help the on-disk data structure 
   generators. It provides a large allocated workspace and members
   to allocate space from the workspace in an effecient fashion.
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_MMAP_H
#define PKGLIB_MMAP_H


#include <string>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/fileutl.h>
using std::string;
#endif

class FileFd;

/* This should be a 32 bit type, larger tyes use too much ram and smaller
   types are too small. Where ever possible 'unsigned long' should be used
   instead of this internal type */
typedef unsigned int map_ptrloc;

class MMap
{
   protected:
   
   unsigned long Flags;
   unsigned long long iSize;
   void *Base;

   // In case mmap can not be used, we keep a dup of the file
   // descriptor that should have been mmaped so that we can write to
   // the file in Sync().
   FileFd *SyncToFd;

   bool Map(FileFd &Fd);
   bool Close(bool DoSync = true);
   
   public:

   enum OpenFlags {NoImmMap = (1<<0),Public = (1<<1),ReadOnly = (1<<2),
                   UnMapped = (1<<3), Moveable = (1<<4), Fallback = (1 << 5)};
      
   // Simple accessors
   inline operator void *() {return Base;};
   inline void *Data() {return Base;}; 
   inline unsigned long long Size() {return iSize;};
   inline void AddSize(unsigned long long const size) {iSize += size;};
   inline bool validData() const { return Base != (void *)-1 && Base != 0; };
   
   // File manipulators
   bool Sync();
   bool Sync(unsigned long Start,unsigned long Stop);
   
   MMap(FileFd &F,unsigned long Flags);
   MMap(unsigned long Flags);
   virtual ~MMap();
};

class DynamicMMap : public MMap
{
   public:
   
   // This is the allocation pool structure
   struct Pool
   {
      unsigned long ItemSize;
      unsigned long Start;
      unsigned long Count;
   };
   
   protected:
   
   FileFd *Fd;
   unsigned long WorkSpace;
   unsigned long const GrowFactor;
   unsigned long const Limit;
   Pool *Pools;
   unsigned int PoolCount;

   bool Grow();
   
   public:

   // Allocation
   unsigned long RawAllocate(unsigned long long Size,unsigned long Aln = 0);
   unsigned long Allocate(unsigned long ItemSize);
   unsigned long WriteString(const char *String,unsigned long Len = (unsigned long)-1);
   inline unsigned long WriteString(const std::string &S) {return WriteString(S.c_str(),S.length());};
   void UsePools(Pool &P,unsigned int Count) {Pools = &P; PoolCount = Count;};
   
   DynamicMMap(FileFd &F,unsigned long Flags,unsigned long const &WorkSpace = 2*1024*1024,
	       unsigned long const &Grow = 1024*1024, unsigned long const &Limit = 0);
   DynamicMMap(unsigned long Flags,unsigned long const &WorkSpace = 2*1024*1024,
	       unsigned long const &Grow = 1024*1024, unsigned long const &Limit = 0);
   virtual ~DynamicMMap();
};

#endif

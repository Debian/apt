// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: mmap.h,v 1.6 1998/07/19 04:42:15 jgg Exp $
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
// Header section: pkglib
#ifndef PKGLIB_MMAP_H
#define PKGLIB_MMAP_H

#ifdef __GNUG__
#pragma interface "apt-pkg/mmap.h"
#endif

#include <string>
#include <apt-pkg/fileutl.h>

class MMap
{
   protected:
   
   FileFd &Fd;
   unsigned long Flags;   
   unsigned long iSize;
   void *Base;

   bool Map();
   bool Close(bool DoClose = true,bool DoSync = true);
   
   public:

   enum OpenFlags {NoImmMap = (1<<0),Public = (1<<1),ReadOnly = (1<<2)};
      
   // Simple accessors
   inline operator void *() {return Base;};
   inline void *Data() {return Base;}; 
   inline unsigned long Size() {return iSize;};
   
   // File manipulators
   bool Sync();
   bool Sync(unsigned long Start,unsigned long Stop);
   
   MMap(FileFd &F,unsigned long Flags);
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
   
   unsigned long WorkSpace;
   Pool *Pools;
   unsigned int PoolCount;
   
   public:

   // Allocation
   unsigned long RawAllocate(unsigned long Size,unsigned long Aln = 0);
   unsigned long Allocate(unsigned long ItemSize);
   unsigned long WriteString(const char *String,unsigned long Len = 0);
   inline unsigned long WriteString(string S) {return WriteString(S.begin(),S.size());};
   void UsePools(Pool &P,unsigned int Count) {Pools = &P; PoolCount = Count;}; 
   
   DynamicMMap(FileFd &F,unsigned long Flags,unsigned long WorkSpace = 1024*1024);
   virtual ~DynamicMMap();
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.h,v 1.7 1998/11/11 06:54:14 jgg Exp $
/* ######################################################################

   Acquire Item - Item to acquire

   When an item is instantiated it will add it self to the local list in
   the Owner Acquire class. Derived classes will then call QueueURI to 
   register all the URI's they wish to fetch for at the initial moment.   
   
   Two item classes are provided to provide functionality for downloading
   of Index files and downloading of Packages.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_ITEM_H
#define PKGLIB_ACQUIRE_ITEM_H

#include <apt-pkg/acquire.h>
#include <apt-pkg/sourcelist.h>

#ifdef __GNUG__
#pragma interface "apt-pkg/acquire-item.h"
#endif 

// Item to acquire
class pkgAcquire::Item
{  
   protected:
   
   pkgAcquire *Owner;
   inline void QueueURI(ItemDesc &Item)
                 {Owner->Enqueue(Item);};
   
   void Rename(string From,string To);
   
   public:

   // State of the item
   enum {StatIdle, StatFetching, StatDone, StatError} Status;
   string ErrorText;
   unsigned long FileSize;
   char *Mode;
   unsigned long ID;
   bool Complete;
   
   // Number of queues we are inserted into
   unsigned int QueueCounter;
   
   // File to write the fetch into
   string DestFile;
   
   virtual void Failed(string Message);
   virtual void Done(string Message,unsigned long Size,string Md5Hash);
   virtual void Start(string Message,unsigned long Size);

   virtual string Custom600Headers() {return string();};
      
   Item(pkgAcquire *Owner);
   virtual ~Item();
};

// Item class for index files
class pkgAcqIndex : public pkgAcquire::Item
{
   protected:
   
   const pkgSourceList::Item *Location;
   bool Decompression;
   bool Erase;
   pkgAcquire::ItemDesc Desc;
   
   public:
   
   virtual void Done(string Message,unsigned long Size,string Md5Hash);   
   virtual string Custom600Headers();

   pkgAcqIndex(pkgAcquire *Owner,const pkgSourceList::Item *Location);
};

// Item class for index files
class pkgAcqIndexRel : public pkgAcquire::Item
{
   protected:
   
   const pkgSourceList::Item *Location;
   pkgAcquire::ItemDesc Desc;
   
   public:
   
   virtual void Done(string Message,unsigned long Size,string Md5Hash);   
   virtual string Custom600Headers();
   
   pkgAcqIndexRel(pkgAcquire *Owner,const pkgSourceList::Item *Location);
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-item.h,v 1.3 1998/10/24 04:57:57 jgg Exp $
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
   inline void QueueURI(string URI,string Description) 
                 {Owner->Enqueue(this,URI,Description);};
   
   public:

   // State of the item
   enum {StatIdle, StatFetching, StatDone, StatError} Status;
   string ErrorText;
   
   // Number of queues we are inserted into
   unsigned int QueueCounter;
   
   // File to write the fetch into
   string DestFile;
   
   virtual void Failed(string Message);
   virtual void Done(string Message,unsigned long Size,string Md5Hash);

   virtual string Custom600Headers() {return string();};
      
   Item(pkgAcquire *Owner);
   virtual ~Item();
};

// Item class for index files
class pkgAcqIndex : public pkgAcquire::Item
{
   protected:
   
   const pkgSourceList::Item *Location;
   
   public:
   
   virtual string Custom600Headers();

   pkgAcqIndex(pkgAcquire *Owner,const pkgSourceList::Item *Location);
};

// Item class for index files
class pkgAcqIndexRel : public pkgAcquire::Item
{
   protected:
   
   const pkgSourceList::Item *Location;
   
   public:
   
   virtual void Done(string Message,unsigned long Size,string Md5Hash);
   
   virtual string Custom600Headers();
   
   pkgAcqIndexRel(pkgAcquire *Owner,const pkgSourceList::Item *Location);
};

#endif

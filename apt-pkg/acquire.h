// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.h,v 1.29.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Acquire - File Acquiration
   
   This module contians the Acquire system. It is responsible for bringing
   files into the local pathname space. It deals with URIs for files and
   URI handlers responsible for downloading or finding the URIs.
   
   Each file to download is represented by an Acquire::Item class subclassed
   into a specialization. The Item class can add itself to several URI
   acquire queues each prioritized by the download scheduler. When the 
   system is run the proper URI handlers are spawned and the the acquire 
   queues are fed into the handlers by the schedular until the queues are
   empty. This allows for an Item to be downloaded from an alternate source
   if the first try turns out to fail. It also alows concurrent downloading
   of multiple items from multiple sources as well as dynamic balancing
   of load between the sources.
   
   Schedualing of downloads is done on a first ask first get basis. This
   preserves the order of the download as much as possible. And means the
   fastest source will tend to process the largest number of files.
   
   Internal methods and queues for performing gzip decompression,
   md5sum hashing and file copying are provided to allow items to apply
   a number of transformations to the data files they are working with.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_H
#define PKGLIB_ACQUIRE_H

#include <vector>
#include <string>

using std::vector;
using std::string;

#ifdef __GNUG__
#pragma interface "apt-pkg/acquire.h"
#endif 

#include <sys/time.h>
#include <unistd.h>

class pkgAcquireStatus;
class pkgAcquire
{   
   public:
   
   class Item;
   class Queue;
   class Worker;
   struct MethodConfig;
   struct ItemDesc;
   friend class Item;
   friend class Queue;

   typedef vector<Item *>::iterator ItemIterator;
   typedef vector<Item *>::const_iterator ItemCIterator;
   
   protected:
   
   // List of items to fetch
   vector<Item *> Items;
   
   // List of active queues and fetched method configuration parameters
   Queue *Queues;
   Worker *Workers;
   MethodConfig *Configs;
   pkgAcquireStatus *Log;
   unsigned long ToFetch;

   // Configurable parameters for the schedular
   enum {QueueHost,QueueAccess} QueueMode;
   bool Debug;
   bool Running;
   
   void Add(Item *Item);
   void Remove(Item *Item);
   void Add(Worker *Work);
   void Remove(Worker *Work);
   
   void Enqueue(ItemDesc &Item);
   void Dequeue(Item *Item);
   string QueueName(string URI,MethodConfig const *&Config);

   // FDSET managers for derived classes
   virtual void SetFds(int &Fd,fd_set *RSet,fd_set *WSet);
   virtual void RunFds(fd_set *RSet,fd_set *WSet);   

   // A queue calls this when it dequeues an item
   void Bump();
   
   public:

   MethodConfig *GetConfig(string Access);

   enum RunResult {Continue,Failed,Cancelled};

   RunResult Run(int PulseIntervall=500000);
   void Shutdown();
   
   // Simple iteration mechanism
   inline Worker *WorkersBegin() {return Workers;};
   Worker *WorkerStep(Worker *I);
   inline ItemIterator ItemsBegin() {return Items.begin();};
   inline ItemIterator ItemsEnd() {return Items.end();};
   
   // Iterate over queued Item URIs
   class UriIterator;
   UriIterator UriBegin();
   UriIterator UriEnd();
   
   // Cleans out the download dir
   bool Clean(string Dir);

   // Returns the size of the total download set
   double TotalNeeded();
   double FetchNeeded();
   double PartialPresent();

   pkgAcquire(pkgAcquireStatus *Log = 0);
   virtual ~pkgAcquire();
};

// Description of an Item+URI
struct pkgAcquire::ItemDesc
{
   string URI;
   string Description;
   string ShortDesc;
   Item *Owner;
};

// List of possible items queued for download.
class pkgAcquire::Queue
{
   friend class pkgAcquire;
   friend class pkgAcquire::UriIterator;
   friend class pkgAcquire::Worker;
   Queue *Next;
   
   protected:

   // Queued item
   struct QItem : pkgAcquire::ItemDesc
   {
      QItem *Next;      
      pkgAcquire::Worker *Worker;
      
      void operator =(pkgAcquire::ItemDesc const &I)
      {
	 URI = I.URI;
	 Description = I.Description;
	 ShortDesc = I.ShortDesc;
	 Owner = I.Owner;
      };
   };
   
   // Name of the queue
   string Name;

   // Items queued into this queue
   QItem *Items;
   pkgAcquire::Worker *Workers;
   pkgAcquire *Owner;
   signed long PipeDepth;
   unsigned long MaxPipeDepth;
   
   public:
   
   // Put an item into this queue
   void Enqueue(ItemDesc &Item);
   bool Dequeue(Item *Owner);

   // Find a Queued item
   QItem *FindItem(string URI,pkgAcquire::Worker *Owner);
   bool ItemStart(QItem *Itm,unsigned long Size);
   bool ItemDone(QItem *Itm);
   
   bool Startup();
   bool Shutdown(bool Final);
   bool Cycle();
   void Bump();
   
   Queue(string Name,pkgAcquire *Owner);
   ~Queue();
};

class pkgAcquire::UriIterator
{
   pkgAcquire::Queue *CurQ;
   pkgAcquire::Queue::QItem *CurItem;
   
   public:
   
   // Advance to the next item
   inline void operator ++() {operator ++();};
   void operator ++(int)
   {
      CurItem = CurItem->Next;
      while (CurItem == 0 && CurQ != 0)
      {
	 CurItem = CurQ->Items;
	 CurQ = CurQ->Next;
      }
   };
   
   // Accessors
   inline pkgAcquire::ItemDesc const *operator ->() const {return CurItem;};
   inline bool operator !=(UriIterator const &rhs) const {return rhs.CurQ != CurQ || rhs.CurItem != CurItem;};
   inline bool operator ==(UriIterator const &rhs) const {return rhs.CurQ == CurQ && rhs.CurItem == CurItem;};
   
   UriIterator(pkgAcquire::Queue *Q) : CurQ(Q), CurItem(0)
   {
      while (CurItem == 0 && CurQ != 0)
      {
	 CurItem = CurQ->Items;
	 CurQ = CurQ->Next;
      }
   }   
};

// Configuration information from each method
struct pkgAcquire::MethodConfig
{
   MethodConfig *Next;
   
   string Access;

   string Version;
   bool SingleInstance;
   bool Pipeline;
   bool SendConfig;
   bool LocalOnly;
   bool NeedsCleanup;
   bool Removable;
   
   MethodConfig();
};

class pkgAcquireStatus
{
   protected:
   
   struct timeval Time;
   struct timeval StartTime;
   double LastBytes;
   double CurrentCPS;
   double CurrentBytes;
   double TotalBytes;
   double FetchedBytes;
   unsigned long ElapsedTime;
   unsigned long TotalItems;
   unsigned long CurrentItems;
   
   public:

   bool Update;
   bool MorePulses;
      
   // Called by items when they have finished a real download
   virtual void Fetched(unsigned long Size,unsigned long ResumePoint);
   
   // Called to change media
   virtual bool MediaChange(string Media,string Drive) = 0;
   
   // Each of these is called by the workers when an event occures
   virtual void IMSHit(pkgAcquire::ItemDesc &/*Itm*/) {};
   virtual void Fetch(pkgAcquire::ItemDesc &/*Itm*/) {};
   virtual void Done(pkgAcquire::ItemDesc &/*Itm*/) {};
   virtual void Fail(pkgAcquire::ItemDesc &/*Itm*/) {};
   virtual bool Pulse(pkgAcquire *Owner); // returns false on user cancel
   virtual void Start();
   virtual void Stop();
   
   pkgAcquireStatus();
   virtual ~pkgAcquireStatus() {};
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.h,v 1.10 1998/11/11 06:54:17 jgg Exp $
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
   friend Item;
   friend Queue;
   
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
   string QueueName(string URI);

   // FDSET managers for derived classes
   void SetFds(int &Fd,fd_set *RSet,fd_set *WSet);
   void RunFds(fd_set *RSet,fd_set *WSet);   

   // A queue calls this when it dequeues an item
   void Bump();
   
   public:

   MethodConfig *GetConfig(string Access);
   bool Run();

   // Simple iteration mechanism
   inline Worker *WorkersBegin() {return Workers;};
   Worker *WorkerStep(Worker *I);
   inline Item **ItemsBegin() {return Items.begin();};
   inline Item **ItemsEnd() {return Items.end();};
   
   pkgAcquire(pkgAcquireStatus *Log = 0);
   ~pkgAcquire();
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
   friend pkgAcquire;
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
   
   public:
   
   // Put an item into this queue
   void Enqueue(ItemDesc &Item);
   bool Dequeue(Item *Owner);

   // Find a Queued item
   QItem *FindItem(string URI,pkgAcquire::Worker *Owner);
   bool ItemStart(QItem *Itm,unsigned long Size);
   bool ItemDone(QItem *Itm);
   
   bool Startup();
   bool Shutdown();
   bool Cycle();
   void Bump();
   
   Queue(string Name,pkgAcquire *Owner);
   ~Queue();
};

// Configuration information from each method
struct pkgAcquire::MethodConfig
{
   MethodConfig *Next;
   
   string Access;

   string Version;
   bool SingleInstance;
   bool PreScan;
   bool Pipeline;
   bool SendConfig;
   
   MethodConfig();
};

class pkgAcquireStatus
{
   protected:
   
   struct timeval Time;
   struct timeval StartTime;
   unsigned long LastBytes;
   double CurrentCPS;
   unsigned long CurrentBytes;
   unsigned long TotalBytes;
   unsigned long FetchedBytes;
   unsigned long ElapsedTime;
   
   public:

   bool Update;
   
   // Called by items when they have finished a real download
   virtual void Fetched(unsigned long Size,unsigned long ResumePoint);
   
   // Each of these is called by the workers when an event occures
   virtual void IMSHit(pkgAcquire::ItemDesc &Itm) {};
   virtual void Fetch(pkgAcquire::ItemDesc &Itm) {};
   virtual void Done(pkgAcquire::ItemDesc &Itm) {};
   virtual void Fail(pkgAcquire::ItemDesc &Itm) {};   
   virtual void Pulse(pkgAcquire *Owner);
   virtual void Start();
   virtual void Stop();

   pkgAcquireStatus();
   virtual ~pkgAcquireStatus() {};
};

#endif

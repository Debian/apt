// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.h,v 1.7 1998/11/01 05:27:35 jgg Exp $
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

#include <unistd.h>

class pkgAcquire
{   
   public:
   
   class Item;
   class Queue;
   class Worker;
   struct MethodConfig;
   friend Item;
   friend Queue;
   
   protected:
   
   // List of items to fetch
   vector<Item *> Items;
   
   // List of active queues and fetched method configuration parameters
   Queue *Queues;
   Worker *Workers;
   MethodConfig *Configs;
   unsigned long ToFetch;
   
   // Configurable parameters for the schedular
   enum {QueueHost,QueueAccess} QueueMode;
   bool Debug;
   bool Running;
   
   void Add(Item *Item);
   void Remove(Item *Item);
   void Add(Worker *Work);
   void Remove(Worker *Work);
   
   void Enqueue(Item *Item,string URI,string Description);
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
   
   pkgAcquire();
   ~pkgAcquire();
};

// List of possible items queued for download.
class pkgAcquire::Queue
{
   friend pkgAcquire;
   Queue *Next;
   
   protected:

   // Queued item
   struct QItem 
   {
      QItem *Next;
      
      string URI;
      string Description;
      Item *Owner;
      pkgAcquire::Worker *Worker;
   };   
   
   // Name of the queue
   string Name;

   // Items queued into this queue
   QItem *Items;
   pkgAcquire::Worker *Workers;
   pkgAcquire *Owner;
   
   public:
   
   // Put an item into this queue
   void Enqueue(Item *Owner,string URI,string Description);
   void Dequeue(Item *Owner);

   // Find a Queued item
   QItem *FindItem(string URI,pkgAcquire::Worker *Owner);
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

#endif

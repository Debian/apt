// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire.h,v 1.2 1998/10/20 02:39:16 jgg Exp $
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

class pkgAcquire
{   
   public:
   
   class Item;
   class Queue;
   class Worker;
   struct MethodConfig;
   friend Item;
   
   protected:
   
   vector<Item *> Items;
   Queue *Queues;
   MethodConfig *Configs;
   
   void Add(Item *Item);
   void Remove(Item *Item);
   void Enqueue(Item *Item,string URI);
   
   public:

   const MethodConfig *GetConfig(string Access);
   string QueueName(string URI);
   
   pkgAcquire();
   ~pkgAcquire();
};

// List of possible items queued for download.
class pkgAcquire::Queue
{
   friend pkgAcquire;
   Queue *Next;
   
   protected:
   
   string URIMatch;

   vector<Item *> Items;
   
   public:
};

// Configuration information from each method
struct pkgAcquire::MethodConfig
{
   MethodConfig *Next;
   
   string Access;

   string Version;
   bool SingleInstance;
   bool PreScan;
   
   MethodConfig();
};

#endif

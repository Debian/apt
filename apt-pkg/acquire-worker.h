// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-worker.h,v 1.3 1998/10/22 04:56:42 jgg Exp $
/* ######################################################################

   Acquire Worker - Worker process manager
   
   Each worker class is associated with exaclty one subprocess.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_WORKER_H
#define PKGLIB_ACQUIRE_WORKER_H

#include <apt-pkg/acquire.h>

#ifdef __GNUG__
#pragma interface "apt-pkg/acquire-worker.h"
#endif 

// Interfacing to the method process
class pkgAcquire::Worker
{
   friend pkgAcquire;
   
   protected:
   friend Queue;

   /* Linked list starting at a Queue and a linked list starting
      at Acquire */
   Worker *NextQueue;
   Worker *NextAcquire;
   
   // The access association
   Queue *OwnerQ;
   MethodConfig *Config;
   string Access;
      
   // This is the subprocess IPC setup
   pid_t Process;
   int InFd;
   int OutFd;
   bool InReady;
   bool OutReady;
   
   // Various internal things
   bool Debug;
   vector<string> MessageQueue;
   string OutQueue;
   
   // Private constructor helper
   void Construct();
   
   // Message handling things
   bool ReadMessages();
   bool RunMessages();
   bool InFdReady();
   bool OutFdReady();
   
   // The message handlers
   bool Capabilities(string Message);
   bool SendConfiguration();

   bool MethodFailure();
   
   public:
   
   pkgAcquire::Queue::QItem *CurrentItem;
   
   string Status;
   
   // Load the method and do the startup 
   bool QueueItem(pkgAcquire::Queue::QItem *Item);
   bool Start();   
   
   Worker(Queue *OwnerQ,MethodConfig *Config);
   Worker(MethodConfig *Config);
   ~Worker();
};

#endif

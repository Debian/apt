// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-worker.h,v 1.2 1998/10/20 02:39:14 jgg Exp $
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
   protected:
   friend Queue;

   Worker *Next;
   
   // The access association
   Queue *OwnerQ;
   MethodConfig *Config;
   string Access;
      
   // This is the subprocess IPC setup
   pid_t Process;
   int InFd;
   int OutFd;
   
   // Various internal things
   bool Debug;
   vector<string> MessageQueue;

   // Private constructor helper
   void Construct();
   
   // Message handling things
   bool ReadMessages();
   bool RunMessages();
   
   // The message handlers
   bool Capabilities(string Message);
   
   public:
   
   // Load the method and do the startup 
   bool Start();   
   
   Worker(Queue *OwnerQ,string Access);
   Worker(MethodConfig *Config);
   ~Worker();
};

#endif

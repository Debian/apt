// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-worker.h,v 1.1 1998/10/15 06:59:59 jgg Exp $
/* ######################################################################

   Acquire Worker - Worker process manager
   
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
   
   Queue *OwnerQ;
   MethodConfig *Config;   
   Worker *Next;
   
   friend Queue;
   
   public:
   
   bool Create();
   
   Worker(Queue *OwnerQ);
   Worker(MethodConfig *Config);
   ~Worker();
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acqprogress.h,v 1.2 1998/11/12 05:30:09 jgg Exp $
/* ######################################################################

   Acquire Progress - Command line progress meter 
   
   ##################################################################### */
									/*}}}*/
#ifndef ACQPROGRESS_H
#define ACQPROGRESS_H

#include <apt-pkg/acquire.h>

class AcqTextStatus : public pkgAcquireStatus
{
   unsigned int &ScreenWidth;
   char BlankLine[300];
   unsigned long ID;
   unsigned long Quiet;
   
   public:
   
   virtual void IMSHit(pkgAcquire::ItemDesc &Itm);
   virtual void Fetch(pkgAcquire::ItemDesc &Itm);
   virtual void Done(pkgAcquire::ItemDesc &Itm);
   virtual void Fail(pkgAcquire::ItemDesc &Itm);
   virtual void Start();
   virtual void Stop();
   
   void Pulse(pkgAcquire *Owner);

   AcqTextStatus(unsigned int &ScreenWidth,unsigned int Quiet);
};

#endif

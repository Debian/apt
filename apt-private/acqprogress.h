// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Acquire Progress - Command line progress meter

   ##################################################################### */
									/*}}}*/
#ifndef ACQPROGRESS_H
#define ACQPROGRESS_H

#include <apt-pkg/acquire.h>
#include <apt-pkg/macros.h>

#include <string>

class APT_PUBLIC AcqTextStatus : public pkgAcquireStatus
{
   unsigned int &ScreenWidth;
   char BlankLine[1024];
   unsigned long ID;
   unsigned long Quiet;

   public:

   virtual bool MediaChange(std::string Media,std::string Drive);
   virtual void IMSHit(pkgAcquire::ItemDesc &Itm);
   virtual void Fetch(pkgAcquire::ItemDesc &Itm);
   virtual void Done(pkgAcquire::ItemDesc &Itm);
   virtual void Fail(pkgAcquire::ItemDesc &Itm);
   virtual void Start();
   virtual void Stop();

   bool Pulse(pkgAcquire *Owner);

   AcqTextStatus(unsigned int &ScreenWidth,unsigned int const Quiet);
};

#endif

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

#include <iostream>
#include <string>

class APT_PUBLIC AcqTextStatus : public pkgAcquireStatus
{
   std::ostream &out;
   unsigned int &ScreenWidth;
   size_t LastLineLength;
   unsigned long ID;
   unsigned long Quiet;

   APT_HIDDEN void clearLastLine();
   APT_HIDDEN void AssignItemID(pkgAcquire::ItemDesc &Itm);

   public:

   virtual bool ReleaseInfoChanges(metaIndex const * const LastRelease, metaIndex const * const CurrentRelease, std::vector<ReleaseInfoChange> &&Changes) APT_OVERRIDE;
   virtual bool MediaChange(std::string Media,std::string Drive) APT_OVERRIDE;
   virtual void IMSHit(pkgAcquire::ItemDesc &Itm) APT_OVERRIDE;
   virtual void Fetch(pkgAcquire::ItemDesc &Itm) APT_OVERRIDE;
   virtual void Done(pkgAcquire::ItemDesc &Itm) APT_OVERRIDE;
   virtual void Fail(pkgAcquire::ItemDesc &Itm) APT_OVERRIDE;
   virtual void Start() APT_OVERRIDE;
   virtual void Stop() APT_OVERRIDE;

   bool Pulse(pkgAcquire *Owner) APT_OVERRIDE;

   AcqTextStatus(std::ostream &out, unsigned int &ScreenWidth,unsigned int const Quiet);
};

#endif

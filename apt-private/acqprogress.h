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
#include <unordered_map>
#include <vector>

class APT_PUBLIC AcqTextStatus : public pkgAcquireStatus
{
   std::ostream &out;
   unsigned int &ScreenWidth;
   size_t LastLineLength;
   unsigned long ID;
   unsigned long Quiet;
   std::unordered_map<unsigned long, std::vector<std::string>> IgnoredErrorTexts;

   APT_HIDDEN void clearLastLine();
   APT_HIDDEN void AssignItemID(pkgAcquire::ItemDesc &Itm);

   public:
   bool ReleaseInfoChanges(metaIndex const *LastRelease, metaIndex const *CurrentRelease, std::vector<ReleaseInfoChange> &&Changes) override;
   bool MediaChange(std::string Media, std::string Drive) override;
   void IMSHit(pkgAcquire::ItemDesc &Itm) override;
   void Fetch(pkgAcquire::ItemDesc &Itm) override;
   void Done(pkgAcquire::ItemDesc &Itm) override;
   void Fail(pkgAcquire::ItemDesc &Itm) override;
   void Start() override;
   void Stop() override;

   bool Pulse(pkgAcquire *Owner) override;

   AcqTextStatus(std::ostream &out, unsigned int &ScreenWidth,unsigned int const Quiet);
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: algorithms.h,v 1.3 1998/07/19 21:24:11 jgg Exp $
/* ######################################################################

   Algorithms - A set of misc algorithms
   
   This simulate class displays what the ordering code has done and
   analyses it with a fresh new dependency cache. In this way we can
   see all of the effects of an upgrade run.

   pkgDistUpgrade computes an upgrade that causes as many packages as
   possible to move to the newest verison.
   
   pkgApplyStatus sets the target state based on the content of the status
   field in the status file. It is important to get proper crash recovery.

   pkgFixBroken corrects a broken system so that it is in a sane state.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_ALGORITHMS_H
#define PKGLIB_ALGORITHMS_H

#ifdef __GNUG__
#pragma interface "apt-pkg/algorithms.h"
#endif 

#include <apt-pkg/packagemanager.h>
#include <apt-pkg/depcache.h>

class pkgSimulate : public pkgPackageManager
{
   protected:

   unsigned char *Flags;
   
   pkgDepCache Sim;
   
   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg);
   void ShortBreaks();
   
   public:

   pkgSimulate(pkgDepCache &Cache);
};

class pkgProblemResolver
{
   pkgDepCache &Cache;
   typedef pkgCache::PkgIterator PkgIterator;
   typedef pkgCache::VerIterator VerIterator;
   typedef pkgCache::DepIterator DepIterator;
   typedef pkgCache::PrvIterator PrvIterator;
   typedef pkgCache::Version Version;
   typedef pkgCache::Package Package;
   
   enum Flags {Protected = (1 << 0), PreInstalled = (1 << 1),
               Upgradable = (1 << 2)};
   signed short *Scores;
   unsigned char *Flags;
   bool Debug;
   
   // Sort stuff
   static pkgProblemResolver *This;
   static int ScoreSort(const void *a,const void *b);

   struct PackageKill
   {
      PkgIterator Pkg;
      DepIterator Dep;
   };

   void MakeScores();
   bool DoUpgrade(pkgCache::PkgIterator Pkg);
   
   public:
   
   inline void Protect(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= Protected;};
   bool Resolve(bool BrokenFix = false);
   
   pkgProblemResolver(pkgDepCache &Cache);
};

bool pkgDistUpgrade(pkgDepCache &Cache);
bool pkgApplyStatus(pkgDepCache &Cache);
bool pkgFixBroken(pkgDepCache &Cache);

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: algorithms.h,v 1.10 2001/05/22 04:17:41 jgg Exp $
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
 
   pkgAllUpgrade attempts to upgade as many packages as possible but 
   without installing new packages.
   
   The problem resolver class contains a number of complex algorithms
   to try to best-guess an upgrade state. It solves the problem of 
   maximizing the number of install state packages while having no broken
   packages. 

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ALGORITHMS_H
#define PKGLIB_ALGORITHMS_H


#include <apt-pkg/packagemanager.h>
#include <apt-pkg/depcache.h>

#include <iostream>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/acquire.h>
using std::ostream;
#endif

class pkgAcquireStatus;

class pkgSimulate : public pkgPackageManager				/*{{{*/
{
   protected:

   class Policy : public pkgDepCache::Policy
   {
      pkgDepCache *Cache;
      public:
      
      virtual VerIterator GetCandidateVer(PkgIterator const &Pkg)
      {
	 return (*Cache)[Pkg].CandidateVerIter(*Cache);
      }
      
      Policy(pkgDepCache *Cache) : Cache(Cache) {};
   };
   
   unsigned char *Flags;
   
   Policy iPolicy;
   pkgDepCache Sim;
   pkgDepCache::ActionGroup group;
   
   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,std::string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg,bool Purge);

private:
   void ShortBreaks();
   void Describe(PkgIterator iPkg,std::ostream &out,bool Current,bool Candidate);
   
   public:

   pkgSimulate(pkgDepCache *Cache);
   ~pkgSimulate();
};
									/*}}}*/
class pkgProblemResolver						/*{{{*/
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   pkgDepCache &Cache;
   typedef pkgCache::PkgIterator PkgIterator;
   typedef pkgCache::VerIterator VerIterator;
   typedef pkgCache::DepIterator DepIterator;
   typedef pkgCache::PrvIterator PrvIterator;
   typedef pkgCache::Version Version;
   typedef pkgCache::Package Package;
   
   enum Flags {Protected = (1 << 0), PreInstalled = (1 << 1),
               Upgradable = (1 << 2), ReInstateTried = (1 << 3),
               ToRemove = (1 << 4)};
   int *Scores;
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

   bool ResolveInternal(bool const BrokenFix = false);
   bool ResolveByKeepInternal();
   
   protected:
   bool InstOrNewPolicyBroken(pkgCache::PkgIterator Pkg);

   public:
   
   inline void Protect(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= Protected; Cache.MarkProtected(Pkg);};
   inline void Remove(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= ToRemove;};
   inline void Clear(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] &= ~(Protected | ToRemove);};
   
   // Try to intelligently resolve problems by installing and removing packages   
   bool Resolve(bool BrokenFix = false);
   
   // Try to resolve problems only by using keep
   bool ResolveByKeep();

   // Install all protected packages   
   void InstallProtect();   
   
   pkgProblemResolver(pkgDepCache *Cache);
   ~pkgProblemResolver();
};
									/*}}}*/
bool pkgDistUpgrade(pkgDepCache &Cache);
bool pkgApplyStatus(pkgDepCache &Cache);
bool pkgFixBroken(pkgDepCache &Cache);
bool pkgAllUpgrade(pkgDepCache &Cache);
bool pkgMinimizeUpgrade(pkgDepCache &Cache);

void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List);

bool ListUpdate(pkgAcquireStatus &progress, pkgSourceList &List, int PulseInterval=0);
bool AcquireUpdate(pkgAcquire &Fetcher, int const PulseInterval = 0,
		   bool const RunUpdateScripts = true, bool const ListCleanup = true);

#endif

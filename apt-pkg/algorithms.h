// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Algorithms - A set of misc algorithms
   
   This simulate class displays what the ordering code has done and
   analyses it with a fresh new dependency cache. In this way we can
   see all of the effects of an upgrade run.

   pkgDistUpgrade computes an upgrade that causes as many packages as
   possible to move to the newest version.
   
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
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <iostream>
#include <string>

#include <apt-pkg/macros.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/acquire.h>
using std::ostream;
#endif

#ifndef APT_9_CLEANER_HEADERS
// include pkg{DistUpgrade,AllUpgrade,MiniizeUpgrade} here for compatibility
#include <apt-pkg/upgrade.h>
#include <apt-pkg/update.h>
#endif


class pkgSimulate : public pkgPackageManager				/*{{{*/
{
   void *d;
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
   
   // The Actual installation implementation
   virtual bool Install(PkgIterator Pkg,std::string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg,bool Purge);

private:
   APT_HIDDEN void ShortBreaks();
   APT_HIDDEN void Describe(PkgIterator iPkg,std::ostream &out,bool Current,bool Candidate);

   public:

   pkgSimulate(pkgDepCache *Cache);
   virtual ~pkgSimulate();
};
									/*}}}*/
class pkgProblemResolver						/*{{{*/
{
 private:
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
   APT_HIDDEN static int ScoreSort(const void *a,const void *b) APT_PURE;

   struct PackageKill
   {
      PkgIterator Pkg;
      DepIterator Dep;
   };

   APT_HIDDEN void MakeScores();
   APT_HIDDEN bool DoUpgrade(pkgCache::PkgIterator Pkg);

   APT_HIDDEN bool ResolveInternal(bool const BrokenFix = false);
   APT_HIDDEN bool ResolveByKeepInternal();

   protected:
   bool InstOrNewPolicyBroken(pkgCache::PkgIterator Pkg);

   public:
   
   inline void Protect(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= Protected; Cache.MarkProtected(Pkg);};
   inline void Remove(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= ToRemove;};
   inline void Clear(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] &= ~(Protected | ToRemove);};

   // Try to intelligently resolve problems by installing and removing packages
#if APT_PKG_ABI >= 413
   bool Resolve(bool BrokenFix = false, OpProgress * const Progress = NULL);
#else
   bool Resolve(bool BrokenFix = false);
   bool Resolve(bool BrokenFix, OpProgress * const Progress);
#endif

   // Try to resolve problems only by using keep
#if APT_PKG_ABI >= 413
   bool ResolveByKeep(OpProgress * const Progress = NULL);
#else
   bool ResolveByKeep();
   bool ResolveByKeep(OpProgress * const Progress);
#endif

   APT_DEPRECATED void InstallProtect();

   pkgProblemResolver(pkgDepCache *Cache);
   virtual ~pkgProblemResolver();
};
									/*}}}*/
bool pkgApplyStatus(pkgDepCache &Cache);
bool pkgFixBroken(pkgDepCache &Cache);

void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List);


#endif

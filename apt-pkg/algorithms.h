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
 
   pkgAllUpgrade attempts to upgrade as many packages as possible but
   without installing new packages.
   
   The problem resolver class contains a number of complex algorithms
   to try to best-guess an upgrade state. It solves the problem of 
   maximizing the number of install state packages while having no broken
   packages. 

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ALGORITHMS_H
#define PKGLIB_ALGORITHMS_H

#include <apt-pkg/cachefilter.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>

#include <iostream>
#include <memory>
#include <string>

#include <apt-pkg/macros.h>




class pkgSimulatePrivate;
class APT_PUBLIC pkgSimulate : public pkgPackageManager				/*{{{*/
{
   pkgSimulatePrivate * const d;
   protected:

   class APT_PUBLIC Policy : public pkgDepCache::Policy
   {
      pkgDepCache *Cache;
      public:
      
      virtual VerIterator GetCandidateVer(PkgIterator const &Pkg) APT_OVERRIDE
      {
	 return (*Cache)[Pkg].CandidateVerIter(*Cache);
      }
      
      explicit Policy(pkgDepCache *Cache) : Cache(Cache) {};
   };
   
   unsigned char *Flags;
   
   Policy iPolicy;
   pkgDepCache Sim;
   pkgDepCache::ActionGroup group;

   // The Actual installation implementation
   virtual bool Install(PkgIterator Pkg,std::string File) APT_OVERRIDE;
   virtual bool Configure(PkgIterator Pkg) APT_OVERRIDE;
   virtual bool Remove(PkgIterator Pkg,bool Purge) APT_OVERRIDE;

public:
   bool Go(APT::Progress::PackageManager * progress) override;

private:
   APT_HIDDEN void ShortBreaks();
   APT_HIDDEN void Describe(PkgIterator iPkg,std::ostream &out,bool Current,bool Candidate);
   APT_HIDDEN bool RealInstall(PkgIterator Pkg,std::string File);
   APT_HIDDEN bool RealConfigure(PkgIterator Pkg);
   APT_HIDDEN bool RealRemove(PkgIterator Pkg,bool Purge);

   public:

   explicit pkgSimulate(pkgDepCache *Cache);
   virtual ~pkgSimulate();
};
									/*}}}*/
class APT_PUBLIC pkgProblemResolver						/*{{{*/
{
 private:
   /** \brief dpointer placeholder (for later in case we need it) */
   void * const d;

   pkgDepCache &Cache;
   typedef pkgCache::PkgIterator PkgIterator;
   typedef pkgCache::VerIterator VerIterator;
   typedef pkgCache::DepIterator DepIterator;
   typedef pkgCache::PrvIterator PrvIterator;
   typedef pkgCache::Version Version;
   typedef pkgCache::Package Package;

   enum Flags
   {
      Protected = (1 << 0),
      PreInstalled = (1 << 1),
      Upgradable = (1 << 2),
      ReInstateTried = (1 << 3),
      ToRemove = (1 << 4),
      BrokenPolicyAllowed = (1 << 5)
   };
   int *Scores;
   unsigned char *Flags;
   bool Debug;
   
   // Sort stuff
   APT_HIDDEN int ScoreSort(Package const *A, Package const *B) APT_PURE;

   struct APT_PUBLIC PackageKill
   {
      PkgIterator Pkg;
      DepIterator Dep;
   };

   APT_HIDDEN void MakeScores();
   APT_HIDDEN bool DoUpgrade(pkgCache::PkgIterator Pkg);

   protected:
   bool InstOrNewPolicyBroken(pkgCache::PkgIterator Pkg);

   public:
   
   inline void Protect(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= Protected; Cache.MarkProtected(Pkg);};
   inline void Remove(pkgCache::PkgIterator Pkg) {Flags[Pkg->ID] |= ToRemove;};
   inline void Clear(pkgCache::PkgIterator Pkg) { Flags[Pkg->ID] &= ~(Protected | ToRemove | BrokenPolicyAllowed); };
#ifdef APT_COMPILING_APT
   inline void AllowBrokenPolicy(pkgCache::PkgIterator Pkg) { Flags[Pkg->ID] |= BrokenPolicyAllowed; };
#endif

   bool KeepPhasedUpdates();

   // Try to intelligently resolve problems by installing and removing packages
   bool Resolve(bool BrokenFix = false, OpProgress * const Progress = NULL);
   APT_HIDDEN bool ResolveInternal(bool const BrokenFix = false);

   // Try to resolve problems only by using keep
   bool ResolveByKeep(OpProgress * const Progress = NULL);
   APT_HIDDEN bool ResolveByKeepInternal();

   explicit pkgProblemResolver(pkgDepCache *Cache);
   virtual ~pkgProblemResolver();
};
									/*}}}*/
APT_PUBLIC bool pkgApplyStatus(pkgDepCache &Cache);
APT_PUBLIC bool pkgFixBroken(pkgDepCache &Cache);

APT_PUBLIC void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List);

namespace APT
{
namespace KernelAutoRemoveHelper
{
// Public for linking to apt-private, but no A{P,B}I guarantee.
APT_PUBLIC std::unique_ptr<APT::CacheFilter::Matcher> GetProtectedKernelsFilter(pkgCache *cache, bool returnRemove = false);
std::string GetProtectedKernelsRegex(pkgCache *cache, bool ReturnRemove = false);
std::string getUname(std::string const &packageName);

} // namespace KernelAutoRemoveHelper

} // namespace APT

#endif

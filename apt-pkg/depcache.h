// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.h,v 1.14 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   DepCache - Dependency Extension data for the cache
   
   This class stores the cache data and a set of extension structures for
   monitoring the current state of all the packages. It also generates and
   caches the 'install' state of many things. This refers to the state of the
   package after an install has been run.

   The StateCache::State field can be -1,0,1,2 which is <,=,>,no current.
   StateCache::Mode is which of the 3 fields is active.
   
   This structure is important to support the readonly status of the cache 
   file. When the data is saved the cache will be refereshed from our 
   internal rep and written to disk. Then the actual persistant data 
   files will be put on the disk.

   Each dependency is compared against 3 target versions to produce to
   3 dependency results.
     Now - Compared using the Currently install version
     Install - Compared using the install version (final state)
     CVer - (Candidate Verion) Compared using the Candidate Version
   The candidate and now results are used to decide wheather a package
   should be automatically installed or if it should be left alone.
   
   Remember, the Candidate Version is selected based on the distribution
   settings for the Package. The Install Version is selected based on the
   state (Delete, Keep, Install) field and can be either the Current Version
   or the Candidate version.
   
   The Candidate version is what is shown the 'Install Version' field.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEPCACHE_H
#define PKGLIB_DEPCACHE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/depcache.h"
#endif

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/progress.h>

class pkgDepCache : protected pkgCache::Namespace
{
   public:
   
   // These flags are used in DepState
   enum DepFlags {DepNow = (1 << 0),DepInstall = (1 << 1),DepCVer = (1 << 2),
                  DepGNow = (1 << 3),DepGInstall = (1 << 4),DepGCVer = (1 << 5)};

   // These flags are used in StateCache::DepState
   enum DepStateFlags {DepNowPolicy = (1 << 0), DepNowMin = (1 << 1),
                       DepInstPolicy = (1 << 2), DepInstMin = (1 << 3),
                       DepCandPolicy = (1 << 4), DepCandMin = (1 << 5)};
   
   // These flags are used in StateCache::iFlags
   enum InternalFlags {AutoKept = (1 << 0), Purge = (1 << 1), ReInstall = (1 << 2)};
      
   enum VersionTypes {NowVersion, InstallVersion, CandidateVersion};
   enum ModeList {ModeDelete = 0, ModeKeep = 1, ModeInstall = 2};
   struct StateCache
   {
      // Epoch stripped text versions of the two version fields
      const char *CandVersion;
      const char *CurVersion;

      // Pointer to the candidate install version. 
      Version *CandidateVer;

      // Pointer to the install version.
      Version *InstallVer;
      
      // Copy of Package::Flags
      unsigned short Flags;
      unsigned short iFlags;           // Internal flags

      // Various tree indicators
      signed char Status;              // -1,0,1,2
      unsigned char Mode;              // ModeList
      unsigned char DepState;          // DepState Flags

      // Update of candidate version
      const char *StripEpoch(const char *Ver);
      void Update(PkgIterator Pkg,pkgCache &Cache);
      
      // Various test members for the current status of the package
      inline bool NewInstall() const {return Status == 2 && Mode == ModeInstall;};
      inline bool Delete() const {return Mode == ModeDelete;};
      inline bool Keep() const {return Mode == ModeKeep;};
      inline bool Upgrade() const {return Status > 0 && Mode == ModeInstall;};
      inline bool Upgradable() const {return Status >= 1;};
      inline bool Downgrade() const {return Status < 0 && Mode == ModeInstall;};
      inline bool Held() const {return Status != 0 && Keep();};
      inline bool NowBroken() const {return (DepState & DepNowMin) != DepNowMin;};
      inline bool InstBroken() const {return (DepState & DepInstMin) != DepInstMin;};
      inline bool Install() const {return Mode == ModeInstall;};
      inline VerIterator InstVerIter(pkgCache &Cache)
                {return VerIterator(Cache,InstallVer);};
      inline VerIterator CandidateVerIter(pkgCache &Cache)
                {return VerIterator(Cache,CandidateVer);};
   };
   
   // Helper functions
   void BuildGroupOrs(VerIterator const &V);
   void UpdateVerState(PkgIterator Pkg);

   // User Policy control
   class Policy
   {
      public:
      
      virtual VerIterator GetCandidateVer(PkgIterator Pkg);
      virtual bool IsImportantDep(DepIterator Dep);
      
      virtual ~Policy() {};
   };
     
   protected:

   // State information
   pkgCache *Cache;
   StateCache *PkgState;
   unsigned char *DepState;
   
   double iUsrSize;
   double iDownloadSize;
   unsigned long iInstCount;
   unsigned long iDelCount;
   unsigned long iKeepCount;
   unsigned long iBrokenCount;
   unsigned long iBadCount;
   
   Policy *delLocalPolicy;           // For memory clean up..
   Policy *LocalPolicy;
   
   // Check for a matching provides
   bool CheckDep(DepIterator Dep,int Type,PkgIterator &Res);
   inline bool CheckDep(DepIterator Dep,int Type)
   {
      PkgIterator Res(*this,0);
      return CheckDep(Dep,Type,Res);
   }
   
   // Computes state information for deps and versions (w/o storing)
   unsigned char DependencyState(DepIterator &D);
   unsigned char VersionState(DepIterator D,unsigned char Check,
			      unsigned char SetMin,
			      unsigned char SetPolicy);

   // Recalculates various portions of the cache, call after changing something
   void Update(DepIterator Dep);           // Mostly internal
   void Update(PkgIterator const &P);
   
   // Count manipulators
   void AddSizes(const PkgIterator &Pkg,signed long Mult = 1);
   inline void RemoveSizes(const PkgIterator &Pkg) {AddSizes(Pkg,-1);};
   void AddStates(const PkgIterator &Pkg,int Add = 1);
   inline void RemoveStates(const PkgIterator &Pkg) {AddStates(Pkg,-1);};
   
   public:

   // Legacy.. We look like a pkgCache
   inline operator pkgCache &() {return *Cache;};
   inline Header &Head() {return *Cache->HeaderP;};
   inline PkgIterator PkgBegin() {return Cache->PkgBegin();};
   inline PkgIterator FindPkg(string const &Name) {return Cache->FindPkg(Name);};

   inline pkgCache &GetCache() {return *Cache;};
   inline pkgVersioningSystem &VS() {return *Cache->VS;};
   
   // Policy implementation
   inline VerIterator GetCandidateVer(PkgIterator Pkg) {return LocalPolicy->GetCandidateVer(Pkg);};
   inline bool IsImportantDep(DepIterator Dep) {return LocalPolicy->IsImportantDep(Dep);};
   inline Policy &GetPolicy() {return *LocalPolicy;};
   
   // Accessors
   inline StateCache &operator [](PkgIterator const &I) {return PkgState[I->ID];};
   inline unsigned char &operator [](DepIterator const &I) {return DepState[I->ID];};

   // Manipulators
   void MarkKeep(PkgIterator const &Pkg,bool Soft = false);
   void MarkDelete(PkgIterator const &Pkg,bool Purge = false);
   void MarkInstall(PkgIterator const &Pkg,bool AutoInst = true,
		    unsigned long Depth = 0);
   void SetReInstall(PkgIterator const &Pkg,bool To);
   void SetCandidateVersion(VerIterator TargetVer);
   
   // This is for debuging
   void Update(OpProgress *Prog = 0);
   
   // Size queries
   inline double UsrSize() {return iUsrSize;};
   inline double DebSize() {return iDownloadSize;};
   inline unsigned long DelCount() {return iDelCount;};
   inline unsigned long KeepCount() {return iKeepCount;};
   inline unsigned long InstCount() {return iInstCount;};
   inline unsigned long BrokenCount() {return iBrokenCount;};
   inline unsigned long BadCount() {return iBadCount;};

   bool Init(OpProgress *Prog);
   
   pkgDepCache(pkgCache *Cache,Policy *Plcy = 0);
   virtual ~pkgDepCache();
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.h,v 1.3 1998/07/12 23:58:25 jgg Exp $
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
// Header section: pkglib
#ifndef PKGLIB_DEPCACHE_H
#define PKGLIB_DEPCACHE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/depcache.h"
#endif

#include <apt-pkg/pkgcache.h>

class pkgDepCache : public pkgCache
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
   enum InternalFlags {AutoKept = (1 << 0)};
      
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

      // Various tree indicators
      signed char Status;              // -1,0,1,2
      unsigned char Mode;              // ModeList
      unsigned char DepState;          // DepState Flags

      // Copy of Package::Flags
      unsigned short Flags;
      unsigned short iFlags;           // Internal flags

      // Update of candidate version
      const char *StripEpoch(const char *Ver);
      void Update(PkgIterator Pkg,pkgCache &Cache);
      
      // Various test members for the current status of the package
      inline bool NewInstall() const {return Status == 2 && Mode == ModeInstall;};
      inline bool Delete() const {return Mode == ModeDelete;};
      inline bool Keep() const {return Mode == ModeKeep;};
      inline bool Upgrade() const {return Status > 0 && Mode == ModeInstall;};
      inline bool Upgradable() const {return Status == 1;};
      inline bool Downgrade() const {return Status < 0;};
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

   bool Init();

   protected:

   // State information
   StateCache *PkgState;
   unsigned char *DepState;
   
   long iUsrSize;
   long iDownloadSize;
   long iInstCount;
   long iDelCount;
   long iKeepCount;
   long iBrokenCount;
   long iBadCount;
      
   // Check for a matching provides
   bool CheckDep(DepIterator Dep,int Type,PkgIterator &Res);
   inline bool CheckDep(DepIterator Dep,int Type)
   {
      PkgIterator Res(*this);
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
   void AddSizes(const PkgIterator &Pkg,long Mult = 1);
   inline void RemoveSizes(const PkgIterator &Pkg) {AddSizes(Pkg,-1);};
   void AddStates(const PkgIterator &Pkg,int Add = 1);
   inline void RemoveStates(const PkgIterator &Pkg) {AddStates(Pkg,-1);};

   public:

   // Policy implementation
   virtual VerIterator GetCandidateVer(PkgIterator Pkg);
   virtual bool IsImportantDep(DepIterator Dep);
         
   // Accessors
   inline StateCache &operator [](PkgIterator const &I) {return PkgState[I->ID];};
   inline unsigned char &operator [](DepIterator const &I) {return DepState[I->ID];};

   // Manipulators
   void MarkKeep(PkgIterator const &Pkg,bool Soft = false);
   void MarkDelete(PkgIterator const &Pkg);
   void MarkInstall(PkgIterator const &Pkg,bool AutoInst = true);
   
   // This is for debuging
   void Update();

   // Hook to keep the extra data in sync
   virtual bool ReMap();
   
   // Size queries
   inline long UsrSize() {return iUsrSize;};
   inline long DebSize() {return iDownloadSize;};
   inline long DelCount() {return iDelCount;};
   inline long KeepCount() {return iKeepCount;};
   inline long InstCount() {return iInstCount;};
   inline long BrokenCount() {return iBrokenCount;};
   inline long BadCount() {return iBadCount;};
   
   pkgDepCache(MMap &Map);
   virtual ~pkgDepCache();
};

#endif

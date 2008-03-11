// -*- mode: c++; mode: fold -*-
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


#include <apt-pkg/pkgcache.h>
#include <apt-pkg/progress.h>

#include <regex.h>

#include <vector>
#include <memory>

class pkgDepCache : protected pkgCache::Namespace
{
   public:

   /** \brief An arbitrary predicate on packages. */
   class InRootSetFunc
   {
   public:
     virtual bool InRootSet(const pkgCache::PkgIterator &pkg) {return false;};
     virtual ~InRootSetFunc() {};
   };

   private:
   /** \brief Mark a single package and all its unmarked important
    *  dependencies during mark-and-sweep.
    *
    *  Recursively invokes itself to mark all dependencies of the
    *  package.
    *
    *  \param pkg The package to mark.
    *
    *  \param ver The version of the package that is to be marked.
    *
    *  \param follow_recommends If \b true, recommendations of the
    *  package will be recursively marked.
    *
    *  \param follow_suggests If \b true, suggestions of the package
    *  will be recursively marked.
    */
   void MarkPackage(const pkgCache::PkgIterator &pkg,
		    const pkgCache::VerIterator &ver,
		    bool follow_recommends,
		    bool follow_suggests);

   /** \brief Update the Marked field of all packages.
    *
    *  Each package's StateCache::Marked field will be set to \b true
    *  if and only if it can be reached from the root set.  By
    *  default, the root set consists of the set of manually installed
    *  or essential packages, but it can be extended using the
    *  parameter #rootFunc.
    *
    *  \param rootFunc A callback that can be used to add extra
    *  packages to the root set.
    *
    *  \return \b false if an error occurred.
    */
   bool MarkRequired(InRootSetFunc &rootFunc);

   /** \brief Set the StateCache::Garbage flag on all packages that
    *  should be removed.
    *
    *  Packages that were not marked by the last call to #MarkRequired
    *  are tested to see whether they are actually garbage.  If so,
    *  they are marked as such.
    *
    *  \return \b false if an error occurred.
    */
   bool Sweep();

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

   /** \brief Represents an active action group.
    *
    *  An action group is a group of actions that are currently being
    *  performed.  While an active group is active, certain routine
    *  clean-up actions that would normally be performed after every
    *  cache operation are delayed until the action group is
    *  completed.  This is necessary primarily to avoid inefficiencies
    *  when modifying a large number of packages at once.
    *
    *  This class represents an active action group.  Creating an
    *  instance will create an action group; destroying one will
    *  destroy the corresponding action group.
    *
    *  The following operations are suppressed by this class:
    *
    *    - Keeping the Marked and Garbage flags up to date.
    *
    *  \note This can be used in the future to easily accumulate
    *  atomic actions for undo or to display "what apt did anyway";
    *  e.g., change the counter of how many action groups are active
    *  to a std::set of pointers to them and use those to store
    *  information about what happened in a group in the group.
    */
   class ActionGroup
   {
       pkgDepCache &cache;

       bool released;

       /** Action groups are noncopyable. */
       ActionGroup(const ActionGroup &other);
   public:
       /** \brief Create a new ActionGroup.
	*
	*  \param cache The cache that this ActionGroup should
	*  manipulate.
	*
	*  As long as this object exists, no automatic cleanup
	*  operations will be undertaken.
	*/
       ActionGroup(pkgDepCache &cache);

       /** \brief Clean up the action group before it is destroyed.
        *
        *  If it is destroyed later, no second cleanup wil be run.
	*/
       void release();

       /** \brief Destroy the action group.
	*
	*  If this is the last action group, the automatic cache
	*  cleanup operations will be undertaken.
	*/
       ~ActionGroup();
   };

   /** \brief Returns \b true for packages matching a regular
    *  expression in APT::NeverAutoRemove.
    */
   class DefaultRootSetFunc : public InRootSetFunc
   {
     std::vector<regex_t *> rootSetRegexp;
     bool constructedSuccessfully;

   public:
     DefaultRootSetFunc();
     ~DefaultRootSetFunc();

     /** \return \b true if the class initialized successfully, \b
      *  false otherwise.  Used to avoid throwing an exception, since
      *  APT classes generally don't.
      */
     bool wasConstructedSuccessfully() const { return constructedSuccessfully; }

     bool InRootSet(const pkgCache::PkgIterator &pkg);
   };

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

      /** \brief \b true if this package can be reached from the root set. */
      bool Marked;

      /** \brief \b true if this package is unused and should be removed.
       *
       *  This differs from !#Marked, because it is possible that some
       *  unreachable packages will be protected from becoming
       *  garbage.
       */
      bool Garbage;

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
      inline bool NowPolicyBroken() const {return (DepState & DepNowPolicy) != DepNowPolicy;};
      inline bool InstBroken() const {return (DepState & DepInstMin) != DepInstMin;};
      inline bool InstPolicyBroken() const {return (DepState & DepInstPolicy) != DepInstPolicy;};
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

   private:
   /** The number of open "action groups"; certain post-action
    *  operations are suppressed if this number is > 0.
    */
   int group_level;

   friend class ActionGroup;
     
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
   unsigned long iPolicyBrokenCount;
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

   /** \return A function identifying packages in the root set other
    *  than manually installed packages and essential packages, or \b
    *  NULL if an error occurs.
    *
    *  \todo Is this the best place for this function?  Perhaps the
    *  settings for mark-and-sweep should be stored in a single
    *  external class?
    */
   virtual InRootSetFunc *GetRootSetFunc();

   /** \return \b true if the garbage collector should follow recommendations.
    */
   virtual bool MarkFollowsRecommends();

   /** \return \b true if the garbage collector should follow suggestions.
    */
   virtual bool MarkFollowsSuggests();

   /** \brief Update the Marked and Garbage fields of all packages.
    *
    *  This routine is implicitly invoked after all state manipulators
    *  and when an ActionGroup is destroyed.  It invokes #MarkRequired
    *  and #Sweep to do its dirty work.
    *
    *  \param rootFunc A predicate that returns \b true for packages
    *  that should be added to the root set.
    */
   bool MarkAndSweep(InRootSetFunc &rootFunc)
   {
     return MarkRequired(rootFunc) && Sweep();
   }

   bool MarkAndSweep()
   {
     std::auto_ptr<InRootSetFunc> f(GetRootSetFunc());
     if(f.get() != NULL)
       return MarkAndSweep(*f.get());
     else
       return false;
   }

   /** \name State Manipulators
    */
   // @{
   void MarkKeep(PkgIterator const &Pkg, bool Soft = false,
		 bool FromUser = true);
   void MarkDelete(PkgIterator const &Pkg,bool Purge = false);
   void MarkInstall(PkgIterator const &Pkg,bool AutoInst = true,
		    unsigned long Depth = 0, bool FromUser = true,
		    bool ForceImportantDeps = false);
   void SetReInstall(PkgIterator const &Pkg,bool To);
   void SetCandidateVersion(VerIterator TargetVer);

   /** Set the "is automatically installed" flag of Pkg. */
   void MarkAuto(const PkgIterator &Pkg, bool Auto);
   // @}
   
   // This is for debuging
   void Update(OpProgress *Prog = 0);

   // read persistent states
   bool readStateFile(OpProgress *prog);
   bool writeStateFile(OpProgress *prog, bool InstalledOnly=false);
   
   // Size queries
   inline double UsrSize() {return iUsrSize;};
   inline double DebSize() {return iDownloadSize;};
   inline unsigned long DelCount() {return iDelCount;};
   inline unsigned long KeepCount() {return iKeepCount;};
   inline unsigned long InstCount() {return iInstCount;};
   inline unsigned long BrokenCount() {return iBrokenCount;};
   inline unsigned long PolicyBrokenCount() {return iPolicyBrokenCount;};
   inline unsigned long BadCount() {return iBadCount;};

   bool Init(OpProgress *Prog);
   
   pkgDepCache(pkgCache *Cache,Policy *Plcy = 0);
   virtual ~pkgDepCache();
};

#endif

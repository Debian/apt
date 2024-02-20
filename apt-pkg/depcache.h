// -*- mode: c++; mode: fold -*-
// Description								/*{{{*/
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
   internal rep and written to disk. Then the actual persistent data
   files will be put on the disk.

   Each dependency is compared against 3 target versions to produce to
   3 dependency results.
     Now - Compared using the Currently install version
     Install - Compared using the install version (final state)
     CVer - (Candidate Version) Compared using the Candidate Version
   The candidate and now results are used to decide whether a package
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

#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <cstddef>

#include <list>
#include <memory>
#include <string>
#include <utility>


class OpProgress;
class pkgVersioningSystem;
namespace APT
{
template <class Container>
class PackageContainer;
using PackageVector = PackageContainer<std::vector<pkgCache::PkgIterator>>;
} // namespace APT

class APT_PUBLIC pkgDepCache : protected pkgCache::Namespace
{
   public:

   /** \brief An arbitrary predicate on packages. */
   class APT_PUBLIC InRootSetFunc
   {
   public:
     virtual bool InRootSet(const pkgCache::PkgIterator &/*pkg*/) {return false;};
     virtual ~InRootSetFunc() {};
   };

   private:
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
   enum InternalFlags {AutoKept = (1 << 0), Purge = (1 << 1), ReInstall = (1 << 2), Protected = (1 << 3)};
      
   enum VersionTypes {NowVersion, InstallVersion, CandidateVersion};
   enum ModeList {ModeDelete = 0, ModeKeep = 1, ModeInstall = 2, ModeGarbage = 3};

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
   class APT_PUBLIC ActionGroup
   {
       void * const d;
       pkgDepCache &cache;

       bool released;

       /** Action groups are noncopyable. */
       APT_HIDDEN ActionGroup(const ActionGroup &other);
   public:
       /** \brief Create a new ActionGroup.
	*
	*  \param cache The cache that this ActionGroup should
	*  manipulate.
	*
	*  As long as this object exists, no automatic cleanup
	*  operations will be undertaken.
	*/
       explicit ActionGroup(pkgDepCache &cache);

       /** \brief Clean up the action group before it is destroyed.
        *
        *  If it is destroyed later, no second cleanup will be run.
	*/
       void release();

       /** \brief Destroy the action group.
	*
	*  If this is the last action group, the automatic cache
	*  cleanup operations will be undertaken.
	*/
       virtual ~ActionGroup();
   };

   /** \brief Returns \b true for packages matching a regular
    *  expression in APT::NeverAutoRemove.
    */
   class APT_PUBLIC DefaultRootSetFunc : public InRootSetFunc, public Configuration::MatchAgainstConfig
   {
   public:
     DefaultRootSetFunc() : Configuration::MatchAgainstConfig("APT::NeverAutoRemove") {};
     virtual ~DefaultRootSetFunc() {};

     bool InRootSet(const pkgCache::PkgIterator &pkg) APT_OVERRIDE { return pkg.end() == false && Match(pkg.Name()); };
   };

   struct APT_PUBLIC StateCache
   {
      // text versions of the two version fields
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
      void Update(PkgIterator Pkg,pkgCache &Cache);
      
      // Various test members for the current status of the package
      inline bool NewInstall() const {return Status == 2 && Mode == ModeInstall;};
      inline bool Delete() const {return Mode == ModeDelete;};
      inline bool Purge() const {return Delete() == true && (iFlags & pkgDepCache::Purge) == pkgDepCache::Purge; };
      inline bool Keep() const {return Mode == ModeKeep;};
      inline bool Protect() const {return (iFlags & Protected) == Protected;};
      inline bool Upgrade() const {return Status > 0 && Mode == ModeInstall;};
      inline bool Upgradable() const {return Status >= 1 && CandidateVer != NULL;};
      inline bool Downgrade() const {return Status < 0 && Mode == ModeInstall;};
      inline bool Held() const {return Status != 0 && Keep();};
      inline bool NowBroken() const {return (DepState & DepNowMin) != DepNowMin;};
      inline bool NowPolicyBroken() const {return (DepState & DepNowPolicy) != DepNowPolicy;};
      inline bool InstBroken() const {return (DepState & DepInstMin) != DepInstMin;};
      inline bool InstPolicyBroken() const {return (DepState & DepInstPolicy) != DepInstPolicy;};
      inline bool Install() const {return Mode == ModeInstall;};
      inline bool ReInstall() const {return Delete() == false && (iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall;};
      inline VerIterator InstVerIter(pkgCache &Cache)
                {return VerIterator(Cache,InstallVer);};
      inline VerIterator CandidateVerIter(pkgCache &Cache)
                {return VerIterator(Cache,CandidateVer);};
   };
   
   // Helper functions
   void BuildGroupOrs(VerIterator const &V);
   void UpdateVerState(PkgIterator const &Pkg);

   // User Policy control
   class APT_PUBLIC Policy
   {
      public:
      Policy() {
         InstallRecommends = _config->FindB("APT::Install-Recommends", false);
         InstallSuggests = _config->FindB("APT::Install-Suggests", false);
      }

      virtual VerIterator GetCandidateVer(PkgIterator const &Pkg);
      virtual bool IsImportantDep(DepIterator const &Dep) const;
      virtual signed short GetPriority(PkgIterator const &Pkg);
      virtual signed short GetPriority(VerIterator const &Ver, bool ConsiderFiles=true);
      virtual signed short GetPriority(PkgFileIterator const &File);

      virtual ~Policy() {};

      private:
      bool InstallRecommends;
      bool InstallSuggests;
   };

   private:
   /** The number of open "action groups"; certain post-action
    *  operations are suppressed if this number is > 0.
    */
   int group_level;

   friend class ActionGroup;
   public:
   int IncreaseActionGroupLevel();
   int DecreaseActionGroupLevel();

   protected:

   // State information
   pkgCache *Cache;
   StateCache *PkgState;
   unsigned char *DepState;

   /** Stores the space changes after installation */
   signed long long iUsrSize;
   /** Stores how much we need to download to get the packages */
   unsigned long long iDownloadSize;
   unsigned long iInstCount;
   unsigned long iDelCount;
   unsigned long iKeepCount;
   unsigned long iBrokenCount;
   unsigned long iPolicyBrokenCount;
   unsigned long iBadCount;

   bool DebugMarker;
   bool DebugAutoInstall;

   Policy *delLocalPolicy;           // For memory clean up..
   Policy *LocalPolicy;
   
   // Check for a matching provides
   bool CheckDep(DepIterator const &Dep,int const Type,PkgIterator &Res);
   inline bool CheckDep(DepIterator const &Dep,int const Type)
   {
      PkgIterator Res(*this,0);
      return CheckDep(Dep,Type,Res);
   }
   
   // Computes state information for deps and versions (w/o storing)
   unsigned char DependencyState(DepIterator const &D);
   unsigned char VersionState(DepIterator D,unsigned char const Check,
			      unsigned char const SetMin,
			      unsigned char const SetPolicy) const;

   // Recalculates various portions of the cache, call after changing something
   void Update(DepIterator Dep);           // Mostly internal
   void Update(PkgIterator const &P);
   
   // Count manipulators
   void AddSizes(const PkgIterator &Pkg, bool const Invert = false);
   inline void RemoveSizes(const PkgIterator &Pkg) {AddSizes(Pkg, true);};
   void AddStates(const PkgIterator &Pkg, bool const Invert = false);
   inline void RemoveStates(const PkgIterator &Pkg) {AddStates(Pkg,true);};
   
   public:

   // Legacy.. We look like a pkgCache
   inline operator pkgCache &() {return *Cache;};
   inline Header &Head() {return *Cache->HeaderP;};
   inline GrpIterator GrpBegin() {return Cache->GrpBegin();};
   inline PkgIterator PkgBegin() {return Cache->PkgBegin();};
   inline GrpIterator FindGrp(APT::StringView Name) {return Cache->FindGrp(Name);};
   inline PkgIterator FindPkg(APT::StringView Name) {return Cache->FindPkg(Name);};
   inline PkgIterator FindPkg(APT::StringView Name, APT::StringView Arch) {return Cache->FindPkg(Name, Arch);};

   inline pkgCache &GetCache() {return *Cache;};
   inline pkgVersioningSystem &VS() {return *Cache->VS;};

   inline bool IsImportantDep(DepIterator Dep) const {return LocalPolicy->IsImportantDep(Dep);};
   inline Policy &GetPolicy() {return *LocalPolicy;};
   
   // Accessors
   inline StateCache &operator [](PkgIterator const &I) {return PkgState[I->ID];};
   inline StateCache &operator [](PkgIterator const &I) const {return PkgState[I->ID];};
   inline unsigned char &operator [](DepIterator const &I) {return DepState[I->ID];};
   inline unsigned char const &operator [](DepIterator const &I) const {return DepState[I->ID];};

   /** \return A function identifying packages in the root set other
    *  than manually installed packages and essential packages, or \b
    *  NULL if an error occurs.
    *
    *  \todo Is this the best place for this function?  Perhaps the
    *  settings for mark-and-sweep should be stored in a single
    *  external class?
    */
   virtual InRootSetFunc *GetRootSetFunc();

   /** This should return const really - do not delete. */
   InRootSetFunc *GetCachedRootSetFunc() APT_HIDDEN;

   /** \return \b true if the garbage collector should follow recommendations.
    */
   virtual bool MarkFollowsRecommends();

   /** \return \b true if the garbage collector should follow suggestions.
    */
   virtual bool MarkFollowsSuggests();

   /** \brief Update the Marked and Garbage fields of all packages.
    *
    *  This routine is implicitly invoked after all state manipulators
    *  and when an ActionGroup is destroyed.  It invokes the private
    *  MarkRequired() and Sweep() to do its dirty work.
    *
    *  \param rootFunc A predicate that returns \b true for packages
    *  that should be added to the root set.
    */
   bool MarkAndSweep(InRootSetFunc &rootFunc);
   bool MarkAndSweep();

   /** Check if the phased update is ready.
    *
    * \return \b false if this is a phased update that is not yet ready for us
    */
   bool PhasingApplied(PkgIterator Pkg) const;

   /** \name State Manipulators
    */
   // @{
   bool MarkKeep(PkgIterator const &Pkg, bool Soft = false,
		 bool FromUser = true, unsigned long Depth = 0);
   bool MarkDelete(PkgIterator const &Pkg, bool MarkPurge = false,
                   unsigned long Depth = 0, bool FromUser = true);
   bool MarkInstall(PkgIterator const &Pkg,bool AutoInst = true,
		    unsigned long Depth = 0, bool FromUser = true,
		    bool ForceImportantDeps = false);
   void MarkProtected(PkgIterator const &Pkg) { PkgState[Pkg->ID].iFlags |= Protected; };

   void SetReInstall(PkgIterator const &Pkg,bool To);

   /** @return 'the' candidate version of a package
    *
    * The version returned is the version previously set explicitly via
    * SetCandidate* methods like #SetCandidateVersion or if there wasn't one
    * set the version as chosen via #Policy.
    *
    * @param Pkg is the package to return the candidate for
    */
   pkgCache::VerIterator GetCandidateVersion(pkgCache::PkgIterator const &Pkg);
   void SetCandidateVersion(VerIterator TargetVer);
   bool SetCandidateRelease(pkgCache::VerIterator TargetVer,
				std::string const &TargetRel);
   /** Set the candidate version for dependencies too if needed.
    *
    *  Sets not only the candidate version as SetCandidateVersion does,
    *  but walks also down the dependency tree and checks if it is required
    *  to set the candidate of the dependency to a version from the given
    *  release, too.
    *
    *  \param TargetVer new candidate version of the package
    *  \param TargetRel try to switch to this release if needed
    *  \param[out] Changed a list of pairs consisting of the \b old
    *              version of the changed package and the version which
    *              required the switch of this dependency
    *  \return \b true if the switch was successful, \b false otherwise
    */
   bool SetCandidateRelease(pkgCache::VerIterator TargetVer,
			    std::string const &TargetRel,
			    std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > &Changed);

   /** Set the "is automatically installed" flag of Pkg. */
   void MarkAuto(const PkgIterator &Pkg, bool Auto);
   // @}

   /** \return \b true if it's OK for MarkInstall to install
    *  the given package.
    *
    *  The default implementation simply calls all IsInstallOk*
    *  method mentioned below.
    *
    *  Overriding implementations should use the hold-state-flag to
    *  cache results from previous checks of this package - if possible.
    *
    *  The parameters are the same as in the calling MarkInstall:
    *  \param Pkg       the package that MarkInstall wants to install.
    *  \param AutoInst  install this and all its dependencies
    *  \param Depth     recursive deep of this Marker call
    *  \param FromUser  was the install requested by the user?
    */
   virtual bool IsInstallOk(const PkgIterator &Pkg,bool AutoInst = true,
			    unsigned long Depth = 0, bool FromUser = true);

   /** \return \b true if it's OK for MarkDelete to remove
    *  the given package.
    *
    *  The default implementation simply calls all IsDeleteOk*
    *  method mentioned below, see also #IsInstallOk.
    *
    *  The parameters are the same as in the calling MarkDelete:
    *  \param Pkg       the package that MarkDelete wants to remove.
    *  \param MarkPurge should we purge instead of "only" remove?
    *  \param Depth     recursive deep of this Marker call
    *  \param FromUser  was the remove requested by the user?
    */
   virtual bool IsDeleteOk(const PkgIterator &Pkg,bool MarkPurge = false,
			    unsigned long Depth = 0, bool FromUser = true);

   // read persistent states
   bool readStateFile(OpProgress * const prog);
   bool writeStateFile(OpProgress * const prog, bool const InstalledOnly=true);
   
   // Size queries
   inline signed long long UsrSize() {return iUsrSize;};
   inline unsigned long long DebSize() {return iDownloadSize;};
   inline unsigned long DelCount() {return iDelCount;};
   inline unsigned long KeepCount() {return iKeepCount;};
   inline unsigned long InstCount() {return iInstCount;};
   inline unsigned long BrokenCount() {return iBrokenCount;};
   inline unsigned long PolicyBrokenCount() {return iPolicyBrokenCount;};
   inline unsigned long BadCount() {return iBadCount;};

   bool Init(OpProgress * const Prog);
   // Generate all state information
   void Update(OpProgress * const Prog = 0);

   pkgDepCache(pkgCache * const Cache,Policy * const Plcy = 0);
   virtual ~pkgDepCache();

   bool CheckConsistency(char const *const msgtag = "");

   protected:
   // methods call by IsInstallOk
   bool IsInstallOkMultiArchSameVersionSynced(PkgIterator const &Pkg,
	 bool const AutoInst, unsigned long const Depth, bool const FromUser);
   bool IsInstallOkDependenciesSatisfiableByCandidates(PkgIterator const &Pkg,
      bool const AutoInst, unsigned long const Depth, bool const FromUser);

   // methods call by IsDeleteOk
   bool IsDeleteOkProtectInstallRequests(PkgIterator const &Pkg,
	 bool const rPurge, unsigned long const Depth, bool const FromUser);

   private:
   struct Private;
   Private *const d;

   APT_HIDDEN bool MarkInstall_StateChange(PkgIterator const &Pkg, bool AutoInst, bool FromUser);
   APT_HIDDEN bool MarkInstall_DiscardInstall(PkgIterator const &Pkg);

   APT_HIDDEN void PerformDependencyPass(OpProgress * const Prog);
};

#endif

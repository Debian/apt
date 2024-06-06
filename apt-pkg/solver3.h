/*
 * solver3.h - The APT 3.0 solver
 *
 * Copyright (c) 2023 Julian Andres Klode
 * Copyright (c) 2023 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <vector>

#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/policy.h>

namespace APT
{

/*
 * \brief APT 3.0 solver
 *
 * This is a simple solver focused on understandability and sensible results, it
 * will not generally find all solutions to the problem but will try to find the best
 * ones.
 *
 * It is a brute force solver with heuristics, conflicts learning, and 2**32 levels
 * of backtracking.
 */
class Solver
{
   enum class Decision : uint16_t;
   enum class Hint : uint16_t;
   struct Reason;
   struct CompareProviders3;
   template <typename T>
   struct State;
   struct Work;

   // \brief Groups of works, these are ordered.
   //
   // Later items will be skipped if they are optional, or we will when backtracking,
   // try a different choice for them.
   enum class Group : uint8_t
   {
      HoldOrDelete,
      NewUnsatRecommends,

      // Satisfying dependencies on entirely new packages first is a good idea because
      // it may contain replacement packages like libfoo1t64 whereas we later will see
      // Depends: libfoo1 where libfoo1t64 Provides libfoo1 and we'd have to choose.
      SatisfyNew,
      Satisfy,
      // On a similar note as for SatisfyNew, if the dependency contains obsolete packages
      // try it last.
      SatisfyObsolete,

      // My intuition tells me that we should try to schedule upgrades first, then
      // any non-obsolete installed packages, and only finally obsolete ones, such
      // that newer packages guide resolution of dependencies for older ones, they
      // may have more stringent dependencies, like a (>> 2) whereas an obsolete
      // package may have a (>> 1), for example.
      UpgradeManual,
      InstallManual,
      ObsoleteManual,

      // Automatically installed packages must come last in the group, this allows
      // us to see if they were installed as a dependency of a manually installed package,
      // allowing a simple implementation of an autoremoval code.
      UpgradeAuto,
      KeepAuto,
      ObsoleteAuto
   };

   // \brief Type to record depth at. This may very well be a 16-bit
   // unsigned integer, then change Solver::State::Decision to be a
   // uint16_t class enum as well to get a more compact space.
   using depth_type = unsigned int;

   // Documentation
   template <typename T>
   using heap = std::vector<T>;

   static_assert(sizeof(depth_type) >= sizeof(map_id_t));

   // Cache is needed to construct Iterators from Version objects we see
   pkgCache &cache;
   // Policy is needed for determining candidate version.
   pkgDepCache::Policy &policy;
   // States for packages
   std::vector<State<pkgCache::Package>> pkgStates{};
   // States for versions
   std::vector<State<pkgCache::Version>> verStates{};

   // \brief Helper function for safe access to package state.
   inline State<pkgCache::Package> &operator[](pkgCache::Package *P)
   {
      return pkgStates[P->ID];
   }

   // \brief Helper function for safe access to version state.
   inline State<pkgCache::Version> &operator[](pkgCache::Version *V)
   {
      return verStates[V->ID];
   }

   std::vector<char> verObsolete;
   bool Obsolete(pkgCache::VerIterator ver);

   // \brief Heap of the remaining work.
   //
   // We are using an std::vector with std::make_heap(), std::push_heap(),
   // and std::pop_heap() rather than a priority_queue because we need to
   // be able to iterate over the queued work and see if a choice would
   // invalidate any work.
   heap<Work> work{};
   // \brief Whether RescoreWork() actually needs to rescore the work.
   bool needsRescore{false};

   // \brief Current decision level.
   //
   // Each time a decision needs to be made we can push the item under
   // consideration to our backlog of choices made and then later we can
   // restore it easily.
   std::vector<Work> choices{};
   // \brief Backlog of solved work.
   //
   // Solved work may become invalidated when backtracking, so store it
   // here to revisit it later.
   std::vector<Work> solved{};

   /// Various configuration options
   // \brief Debug level
   int debug{_config->FindI("Debug::APT::Solver")};
   // \brief If set, we try to keep automatically installed packages installed.
   bool KeepAuto{not _config->FindB("APT::Get::AutomaticRemove")};
   // \brief If set, removals are allowed.
   bool AllowRemove{_config->FindB("APT::Solver::Remove", true)};
   // \brief If set, installs are allowed.
   bool AllowInstall{_config->FindB("APT::Solver::Install", true)};
   // \brief If set, we use strict pinning.
   bool StrictPinning{_config->FindB("APT::Solver::Strict-Pinning", true)};

   // \brief Enqueue dependencies shared by all versions of the package.
   bool EnqueueCommonDependencies(pkgCache::PkgIterator Pkg);
   // \brief Reject reverse dependencies. Must call std::make_heap() after.
   bool RejectReverseDependencies(pkgCache::VerIterator Ver);
   // \brief Enqueue a single or group
   bool EnqueueOrGroup(pkgCache::DepIterator start, pkgCache::DepIterator end, Reason reason);
   // \brief Check if a version is allowed by policy.
   bool IsAllowedVersion(pkgCache::Version *V);

   // \brief Return the current depth (choices.size() with casting)
   depth_type depth()
   {
      return static_cast<depth_type>(choices.size());
   }

   public:
   // \brief Create a new decision level.
   bool Pop();
   // \brief Revert to the previous decision level.
   void Push(Work work);
   // \brief Add work to our work queue.
   void AddWork(Work &&work);
   // \brief Rescore the work after a reject or a pop
   void RescoreWorkIfNeeded();

   // \brief Basic solver initializer. This cannot fail.
   Solver(pkgCache &Cache, pkgDepCache::Policy &Policy);

   // \brief Mark the package for install. This is annoying as it incurs a decision
   bool Install(pkgCache::PkgIterator Pkg, Reason reason, Group group);
   // \brief Install a version.
   bool Install(pkgCache::VerIterator Ver, Reason reason, Group group);
   // \brief Do not install this package
   bool Reject(pkgCache::PkgIterator Pkg, Reason reason, Group group);
   // \brief Do not install this version.
   bool Reject(pkgCache::VerIterator Ver, Reason reason, Group group);

   // \brief Apply the selections from the dep cache to the solver
   bool FromDepCache(pkgDepCache &depcache);
   // \brief Apply the solver result to the depCache
   bool ToDepCache(pkgDepCache &depcache);

   // \brief Solve the dependencies
   bool Solve();

   // Print dependency chain
   std::string WhyStr(Reason reason);
};

}; // namespace APT

/**
 * \brief Tagged union holding either a package, version, or nothing; representing the reason for installing something.
 *
 * We want to keep track of the reason why things are being installed such that
 * we can have sensible debugging abilities.
 *
 * If the reason is empty, this means the package is automatically installed.
 */
struct APT::Solver::Reason
{
   uint32_t IsVersion : 1;
   uint32_t MapPtr : 31;

   Reason() : IsVersion(0), MapPtr(0) {}
   explicit Reason(pkgCache::PkgIterator const &Pkg) : IsVersion(0), MapPtr(Pkg.MapPointer()) {}
   explicit Reason(pkgCache::VerIterator const &Ver) : IsVersion(1), MapPtr(Ver.MapPointer()) {}

   // \brief Return the package, if any, otherwise 0.
   map_pointer<pkgCache::Package> Pkg() const
   {
      return IsVersion ? 0 : map_pointer<pkgCache::Package>{(uint32_t)MapPtr};
   }
   // \brief Return the version, if any, otherwise 0.
   map_pointer<pkgCache::Version> Ver() const
   {
      return IsVersion ? map_pointer<pkgCache::Version>{(uint32_t)MapPtr} : 0;
   }
   // \brief Check if there is no reason.
   bool empty() const
   {
      return IsVersion == 0 && MapPtr == 0;
   }
};

/**
 * \brief A single work item
 *
 * A work item is a positive dependency that still needs to be resolved. Work
 * is ordered, by depth, length of solutions, and optionality.
 *
 * The work can always be recalculated from the state by iterating over dependencies
 * of all packages in there, finding solutions to them, and then adding all dependencies
 * not yet resolved to the work queue.
 */
struct APT::Solver::Work
{
   // \brief Reason for the work
   Reason reason;
   // \brief The depth at which the item has been added
   depth_type depth;
   // \brief The group we are in
   Group group;
   // \brief Possible solutions to this task, ordered in order of preference.
   std::vector<pkgCache::Version *> solutions{};

   // This is a union because we only need to store the choice we made when adding
   // to the choice vector, and we don't need the size of valid choices in there.
   union
   {
      // The choice we took
      pkgCache::Version *choice;
      // Number of valid choices
      size_t size;
   };

   // \brief Whether this is an optional work item, they will be processed last
   bool optional;
   // \brief Whether this is an ugprade
   bool upgrade;
   // \brief This item should be removed from the queue.
   bool dirty;

   bool operator<(APT::Solver::Work const &b) const;
   // \brief Dump the work item to std::cerr
   void Dump(pkgCache &cache);

   inline Work(Reason reason, depth_type depth, Group group, bool optional = false, bool upgrade = false) : reason(reason), depth(depth), group(group), size(0), optional(optional), upgrade(upgrade), dirty(false) {}
};

// \brief This essentially describes the install state in RFC2119 terms.
enum class APT::Solver::Decision : uint16_t
{
   // \brief We have not made a choice about the package yet
   NONE,
   // \brief We need to install this package
   MUST,
   // \brief We cannot install this package (need conflicts with it)
   MUSTNOT,
};

// \brief Hints for the solver about the item.
enum class APT::Solver::Hint : uint16_t
{
   // \brief We have not made a choice about the package yet
   NONE,
   // \brief This package was listed as a Recommends of a must package,
   SHOULD,
   // \brief This package was listed as a Suggests of a must-not package
   MAY,
};

/**
 * \brief The solver state
 *
 * For each version, the solver records a decision at a certain level. It
 * maintains an array mapping from version ID to state.
 */
template <typename T>
struct APT::Solver::State
{
   // \brief The reason for causing this state (invalid for NONE).
   //
   // Rejects may have been caused by a later state. Consider we select
   // between x1 and x2 in depth = N. If we now find dependencies of x1
   // leading to a conflict with a package in K < N, we will record all
   // of them as REJECT in depth = K.
   //
   // You can follow the reason chain upwards as long as the depth
   // doesn't increase to unwind.
   //
   // Reasons < 0 are package ID, reasons > 0 are version IDs.
   Reason reason{};

   // \brief The depth at which the decision has been taken
   depth_type depth{0};

   // \brief This essentially describes the install state in RFC2119 terms.
   Decision decision{Decision::NONE};

   // \brief Any hint.
   Hint hint{Hint::NONE};
};

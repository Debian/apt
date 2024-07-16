/*
 * solver3.cc - The APT 3.0 solver
 *
 * Copyright (c) 2023 Julian Andres Klode
 * Copyright (c) 2023 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#define APT_COMPILING_APT

#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/error.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/solver3.h>
#include <apt-pkg/version.h>

#include <algorithm>
#include <cassert>
#include <sstream>

// FIXME: Helpers stolen from DepCache, please give them back.
struct APT::Solver::CompareProviders3 /*{{{*/
{
   pkgCache &Cache;
   pkgDepCache::Policy &Policy;
   pkgCache::PkgIterator const Pkg;
   APT::Solver &Solver;
   bool upgrade{_config->FindB("APT::Solver::Upgrade", false)};

   bool operator()(pkgCache::Version *AV, pkgCache::Version *BV)
   {
      return (*this)(pkgCache::VerIterator(Cache, AV), pkgCache::VerIterator(Cache, BV));
   }
   bool operator()(pkgCache::VerIterator const &AV, pkgCache::VerIterator const &BV)
   {
      pkgCache::PkgIterator const A = AV.ParentPkg();
      pkgCache::PkgIterator const B = BV.ParentPkg();
      // Compare versions for the same package. FIXME: Move this to the real implementation
      if (A == B)
      {
	 if (AV == BV)
	    return false;
	 // The current version should win, unless we are upgrading and the other is the
	 // candidate.
	 // If AV is the current version, AV only wins on upgrades if BV is not the candidate.
	 if (A.CurrentVer() == AV)
	    return upgrade ? Policy.GetCandidateVer(A) != BV : true;
	 // If BV is the current version, AV only wins on upgrades if it is the candidate.
	 if (A.CurrentVer() == BV)
	    return upgrade ? Policy.GetCandidateVer(A) == AV : false;
	 // If neither are the current version, order them by priority.
	 if (Policy.GetPriority(AV) < Policy.GetPriority(BV))
	    return false;

	 return _system->VS->CmpVersion(AV.VerStr(), BV.VerStr()) > 0;
      }
      // Try obsolete choices only after exhausting non-obsolete choices such that we install
      // packages replacing them and don't keep back upgrades depending on the replacement to
      // keep the obsolete package installed.
      if (upgrade)
	 if (auto obsoleteA = Solver.Obsolete(A), obsoleteB = Solver.Obsolete(B); obsoleteA != obsoleteB)
	    return obsoleteB;
      // Prefer MA:same packages if other architectures for it are installed
      if ((AV->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same ||
	  (BV->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
      {
	 bool instA = false;
	 if ((AV->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
	 {
	    pkgCache::GrpIterator Grp = A.Group();
	    for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() == false; P = Grp.NextPkg(P))
	       if (P->CurrentVer != 0)
	       {
		  instA = true;
		  break;
	       }
	 }
	 bool instB = false;
	 if ((BV->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
	 {
	    pkgCache::GrpIterator Grp = B.Group();
	    for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() == false; P = Grp.NextPkg(P))
	    {
	       if (P->CurrentVer != 0)
	       {
		  instB = true;
		  break;
	       }
	    }
	 }
	 if (instA != instB)
	    return instA;
      }
      if ((A->CurrentVer == 0 || B->CurrentVer == 0) && A->CurrentVer != B->CurrentVer)
	 return A->CurrentVer != 0;
      // Prefer packages in the same group as the target; e.g. foo:i386, foo:amd64
      if (A->Group != B->Group)
      {
	 if (A->Group == Pkg->Group && B->Group != Pkg->Group)
	    return true;
	 else if (B->Group == Pkg->Group && A->Group != Pkg->Group)
	    return false;
      }
      // we like essentials
      if ((A->Flags & pkgCache::Flag::Essential) != (B->Flags & pkgCache::Flag::Essential))
      {
	 if ((A->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    return true;
	 else if ((B->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    return false;
      }
      if ((A->Flags & pkgCache::Flag::Important) != (B->Flags & pkgCache::Flag::Important))
      {
	 if ((A->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	    return true;
	 else if ((B->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	    return false;
      }
      // prefer native architecture
      if (strcmp(A.Arch(), B.Arch()) != 0)
      {
	 if (strcmp(A.Arch(), A.Cache()->NativeArch()) == 0)
	    return true;
	 else if (strcmp(B.Arch(), B.Cache()->NativeArch()) == 0)
	    return false;
	 std::vector<std::string> archs = APT::Configuration::getArchitectures();
	 for (std::vector<std::string>::const_iterator a = archs.begin(); a != archs.end(); ++a)
	    if (*a == A.Arch())
	       return true;
	    else if (*a == B.Arch())
	       return false;
      }
      // higher priority seems like a good idea
      if (AV->Priority != BV->Priority)
	 return AV->Priority < BV->Priority;
      if (auto NameCmp = strcmp(A.Name(), B.Name()))
	 return NameCmp < 0;
      // unable to decide…
      return A->ID > B->ID;
   }
};

/** \brief Returns \b true for packages matching a regular
 *  expression in APT::NeverAutoRemove.
 */
class DefaultRootSetFunc2 : public pkgDepCache::DefaultRootSetFunc
{
   std::unique_ptr<APT::CacheFilter::Matcher> Kernels;

   public:
   DefaultRootSetFunc2(pkgCache *cache) : Kernels(APT::KernelAutoRemoveHelper::GetProtectedKernelsFilter(cache)) {};
   virtual ~DefaultRootSetFunc2() {};

   bool InRootSet(const pkgCache::PkgIterator &pkg) APT_OVERRIDE { return pkg.end() == false && ((*Kernels)(pkg) || DefaultRootSetFunc::InRootSet(pkg)); };
}; // FIXME: DEDUP with pkgDepCache.
/*}}}*/

APT::Solver::Solver(pkgCache &cache, pkgDepCache::Policy &policy)
    : cache(cache),
      policy(policy),
      pkgStates(cache.Head().PackageCount),
      verStates(cache.Head().VersionCount),
      pkgObsolete(cache.Head().PackageCount)
{
   static_assert(sizeof(APT::Solver::State<pkgCache::PkgIterator>) == 3 * sizeof(int));
   static_assert(sizeof(APT::Solver::State<pkgCache::VerIterator>) == 3 * sizeof(int));
   static_assert(sizeof(APT::Solver::Reason) == sizeof(map_pointer<pkgCache::Package>));
   static_assert(sizeof(APT::Solver::Reason) == sizeof(map_pointer<pkgCache::Version>));
}

// This function determines if a work item is less important than another.
bool APT::Solver::Work::operator<(APT::Solver::Work const &b) const
{
   if ((not optional && size < 2) != (not b.optional && b.size < 2))
      return not b.optional && b.size < 2;
   if (group != b.group)
      return group > b.group;
   if (optional && b.optional && reason.empty() != b.reason.empty())
      return reason.empty();
   // An optional item is less important than a required one.
   if (optional != b.optional)
      return optional;
   // We enqueue common dependencies at the package level to avoid choosing versions, so let's solve package items first,
   // this improves the implication graph as it now tells you that common dependencies were installed by the package.
   if (reason.Pkg() != b.reason.Pkg())
      return reason.Pkg() == 0;

   return false;
}

void APT::Solver::Work::Dump(pkgCache &cache)
{
   if (dirty)
      std::cerr << "Dirty ";
   if (optional)
      std::cerr << "Optional ";
   std::cerr << "Item (" << ssize_t(size <= solutions.size() ? size : -1) << "@" << depth << (upgrade ? "u" : "") << ") ";
   if (auto Pkg = reason.Pkg(cache); not Pkg.end())
      std::cerr << Pkg.FullName();
   if (auto Ver = reason.Ver(cache); not Ver.end())
      std::cerr << Ver.ParentPkg().FullName() << "=" << Ver.VerStr();
   std::cerr << " -> ";
   for (auto sol : solutions)
   {
      auto Ver = pkgCache::VerIterator(cache, sol);
      std::cerr << " | " << Ver.ParentPkg().FullName() << "=" << Ver.VerStr();
   }
}

// Prints an implication graph part of the form A -> B -> C, possibly with "not"
std::string APT::Solver::WhyStr(Reason reason)
{
   std::vector<std::string> out;

   while (not reason.empty())
   {
      if (auto Pkg = reason.Pkg(cache); not Pkg.end())
      {
	 if ((*this)[Pkg].decision == Decision::MUSTNOT)
	    out.push_back(std::string("not ") + Pkg.FullName());
	 else
	    out.push_back(Pkg.FullName());
	 reason = (*this)[Pkg].reason;
      }
      if (auto Ver = reason.Ver(cache); not Ver.end())
      {
	 if ((*this)[Ver].decision == Decision::MUSTNOT)
	    out.push_back(std::string("not ") + Ver.ParentPkg().FullName() + "=" + Ver.VerStr());
	 else
	    out.push_back(Ver.ParentPkg().FullName() + "=" + Ver.VerStr());
	 reason = (*this)[Ver].reason;
      }
   }

   std::string outstr;
   for (auto I = out.rbegin(); I != out.rend(); ++I)
   {
      outstr += (outstr.size() == 0 ? "" : " -> ") + *I;
   }
   return outstr;
}

bool APT::Solver::Obsolete(pkgCache::PkgIterator pkg)
{
   auto ver = policy.GetCandidateVer(pkg);

   if (ver.end() && not StrictPinning)
      ver = pkg.VersionList();
   if (ver.end())
   {
      std::cerr << "Obsolete: " << pkg.FullName() << " - not installable\n";
      return true;
   }
   if (pkgObsolete[pkg->ID] != 0)
      return pkgObsolete[pkg->ID] == 2;
   for (auto bin = ver.Cache()->FindGrp(ver.SourcePkgName()).VersionsInSource(); not bin.end(); bin = bin.NextInSource())
      if (bin != ver && bin.ParentPkg()->Arch == ver.ParentPkg()->Arch && bin->ParentPkg != ver->ParentPkg && (not StrictPinning || policy.GetCandidateVer(bin.ParentPkg()) == bin) && _system->VS->CmpVersion(bin.SourceVerStr(), ver.SourceVerStr()) > 0)
      {
	 pkgObsolete[pkg->ID] = 2;
	 if (debug >= 3)
	    std::cerr << "Obsolete: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << " due to " << bin.ParentPkg().FullName() << "=" << bin.VerStr() << "\n";
	 return true;
      }
   for (auto file = ver.FileList(); !file.end(); file++)
      if ((file.File()->Flags & pkgCache::Flag::NotSource) == 0)
      {
	 pkgObsolete[pkg->ID] = 1;
	 return false;
      }
   if (debug >= 3)
      std::cerr << "Obsolete: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << " - not installable\n";
   pkgObsolete[pkg->ID] = 2;
   return true;
}

bool APT::Solver::Install(pkgCache::PkgIterator Pkg, Reason reason, Group group)
{
   if ((*this)[Pkg].decision == Decision::MUST)
      return true;

   // Check conflicting selections
   if ((*this)[Pkg].decision == Decision::MUSTNOT)
      return _error->Error("Conflict: %s -> %s but %s", WhyStr(reason).c_str(), Pkg.FullName().c_str(), WhyStr(Reason(Pkg)).c_str());

   bool anyInstallable = false;
   for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
      if ((*this)[ver].decision != Decision::MUSTNOT)
	 anyInstallable = true;

   if (not anyInstallable)
   {
      _error->Error("Conflict: %s -> %s but no versions are installable",
		    WhyStr(reason).c_str(), Pkg.FullName().c_str());
      for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
	 _error->Error("Uninstallable version: %s", WhyStr(Reason(ver)).c_str());
      return false;
   }

   // Note decision
   if (unlikely(debug >= 1))
      std::cerr << "[" << depth() << "] Install:" << Pkg.FullName() << " (" << WhyStr(reason) << ")\n";
   (*this)[Pkg] = {reason, depth(), Decision::MUST};

   // Insert the work item.
   Work workItem{Reason(Pkg), depth(), group};
   for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
      if (IsAllowedVersion(ver))
	 workItem.solutions.push_back(ver);
   std::stable_sort(workItem.solutions.begin(), workItem.solutions.end(), CompareProviders3{cache, policy, Pkg, *this});
   assert(workItem.solutions.size() > 0);

   if (workItem.solutions.size() > 1 || workItem.optional)
      AddWork(std::move(workItem));
   else if (not Install(pkgCache::VerIterator(cache, workItem.solutions[0]), workItem.reason, group))
      return false;

   if (not EnqueueCommonDependencies(Pkg))
      return false;

   return true;
}

bool APT::Solver::Install(pkgCache::VerIterator Ver, Reason reason, Group group)
{
   if ((*this)[Ver].decision == Decision::MUST)
      return true;

   if (unlikely(debug >= 1))
      assert(IsAllowedVersion(Ver));

   // Check conflicting selections
   if ((*this)[Ver].decision == Decision::MUSTNOT)
      return _error->Error("Conflict: %s -> %s but %s",
			   WhyStr(reason).c_str(),
			   (Ver.ParentPkg().FullName() + "=" + Ver.VerStr()).c_str(),
			   WhyStr(Reason(Ver)).c_str());
   if ((*this)[Ver.ParentPkg()].decision == Decision::MUSTNOT)
      return _error->Error("Conflict: %s -> %s but %s",
			   WhyStr(reason).c_str(),
			   (Ver.ParentPkg().FullName() + "=" + Ver.VerStr()).c_str(),
			   WhyStr(Reason(Ver.ParentPkg())).c_str());
   for (auto otherVer = Ver.ParentPkg().VersionList(); not otherVer.end(); otherVer++)
      if (otherVer->ID != Ver->ID && (*this)[otherVer].decision == Decision::MUST)
	 return _error->Error("Conflict: %s -> %s but %s",
			      WhyStr(reason).c_str(),
			      (Ver.ParentPkg().FullName() + "=" + Ver.VerStr()).c_str(),
			      WhyStr(Reason(otherVer)).c_str());

   // Note decision
   if (unlikely(debug >= 1))
      std::cerr << "[" << depth() << "] Install:" << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " (" << WhyStr(reason) << ")\n";
   (*this)[Ver] = {reason, depth(), Decision::MUST};
   if ((*this)[Ver.ParentPkg()].decision != Decision::MUST)
      (*this)[Ver.ParentPkg()] = {Reason(Ver), depth(), Decision::MUST};

   for (auto OV = Ver.ParentPkg().VersionList(); not OV.end(); ++OV)
   {
      if (OV != Ver && not Reject(OV, Reason(Ver), group))
	 return false;
   }

   for (auto dep = Ver.DependsList(); not dep.end();)
   {
      // Compute a single dependency element (glob or)
      pkgCache::DepIterator start;
      pkgCache::DepIterator end;
      dep.GlobOr(start, end); // advances dep

      if (not EnqueueOrGroup(start, end, Reason(Ver)))
	 return false;
   }

   return true;
}

bool APT::Solver::Reject(pkgCache::PkgIterator Pkg, Reason reason, Group group)
{
   if ((*this)[Pkg].decision == Decision::MUSTNOT)
      return true;

   // Check conflicting selections
   for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
      if ((*this)[ver].decision == Decision::MUST)
	 return _error->Error("Conflict: %s -> not %s but %s", WhyStr(reason).c_str(), Pkg.FullName().c_str(), WhyStr(Reason(ver)).c_str());
   if ((*this)[Pkg].decision == Decision::MUST)
      return _error->Error("Conflict: %s -> not %s but %s", WhyStr(reason).c_str(), Pkg.FullName().c_str(), WhyStr(Reason(Pkg)).c_str());

   // Reject the package and its versions.
   if (unlikely(debug >= 1))
      std::cerr << "[" << depth() << "] Reject:" << Pkg.FullName() << " (" << WhyStr(reason) << ")\n";
   (*this)[Pkg] = {reason, depth(), Decision::MUSTNOT};
   for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
      if (not Reject(ver, Reason(Pkg), group))
	 return false;

   needsRescore = true;

   return true;
}

// \brief Do not install this version
bool APT::Solver::Reject(pkgCache::VerIterator Ver, Reason reason, Group group)
{
   (void)group;

   if ((*this)[Ver].decision == Decision::MUSTNOT)
      return true;

   // Check conflicting choices.
   if ((*this)[Ver].decision == Decision::MUST)
      return _error->Error("Conflict: %s -> not %s but %s",
			   WhyStr(reason).c_str(),
			   (Ver.ParentPkg().FullName() + "=" + Ver.VerStr()).c_str(),
			   WhyStr(Reason(Ver)).c_str());

   // Mark the package as rejected and propagate up as needed.
   if (unlikely(debug >= 1))
      std::cerr << "[" << depth() << "] Reject:" << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " (" << WhyStr(reason) << ")\n";
   (*this)[Ver] = {reason, depth(), Decision::MUSTNOT};
   if (auto pkg = Ver.ParentPkg(); (*this)[pkg].decision != Decision::MUSTNOT)
   {
      bool anyInstallable = false;
      for (auto otherVer = pkg.VersionList(); not otherVer.end(); otherVer++)
	 if (otherVer->ID != Ver->ID && (*this)[otherVer].decision != Decision::MUSTNOT)
	    anyInstallable = true;

      if (anyInstallable)
	 ;
      else if ((*this)[pkg].decision == Decision::MUST) // Must install, but none available
      {
	 _error->Error("Conflict: %s but no versions are installable",
		       WhyStr(Reason(pkg)).c_str());
	 for (auto otherVer = pkg.VersionList(); not otherVer.end(); otherVer++)
	    if ((*this)[otherVer].decision == Decision::MUSTNOT)
	       _error->Error("Uninstallable version: %s", WhyStr(Reason(otherVer)).c_str());
	 return _error->Error("Uninstallable version: %s -> not %s",
			      WhyStr(reason).c_str(),
			      (Ver.ParentPkg().FullName() + "=" + Ver.VerStr()).c_str());
      }
      else if ((*this)[Ver.ParentPkg()].decision != Decision::MUSTNOT) // Last installable invalidated
	 (*this)[Ver.ParentPkg()] = {Reason(Ver), depth(), Decision::MUSTNOT};
   }

   if (not RejectReverseDependencies(Ver))
      return false;

   needsRescore = true;

   return true;
}

bool APT::Solver::EnqueueCommonDependencies(pkgCache::PkgIterator Pkg)
{
   if (not _config->FindB("APT::Solver::Enqueue-Common-Dependencies", true))
      return false;
   for (auto dep = Pkg.VersionList().DependsList(); not dep.end();)
   {
      pkgCache::DepIterator start;
      pkgCache::DepIterator end;
      dep.GlobOr(start, end); // advances dep

      bool allHaveDep = true;
      for (auto ver = Pkg.VersionList()++; not ver.end(); ver++)
      {
	 bool haveDep = false;
	 for (auto otherDep = ver.DependsList(); not haveDep && not otherDep.end(); otherDep++)
	    haveDep = otherDep->DependencyData == start->DependencyData;
	 if (!haveDep)
	    allHaveDep = haveDep;
      }
      if (not allHaveDep)
	 continue;
      if (not EnqueueOrGroup(start, end, Reason(Pkg)))
	 return false;
   }

   return true;
}

bool APT::Solver::EnqueueOrGroup(pkgCache::DepIterator start, pkgCache::DepIterator end, Reason reason)
{
   auto TgtPkg = start.TargetPkg();
   auto Ver = start.ParentVer();
   auto fixPolicy = _config->FindB("APT::Get::Fix-Policy-Broken");

   // Non-important dependencies can only be installed if they are currently satisfied, see the check further
   // below once we have calculated all possible solutions.
   if (start.ParentPkg()->CurrentVer == 0 && not policy.IsImportantDep(start))
      return true;
   // Replaces and Enhances are not a real dependency.
   if (start->Type == pkgCache::Dep::Replaces || start->Type == pkgCache::Dep::Enhances)
      return true;
   if (unlikely(debug >= 3))
      std::cerr << "Found dependency critical " << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " -> " << start.TargetPkg().FullName() << "\n";

   Work workItem{reason, depth(), Group::Satisfy, not start.IsCritical() /* optional */};

   do
   {
      auto begin = workItem.solutions.size();
      auto all = start.AllTargets();

      for (auto tgt = all; *tgt; ++tgt)
      {
	 pkgCache::VerIterator tgti(cache, *tgt);

	 if (start.IsNegative())
	 {
	    if (unlikely(debug >= 3))
	       std::cerr << "Reject: " << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " -> " << tgti.ParentPkg().FullName() << "=" << tgti.VerStr() << "\n";
	    // FIXME: We should be collecting these and marking the heap only once.
	    if (not Reject(pkgCache::VerIterator(cache, *tgt), Reason(Ver), Group::HoldOrDelete))
	       return false;
	 }
	 else
	 {
	    if (unlikely(debug >= 3))
	       std::cerr << "Adding work to  item " << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " -> " << tgti.ParentPkg().FullName() << "=" << tgti.VerStr() << "\n";
	    if (IsAllowedVersion(*tgt))
	       workItem.solutions.push_back(*tgt);
	 }
      }
      delete[] all;

      // If we are fixing the policy, we need to sort each alternative in an or group separately
      // FIXME: This is not really true, though, we should fix the CompareProviders to ignore the
      // installed state
      if (fixPolicy)
	 std::stable_sort(workItem.solutions.begin() + begin, workItem.solutions.end(), CompareProviders3{cache, policy, TgtPkg, *this});

      if (start == end)
	 break;
      ++start;
   } while (1);

   if (not fixPolicy)
      std::stable_sort(workItem.solutions.begin(), workItem.solutions.end(), CompareProviders3{cache, policy, TgtPkg, *this});

   if (std::all_of(workItem.solutions.begin(), workItem.solutions.end(), [this](auto V) -> auto
		   { return pkgCache::VerIterator(cache, V).ParentPkg()->CurrentVer == 0; }))
      workItem.group = Group::SatisfyNew;
   if (std::any_of(workItem.solutions.begin(), workItem.solutions.end(), [this](auto V) -> auto
		   { return Obsolete(pkgCache::VerIterator(cache, V).ParentPkg()); }))
      workItem.group = Group::SatisfyObsolete;
   // Try to perserve satisfied Recommends. FIXME: We should check if the Recommends was there in the installed version?
   if (workItem.optional && start.ParentPkg()->CurrentVer)
   {
      bool important = policy.IsImportantDep(start);
      bool newOptional = true;
      bool wasImportant = false;
      for (auto D = start.ParentPkg().CurrentVer().DependsList(); not D.end(); D++)
	 if (not D.IsCritical() && not D.IsNegative() && D.TargetPkg() == start.TargetPkg())
	    newOptional = false, wasImportant = policy.IsImportantDep(D);

      bool satisfied = std::any_of(workItem.solutions.begin(), workItem.solutions.end(), [this](auto ver)
				   { return pkgCache::VerIterator(cache, ver).ParentPkg()->CurrentVer != 0; });

      if (important && wasImportant && not newOptional && not satisfied)
      {
	 if (unlikely(debug >= 3))
	 {
	    std::cerr << "Ignoring unsatisfied Recommends ";
	    workItem.Dump(cache);
	    std::cerr << "\n";
	 }
	 return true;
      }
      if (not important && not wasImportant && not newOptional && satisfied)
      {
	 if (unlikely(debug >= 3))
	 {
	    std::cerr << "Promoting satisfied Suggests to Recommends: ";
	    workItem.Dump(cache);
	    std::cerr << "\n";
	 }
	 important = true;
      }
      if (not important)
      {
	 if (unlikely(debug >= 3))
	 {
	    std::cerr << "Ignoring Suggests ";
	    workItem.Dump(cache);
	    std::cerr << "\n";
	 }
	 return true;
      }
   }
   else if (workItem.optional && start.ParentPkg()->CurrentVer == 0)
      workItem.group = Group::NewUnsatRecommends;

   if (not workItem.solutions.empty())
   {
      // std::stable_sort(workItem.solutions.begin(), workItem.solutions.end(), CompareProviders3{cache, TgtPkg});
      if (unlikely(debug >= 3 && workItem.optional))
      {
	 std::cerr << "Enqueuing Recommends ";
	 workItem.Dump(cache);
	 std::cerr << "\n";
      }
      if (workItem.optional || workItem.solutions.size() > 1)
	 AddWork(std::move(workItem));
      else if (not Install(pkgCache::VerIterator(cache, workItem.solutions[0]), reason, workItem.group))
	 return false;
   }
   else if (start.IsCritical() && not start.IsNegative())
   {
      return _error->Error("Unsatisfiable dependency group %s=%s -> %s", Ver.ParentPkg().FullName().c_str(), Ver.VerStr(), TgtPkg.FullName().c_str());
   }
   return true;
}

// \brief Find the or group containing the given dependency.
static void FindOrGroup(pkgCache::DepIterator const &D, pkgCache::DepIterator &start, pkgCache::DepIterator &end)
{
   for (auto dep = D.ParentVer().DependsList(); not dep.end();)
   {
      dep.GlobOr(start, end); // advances dep

      for (auto member = start;;)
      {
	 if (member == D)
	    return;
	 if (member == end)
	    break;
	 member++;
      }
   }

   _error->Fatal("Found a dependency that does not exist in its parent version");
   abort();
}

// This is the opposite of EnqueueOrDependencies, it rejects the reverse dependencies of the
// given version iterator.
bool APT::Solver::RejectReverseDependencies(pkgCache::VerIterator Ver)
{
   // This checks whether an or group is still satisfiable.
   auto stillPossible = [this](pkgCache::DepIterator start, pkgCache::DepIterator end)
   {
      while (1)
      {
	 std::unique_ptr<pkgCache::Version *[]> Ts{start.AllTargets()};
	 for (size_t i = 0; Ts[i] != nullptr; ++i)
	    if ((*this)[Ts[i]].decision != Decision::MUSTNOT)
	       return true;

	 if (start == end)
	    return false;

	 start++;
      }
   };

   for (auto RD = Ver.ParentPkg().RevDependsList(); not RD.end(); ++RD)
   {
      auto RDV = RD.ParentVer();
      if (RD.IsNegative() || not RD.IsCritical() || not RD.IsSatisfied(Ver))
	 continue;

      if ((*this)[RDV].decision == Decision::MUSTNOT)
	 continue;

      pkgCache::DepIterator start;
      pkgCache::DepIterator end;
      FindOrGroup(RD, start, end);

      if (stillPossible(start, end))
	 continue;

      if (unlikely(debug >= 3))
	 std::cerr << "Propagate NOT " << Ver.ParentPkg().FullName() << "=" << Ver.VerStr() << " to " << RDV.ParentPkg().FullName() << "=" << RDV.VerStr() << " for dependency group starting with" << start.TargetPkg().FullName() << std::endl;

      if (not Reject(RDV, Reason(Ver), Group::HoldOrDelete))
	 return false;
   }
   return true;
}

bool APT::Solver::IsAllowedVersion(pkgCache::Version *V)
{
   pkgCache::VerIterator ver(cache, V);
   if (not StrictPinning || ver.ParentPkg().CurrentVer() == ver || policy.GetCandidateVer(ver.ParentPkg()) == ver)
      return true;

   if (unlikely(debug >= 3))
      std::cerr << "Ignoring: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << "(neither candidate nor installed)\n";
   return false;
}

void APT::Solver::Push(Work work)
{
   if (unlikely(debug >= 2))
   {
      std::cerr << "Trying choice for ";
      work.Dump(cache);
      std::cerr << "\n";
   }
   choices.push_back(std::move(work));
   // Pop() will call MergeWithStack() when reverting to level 0, or RevertToStack after dumping to the debug log.
   _error->PushToStack();
}

bool APT::Solver::Pop()
{
   auto depth = APT::Solver::depth();
   if (depth == 0)
      return false;

   if (unlikely(debug >= 2))
      for (std::string msg; _error->PopMessage(msg);)
	 std::cerr << "Branch failed: " << msg << std::endl;

   _error->RevertToStack();

   depth--;

   // Clean up the higher level states.
   // FIXME: Do not override the hints here.
   for (auto &state : pkgStates)
      if (state.depth > depth)
	 state = {};
   for (auto &state : verStates)
      if (state.depth > depth)
	 state = {};

   // This destroys the invariants that `work` must be a heap. But this is ok:
   // we are restoring the invariant below, because rejecting a package always
   // calls std::make_heap.
   work.erase(std::remove_if(work.begin(), work.end(), [depth](Work &w) -> bool
			     { return w.depth > depth || w.dirty; }),
	      work.end());
   std::make_heap(work.begin(), work.end());

   // Go over the solved items, see if any of them need to be moved back or deleted.
   solved.erase(std::remove_if(solved.begin(), solved.end(), [this, depth](Work &w) -> bool
			       {
				 if (w.depth > depth) // Deeper decision level is no longer valid.
				    return true;
				 // This item is still solved, keep it on the solved list.
				 if (std::any_of(w.solutions.begin(), w.solutions.end(), [this](auto ver)
						     { return (*this)[ver].decision == Decision::MUST; }))
				    return false;
				 // We are not longer solved, move it back to work.
				 AddWork(std::move(w));
				 return true; }),
		solved.end());

   Work w = std::move(choices.back());
   choices.pop_back();
   if (unlikely(debug >= 2))
   {
      std::cerr << "Backtracking to choice ";
      w.Dump(cache);
      std::cerr << "\n";
   }
   if (unlikely(debug >= 4))
   {
      std::cerr << "choices: ";
      for (auto &i : choices)
      {
	 std::cerr << pkgCache::VerIterator(cache, i.choice).ParentPkg().FullName(true) << "=" << pkgCache::VerIterator(cache, i.choice).VerStr();
      }
      std::cerr << std::endl;
   }

   assert(w.choice != nullptr);
   // FIXME: There should be a reason!
   if (not Reject(pkgCache::VerIterator(cache, w.choice), {}, Group::HoldOrDelete))
      return false;

   w.choice = nullptr;
   AddWork(std::move(w));
   return true;
}

void APT::Solver::AddWork(Work &&w)
{
   w.size = std::count_if(w.solutions.begin(), w.solutions.end(), [this](auto V)
			  { return (*this)[V].decision != Decision::MUSTNOT; });
   work.push_back(std::move(w));
   std::push_heap(work.begin(), work.end());
}

void APT::Solver::RescoreWorkIfNeeded()
{
   if (not needsRescore)
      return;

   needsRescore = false;
   std::vector<Work> resized;
   for (auto &w : work)
   {
      if (w.dirty)
	 continue;
      size_t newSize = std::count_if(w.solutions.begin(), w.solutions.end(), [this](auto V)
				     { return (*this)[V].decision != Decision::MUSTNOT; });

      // Notably we only insert the work into the queue if it got smaller. Work that got larger
      // we just move around when we get to it too early in Solve(). This reduces memory usage
      // at the expense of counting each item we see in Solve().
      if (newSize < w.size)
      {
	 Work newWork(w);
	 newWork.size = newSize;
	 resized.push_back(std::move(newWork));
	 w.dirty = true;
      }
   }
   if (unlikely(debug >= 2))
      std::cerr << "Rescored: " << resized.size() << "items\n";
   for (auto &w : resized)
   {
      work.push_back(std::move(w));
      std::push_heap(work.begin(), work.end());
   }
}

bool APT::Solver::Solve()
{
   while (not work.empty())
   {
      // Rescore the work if we need to
      RescoreWorkIfNeeded();
      // *NOW* we can pop the item.
      std::pop_heap(work.begin(), work.end());

      // This item has been replaced with a new one. Remove it.
      if (work.back().dirty)
      {
	 work.pop_back();
	 continue;
      }

      // If our size increased, queue again.
      size_t newSize = std::count_if(work.back().solutions.begin(), work.back().solutions.end(), [this](auto V)
				     { return (*this)[V].decision != Decision::MUSTNOT; });

      if (newSize > work.back().size)
      {
	 work.back().size = newSize;
	 std::push_heap(work.begin(), work.end());
	 continue;
      }
      assert(newSize == work.back().size);

      auto item = std::move(work.back());
      work.pop_back();
      solved.push_back(item);

      if (std::any_of(item.solutions.begin(), item.solutions.end(), [this](auto ver)
		      { return (*this)[ver].decision == Decision::MUST; }))
      {
	 if (unlikely(debug >= 2))
	 {
	    std::cerr << "ELIDED ";
	    item.Dump(cache);
	    std::cerr << "\n";
	 }
	 continue;
      }

      if (unlikely(debug >= 1))
      {
	 item.Dump(cache);
	 std::cerr << "\n";
      }

      assert(item.solutions.size() > 1 || item.optional);

      bool foundSolution = false;
      for (auto &sol : item.solutions)
      {
	 pkgCache::VerIterator ver(cache, sol);
	 if ((*this)[ver].decision == Decision::MUSTNOT)
	 {
	    if (unlikely(debug >= 3))
	       std::cerr << "(existing conflict: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << ")\n";
	    continue;
	 }
	 if (item.size > 1 || item.optional)
	 {
	    item.choice = ver;
	    Push(item);
	 }
	 if (unlikely(debug >= 3))
	    std::cerr << "(try it: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << ")\n";
	 if (not Install(pkgCache::VerIterator(cache, ver), item.reason, Group::Satisfy) && not Pop())
	    return false;
	 foundSolution = true;
	 break;
      }
      if (not foundSolution && not item.optional)
      {
	 std::ostringstream dep;
	 assert(item.solutions.size() > 0);
	 for (auto &sol : item.solutions)
	    dep << (dep.tellp() == 0 ? "" : " | ") << pkgCache::VerIterator(cache, sol).ParentPkg().FullName() << "=" << pkgCache::VerIterator(cache, sol).VerStr();
	 _error->Error("Unsatisfiable dependency: %s -> %s", WhyStr(item.reason).c_str(), dep.str().c_str());
	 for (auto &sol : item.solutions)
	    if ((*this)[sol].decision == Decision::MUSTNOT)
	       _error->Error("Not considered: %s=%s: %s", pkgCache::VerIterator(cache, sol).ParentPkg().FullName().c_str(),
			     pkgCache::VerIterator(cache, sol).VerStr(),
			     WhyStr(Reason(pkgCache::VerIterator(cache, sol))).c_str());
	 if (not Pop())
	    return false;
      }
   }

   return true;
}

// \brief Apply the selections from the dep cache to the solver
bool APT::Solver::FromDepCache(pkgDepCache &depcache)
{
   bool AllowRemoveManual = AllowRemove && _config->FindB("APT::Solver::RemoveManual", false);
   DefaultRootSetFunc2 rootSet(&cache);

   for (auto P = cache.PkgBegin(); not P.end(); P++)
   {
      if (P->VersionList == nullptr)
	 continue;

      auto state = depcache[P];
      if (P->SelectedState == pkgCache::State::Hold && not state.Protect())
      {
	 if (unlikely(debug >= 1))
	    std::cerr << "Hold " << P.FullName() << "\n";
	 if (P->CurrentVer ? not Install(P.CurrentVer(), {}, Group::HoldOrDelete) : not Reject(P, {}, Group::HoldOrDelete))
	    return false;
      }
      else if (state.Delete()						  // Normal delete request.
	       || (not P->CurrentVer && state.Keep() && state.Protect())  // Delete request of not installed package.
	       || (not P->CurrentVer && state.Keep() && not AllowInstall) // New package installs not allowed.
      )
      {
	 if (unlikely(debug >= 1))
	    std::cerr << "Delete " << P.FullName() << "\n";
	 if (!Reject(P, {}, Group::HoldOrDelete))
	    return false;
      }
      else if (state.Install() || (state.Keep() && P->CurrentVer))
      {
	 auto isEssential = P->Flags & (pkgCache::Flag::Essential | pkgCache::Flag::Important);
	 auto isAuto = (depcache[P].Flags & pkgCache::Flag::Auto);
	 auto isOptional = ((isAuto && AllowRemove) || AllowRemoveManual) && not isEssential && not depcache[P].Protect();
	 auto Root = rootSet.InRootSet(P);
	 auto Upgrade = depcache.GetCandidateVersion(P) != P.CurrentVer();
	 auto Group = isAuto ? (Upgrade ? Group::UpgradeAuto : Group::KeepAuto)
			     : (Upgrade ? Group::UpgradeManual : Group::InstallManual);

	 if (isAuto && not depcache[P].Protect() && not isEssential && not KeepAuto && not rootSet.InRootSet(P))
	 {
	    if (unlikely(debug >= 1))
	       std::cerr << "Ignore automatic install " << P.FullName() << " (" << (isEssential ? "E" : "") << (isAuto ? "M" : "") << (Root ? "R" : "") << ")"
			 << "\n";
	    continue;
	 }
	 if (unlikely(debug >= 1))
	    std::cerr << "Install " << P.FullName() << " (" << (isEssential ? "E" : "") << (isAuto ? "M" : "") << (Root ? "R" : "") << ")"
		      << "\n";

	 if (not isOptional)
	 {
	    // Pre-empt the non-optional requests, as we don't want to queue them, we can just "unit propagate" here.
	    if (depcache[P].Keep() ? not Install(P, {}, Group) : not Install(depcache.GetCandidateVersion(P), {}, Group))
	       return false;
	 }
	 else
	 {
	    Work w{Reason(), depth(), Group, isOptional, Upgrade};
	    for (auto V = P.VersionList(); not V.end(); ++V)
	       if (IsAllowedVersion(V))
		  w.solutions.push_back(V);
	    std::stable_sort(w.solutions.begin(), w.solutions.end(), CompareProviders3{cache, policy, P, *this});
	    AddWork(std::move(w));
	 }
      }
   }

   return true;
}

bool APT::Solver::ToDepCache(pkgDepCache &depcache)
{
   pkgDepCache::ActionGroup group(depcache);
   for (auto P = cache.PkgBegin(); not P.end(); P++)
   {
      depcache[P].Marked = 0;
      depcache[P].Garbage = 0;
      if ((*this)[P].decision == Decision::MUST)
      {
	 for (auto V = P.VersionList(); not V.end(); V++)
	 {
	    if ((*this)[V].decision == Decision::MUST)
	    {
	       depcache.SetCandidateVersion(V);
	       break;
	    }
	 }
	 auto reason = (*this)[depcache.GetCandidateVersion(P)].reason;
	 if (auto RP = reason.Pkg(); RP == P.MapPointer())
	    reason = (*this)[P].reason;

	 depcache.MarkInstall(P, false, 0, reason.empty());
	 if (not P->CurrentVer)
	    depcache.MarkAuto(P, not reason.empty());
	 depcache[P].Marked = 1;
	 depcache[P].Garbage = 0;
      }
      else if (P->CurrentVer || depcache[P].Install())
      {
	 depcache.MarkDelete(P, false, 0, (*this)[P].reason.empty());
	 depcache[P].Marked = 0;
	 depcache[P].Garbage = 1;
      }
   }
   return true;
}

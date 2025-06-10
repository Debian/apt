/*
 * solver3.cc - The APT 3.0 solver
 *
 * Copyright (c) 2023 Julian Andres Klode
 * Copyright (c) 2023 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This solver started from scratch but turns slowly into a variant of
 * MiniSat as documented in the paper
 *    "An extensible SAT-solver [extended version 1.2]."
 * by Niklas Eén, and Niklas Sörensson.
 *
 * It extends MiniSAT with support for optional clauses, and differs
 * in that it removes non-deterministic aspects like the activity based
 * ordering. Instead it uses a more nuanced static ordering that, to
 * some extend, preserves some greediness and sub-optimality of the
 * classic APT solver.
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

#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// FIXME: Helpers stolen from DepCache, please give them back.
struct APT::Solver::CompareProviders3 /*{{{*/
{
   pkgCache &Cache;
   pkgDepCache::Policy &Policy;
   pkgCache::PkgIterator const Pkg;
   APT::Solver &Solver;

   pkgCache::VerIterator bestVersion(pkgCache::PkgIterator pkg)
   {
      pkgCache::VerIterator res = pkg.VersionList();
      for (auto v = res; not v.end(); ++v)
	 res = std::max(res, v, *this);
      return res;
   }
   bool operator()(Var a, Var b)
   {
      pkgCache::VerIterator va = a.Ver(Cache);
      pkgCache::VerIterator vb = b.Ver(Cache);
      if (auto pa = a.Pkg(Cache))
	 va = bestVersion(pa);
      if (auto pb = b.Pkg(Cache))
	 vb = bestVersion(pb);

      assert(not va.end() && not vb.end());
      return (*this)(va, vb);
   }
   bool operator()(pkgCache::VerIterator const &AV, pkgCache::VerIterator const &BV)
   {
      assert(not AV.end() && not BV.end());
      pkgCache::PkgIterator const A = AV.ParentPkg();
      pkgCache::PkgIterator const B = BV.ParentPkg();
      // Compare versions for the same package. FIXME: Move this to the real implementation
      if (A == B)
      {
	 if (AV == BV)
	    return false;

	 // Candidate wins in upgrade scenario
	 if (Solver.IsUpgrade)
	 {
	    auto Cand = Solver.GetCandidateVer(A);
	    if (AV == Cand || BV == Cand)
	       return (AV == Cand);
	 }

	 // Installed version wins otherwise
	 if (A.CurrentVer() == AV || B.CurrentVer() == BV)
	    return (A.CurrentVer() == AV);

	 // Rest is ordered list, first by priority
	 if (auto pinA = Solver.GetPriority(AV), pinB = Solver.GetPriority(BV); pinA != pinB)
	    return pinA > pinB;

	 // Then by version
	 return _system->VS->CmpVersion(AV.VerStr(), BV.VerStr()) > 0;
      }
      // Try obsolete choices only after exhausting non-obsolete choices such that we install
      // packages replacing them and don't keep back upgrades depending on the replacement to
      // keep the obsolete package installed.
      if (Solver.IsUpgrade)
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
      if (A->Group != B->Group && not Pkg.end())
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
   ~DefaultRootSetFunc2() override = default;

   bool InRootSet(const pkgCache::PkgIterator &pkg) override { return pkg.end() == false && ((*Kernels)(pkg) || DefaultRootSetFunc::InRootSet(pkg)); };
}; // FIXME: DEDUP with pkgDepCache.
/*}}}*/

APT::Solver::Solver(pkgCache &cache, pkgDepCache::Policy &policy, EDSP::Request::Flags requestFlags)
    : cache(cache),
      policy(policy),
      rootState(new State),
      pkgStates(cache),
      verStates(cache),
      pkgObsolete(cache),
      priorities(cache),
      candidates(cache),
      requestFlags(requestFlags)
{
   // Ensure trivially
   static_assert(std::is_trivially_destructible_v<Work>);
   static_assert(std::is_trivially_destructible_v<Solved>);
   static_assert(sizeof(APT::Solver::Var) == sizeof(map_pointer<pkgCache::Package>));
   static_assert(sizeof(APT::Solver::Var) == sizeof(map_pointer<pkgCache::Version>));
   // Root state is "true".
   rootState->decision = Decision::MUST;
}

APT::Solver::~Solver() = default;

// This function determines if a work item is less important than another.
bool APT::Solver::Work::operator<(APT::Solver::Work const &b) const
{
   if ((not clause->optional && size < 2) != (not b.clause->optional && b.size < 2))
      return not b.clause->optional && b.size < 2;
   if (clause->optional != b.clause->optional)
      return clause->optional;
   if (clause->group != b.clause->group)
      return clause->group > b.clause->group;
   if ((size < 2) != (b.size < 2))
      return b.size < 2;
   if (size == 1 && b.size == 1) // Special case: 'shortcircuit' optional packages
      return clause->solutions.size() < b.clause->solutions.size();
   return false;
}

std::string APT::Solver::Clause::toString(pkgCache &cache, bool pretty, bool showMerged) const
{
   std::string out;
   if (showMerged)
      out.append(reason.toString(cache));
   if (dep && pretty)
   {
      out.append(" ").append(pkgCache::DepIterator(cache, dep).DepType()).append(" ");
      for (auto dep = pkgCache::DepIterator(cache, this->dep); not dep.end(); ++dep)
      {
	 out.append(dep.TargetPkg().FullName(true));
	 if (dep.TargetVer())
	    out.append(" (").append(dep.CompType()).append(" ").append(dep.TargetVer()).append(")");
	 if (!(dep->CompareOp & pkgCache::Dep::Or))
	    break;
	 out.append(" | ");
      }
   }
   else if (pretty && group == Group::SelectVersion && negative)
      out.append(" conflicts with other versions of itself");
   else if (pretty && group == Group::SelectVersion && reason.Pkg()) {
      out.append(solutions.size() > 1 ? " is available in versions " : " is available in version ");
      for (auto var : solutions) {
	 assert(var.Ver());
	 if (var != solutions.front())
	    out.append(", ");
	 out.append(var.Ver(cache).VerStr());
      }
   }
   else
   {
      out.append(" -> ");
      for (auto var : solutions)
	 out.append(" | ").append(var.toString(cache));
   }
   if (showMerged && not merged.empty())
   {
      for (auto &clause : merged)
      {
	 out.append(" and");
	 out.append(clause.toString(cache, pretty, false));
      }
   }
   return out;
}

std::string APT::Solver::Work::toString(pkgCache &cache) const
{
   std::ostringstream out;
   if (erased)
      out << "Erased ";
   if (clause->optional)
      out << "Optional ";
   out << "Item (" << ssize_t(size <= clause->solutions.size() ? size : -1) << "@" << depth << ") ";
   out << clause->toString(cache);
   return out.str();
}

inline APT::Solver::Var APT::Solver::bestReason(APT::Solver::Clause const *clause, APT::Solver::Var var) const
{
   if (not clause)
      return Var{};
   if (clause->reason == var)
      for (auto choice : clause->solutions)
      {
	 if (clause->negative && (*this)[choice].decision == Decision::MUST)
	    return choice;
	 if (not clause->negative && (*this)[choice].decision == Decision::MUSTNOT)
	    return choice;
      }
   return clause->reason;
}

// Prints an implication graph part of the form A -> B -> C, possibly with "not"
std::string APT::Solver::WhyStr(Var reason) const
{
   std::vector<std::string> out;
   while (not reason.empty())
   {
      if ((*this)[reason].decision == Decision::MUSTNOT)
	 out.push_back(std::string("not ") + reason.toString(cache));
      else
	 out.push_back(reason.toString(cache));
      reason = bestReason((*this)[reason].reason, reason);
   }

   std::string outstr;
   for (auto I = out.rbegin(); I != out.rend(); ++I)
   {
      outstr += (outstr.size() == 0 ? "" : " -> ") + *I;
   }
   return outstr;
}

std::string APT::Solver::LongWhyStr(Var var, bool decision, const Clause *rclause, std::string prefix, std::unordered_set<Var> &seen) const
{
   std::ostringstream out;

   // Helper function to nicely print more details than just "install/do not install", such as "removal", "upgrade", "downgrade", "install"
   auto printSelection = [this](Var var, bool decision)
   {
      std::string s;
      if (auto pkg = var.Pkg(cache); not decision && pkg && pkg->CurrentVer)
	 strprintf(s, "%s is selected for removal", var.toString(cache).c_str());
      else if (auto ver = var.Ver(cache); decision && ver && ver.ParentPkg().CurrentVer() && ver.ParentPkg().CurrentVer() != ver)
      {
	 if (cache.VS->CmpVersion(ver.ParentPkg().CurrentVer().VerStr(), ver.VerStr()) < 0)
	    strprintf(s, "%s is selected as an upgrade", var.toString(cache).c_str());
	 else
	    strprintf(s, "%s is selected as a downgrade", var.toString(cache).c_str());
      }
      else if (not decision)
	 strprintf(s, "%s is not selected for install", var.toString(cache).c_str());
      else
	 strprintf(s, "%s is selected for install", var.toString(cache).c_str());
      return s;
   };

   // Helper: Recurse into all of the children of the clause and print the decision for them.
   auto recurseChildren = [&](const Clause *clause, Var skip = Var())
   {
      if (clause->solutions.empty())
	 out << prefix << "[no choices]\n";
      for (auto choice : clause->solutions)
      {
	 if (choice == skip)
	    continue;

	 if ((*this)[choice].decision == Decision::NONE)
	    out << prefix << "- " << choice.toString(cache) << " is undecided\n";
	 else
	    out << prefix << "- " << LongWhyStr(choice, (*this)[choice].decision == Decision::MUST, (*this)[choice].reason, prefix + "  ", seen).substr(prefix.size() + 2);
      }
   };

   // Inverse version selection clauses that select the package if the version is selected,
   // such as pkg=ver -> pkg, are irrelevant for the user, skip them
   if (var.Pkg() && decision && rclause && rclause->group == Group::SelectVersion)
   {
      var = rclause->reason;
      rclause = (*this)[var].reason;
   }

   // No reason given, probably a user request or manually installed or essential or whatnot.
   if (not rclause)
   {
      out << prefix << printSelection(var, decision) << "\n";
      return out.str();
   }

   // We could be called with a decision we tried to make but failed due to a conflict;
   // this checks if it is the real decision.
   if ((*this)[var].decision != Decision::NONE && decision == ((*this)[var].decision == Decision::MUST) && (*this)[var].reason == rclause)
   {
      // If we have seen the real decision before; we dont't need to print it again.
      if (seen.find(var) != seen.end())
      {
	 out << prefix << printSelection(var, decision) << " as above\n";
	 return out.str();
      }
      seen.insert(var);
   }

   // A package was decided "not install" due to a positive clause, so the clause is unsat.
   if (not decision && rclause && not rclause->negative)
   {
      out << prefix << rclause->toString(cache, true) << "\n";
      out << prefix << "but none of the choices are installable:\n";
      recurseChildren(rclause);
      return out.str();
   }

   // Build the strongest path from a root to our decision leaf
   std::vector<Var> path;
   for (auto reason = bestReason(rclause, var); not reason.empty(); reason = bestReason((*this)[reason].reason, reason))
      path.push_back(reason);

   // Render the strong reasoning path
   out << prefix << printSelection(var, decision) << " because:\n";
   auto w = std::to_string(path.size() + 1).size();
   size_t i = 1;
   for (auto it = path.rbegin(); it != path.rend(); ++it, ++i)
   {
      auto const &state = (*this)[*it];
      // Don't print version selection clauses
      if (it->Pkg() && state.reason && state.reason->group == Group::SelectVersion)
      {
	 --i;
	 continue;
      }
      if (seen.find(*it) != seen.end())
      {
	 if ((it + 1) == path.rend() || seen.find(*(it + 1)) == seen.end())
	 {
	    out << prefix << (i == 1 ? "" : "1-") << i << ". " << printSelection(*it, state.decision == Decision::MUST) << " as above\n";
	 }
	 continue;
      }
      seen.insert(*it);
      if (state.reason)
      {
	 out << prefix << std::setw(w) << i << ". " << state.reason->toString(cache, true) << "\n";
	 if (state.reason->solutions.size() > 1)
	    out << prefix << std::setw(w) << " " << "  [selected " << it->toString(cache) << " for " << (state.decision == Decision::MUST ? "install" : "remove") << "]\n";
      }
      else
	 out << prefix << std::setw(w) << i << ". " << printSelection(*it, state.decision == Decision::MUST) << "\n";
   }

   // Print the leaf. We can't have the leaf in the path because we might be called for an attempted decision
   // that conflicts with the actual assignment (to simplify: we marked X for not install, then we process Y depends X
   // and try to mark X and reach the conflict, we are called with "X" and the "Y depends X" clause).
   out << prefix << std::setw(w) << i << ". " << rclause->toString(cache, true) << "\n";
   if (rclause->solutions.size() > 1)
      out << prefix << std::setw(w) << " " << "  " << "[selected " << bestReason(rclause, var).toString(cache) << "]\n";

   bool firstContext = true;
   // Show alternative paths not taken. Consider you have Y depends X|Z; and you mark X
   // for it; we show above Y -> X as the path, but here we go show why Z was not considered in "Y depends X|Z".
   for (auto it = path.rbegin(); it != path.rend(); ++it)
   {
      auto const &state = (*this)[*it];
      if (not state.reason) // If we have no reason, we don't have alternatives
	 continue;
      if (state.reason->solutions.size() <= 1) // Nothing to print if we no alternatives
	 continue;
      if (state.reason->negative) // We only actually need one conflicting choice, ignore others
	 continue;

      if (firstContext)
      {
	 out << prefix << "For context, additional choices that could not be installed:" << "\n";
      }

      firstContext = false;
      out << prefix << "* In " << state.reason->toString(cache, true) << ":\n";
      prefix += "  ";
      recurseChildren(state.reason, *it);
      prefix.resize(prefix.size() - 2);
   }
   if (rclause->solutions.size() > 1 && not rclause->negative)
   {
      if (firstContext)
      {
	 out << prefix << "For context, additional choices that could not be installed:" << "\n";
      }
      firstContext = false;
      out << prefix << "* In " << rclause->toString(cache, true) << ":\n";
      prefix += "  ";
      recurseChildren(rclause, var);
      prefix.resize(prefix.size() - 2);
   }

   return out.str();
}

// This is essentially asking whether any other binary in the source package has a higher candidate
// version. This pretends that each package is installed at the same source version as the package
// under consideration.
bool APT::Solver::ObsoletedByNewerSourceVersion(pkgCache::VerIterator cand) const
{
   const auto pkg = cand.ParentPkg();
   const int candPriority = GetPriority(cand);

   for (auto ver = cand.Cache()->FindGrp(cand.SourcePkgName()).VersionsInSource(); not ver.end(); ver = ver.NextInSource())
   {
      // We are only interested in other packages in the same source package; built for the same architecture.
      if (ver->ParentPkg == cand->ParentPkg || ver.ParentPkg()->Arch != cand.ParentPkg()->Arch ||
	    (ver->MultiArch & pkgCache::Version::All) != (cand->MultiArch & pkgCache::Version::All) ||
	     cache.VS->CmpVersion(ver.SourceVerStr(), cand.SourceVerStr()) <= 0)
	 continue;

      // We also take equal priority here, given that we have a higher version
      const int priority = GetPriority(ver);
      if (priority == 0 || priority < candPriority)
	 continue;

      pkgObsolete[pkg] = 2;
      if (debug >= 3)
	 std::cerr << "Obsolete: " << cand.ParentPkg().FullName() << "=" << cand.VerStr() << " due to " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << "\n";
      return true;
   }

   return false;
}

bool APT::Solver::Obsolete(pkgCache::PkgIterator pkg, bool AllowManual) const
{
   if ((*this)[pkg].flags.manual && not AllowManual)
      return false;
   if (pkgObsolete[pkg] != 0)
      return pkgObsolete[pkg] == 2;

   auto ver = GetCandidateVer(pkg);

   if (ver.end() && not StrictPinning)
      ver = pkg.VersionList();
   if (ver.end())
   {
      if (debug >= 3)
	 std::cerr << "Obsolete: " << pkg.FullName() << " - not installable\n";
      pkgObsolete[pkg] = 2;
      return true;
   }

   if (ObsoletedByNewerSourceVersion(ver))
      return true;

   if (ver.Downloadable())
   {
      pkgObsolete[pkg] = 1;
      return false;
   }
   if (debug >= 3)
      std::cerr << "Obsolete: " << ver.ParentPkg().FullName() << "=" << ver.VerStr() << " - not installable\n";
   pkgObsolete[pkg] = 2;
   return true;
}
bool APT::Solver::Assume(Var var, bool decision, const Clause *reason)
{
   choices.push_back(solved.size());
   return Enqueue(var, decision, std::move(reason));
}

bool APT::Solver::Enqueue(Var var, bool decision, const Clause *reason)
{
   auto &state = (*this)[var];
   auto decisionCast = decision ? Decision::MUST : Decision::MUSTNOT;

   if (state.decision != Decision::NONE)
   {
      if (state.decision != decisionCast)
      {
	 std::ostringstream err;
	 err << "Unable to satisfy dependencies. Reached two conflicting decisions:" << "\n";
	 std::unordered_set<Var> seen;
	 err << "1. " << LongWhyStr(var, state.decision == Decision::MUST, state.reason, "   ", seen).substr(3) << "\n";
	 err << "2. " << LongWhyStr(var, decision, reason, "   ", seen).substr(3);
	 return _error->Error("%s", err.str().c_str());
      }
      return true;
   }

   state.decision = decisionCast;
   state.depth = depth();
   state.reason = reason;

   if (unlikely(debug >= 1))
      std::cerr << "[" << depth() << "] " << (decision ? "Install" : "Reject") << ":" << var.toString(cache) << " (" << WhyStr(bestReason(reason, var)) << ")\n";

   solved.push_back(Solved{var, std::nullopt});
   propQ.push(var);

   return true;
}

bool APT::Solver::Propagate()
{
   while (!propQ.empty())
   {
      Var var = propQ.front();
      propQ.pop();
      if ((*this)[var].decision == Decision::MUST)
      {
	 Discover(var);
	 for (auto &clause : (*this)[var].clauses)
	    if (not AddWork(Work{clause.get(), depth()}))
	       return false;
	 for (auto rclause : (*this)[var].rclauses)
	 {
	    if (not rclause->negative || rclause->optional || rclause->reason.empty())
	       continue;
	    if (unlikely(debug >= 3))
	       std::cerr << "Propagate " << var.toString(cache) << " to NOT " << rclause->reason.toString(cache) << " for dep " << const_cast<Clause *>(rclause)->toString(cache) << std::endl;
	    if (not Enqueue(rclause->reason, false, rclause))
	       return false;
	 }
      }
      else if ((*this)[var].decision == Decision::MUSTNOT)
      {
	 for (auto rclause : (*this)[var].rclauses)
	 {
	    if (rclause->negative || rclause->reason.empty())
	       continue;
	    if ((*this)[rclause->reason].decision == Decision::MUSTNOT)
	       continue;

	    auto count = std::count_if(rclause->solutions.begin(), rclause->solutions.end(), [this](auto var)
				       { return (*this)[var].decision != Decision::MUSTNOT; });

	    if (count == 1 && (*this)[rclause->reason].decision == Decision::MUST)
	    {
	       if (unlikely(debug >= 3))
		  std::cerr << "Propagate NOT " << var.toString(cache) << " to unit clause " << rclause->toString(cache);
	       if (rclause->optional)
	       {
		  // Enqueue duplicated item, this will ensure we see it at the correct time
		  if (not AddWork(Work{rclause, depth()}))
		     return false;
	       }
	       else
	       {
		  // Find the variable that must be chosen and enqueue it as a fact
		  for (auto sol : rclause->solutions)
		     if ((*this)[sol].decision == Decision::NONE && not Enqueue(sol, true, rclause))
			return false;
	       }
	       continue;
	    }
	    if (count >= 1 || rclause->optional)
	       continue;

	    if (unlikely(debug >= 3))
	       std::cerr << "Propagate NOT " << var.toString(cache) << " to " << rclause->reason.toString(cache) << " for dep " << const_cast<Clause *>(rclause)->toString(cache) << std::endl;

	    if (not Enqueue(rclause->reason, false, rclause)) // Last version invalidated
	       return false;
	 }
      }
   }
   return true;
}

static bool SameOrGroup(pkgCache::DepIterator a, pkgCache::DepIterator b)
{
   while (1)
   {
      if (a->DependencyData != b->DependencyData)
	 return false;

      // At least one has reached the end, break
      if (not(a->CompareOp & pkgCache::Dep::Or) || not(b->CompareOp & pkgCache::Dep::Or))
	 break;

      ++a, ++b;
   }
   // Fully iterated over a and b
   return not(a->CompareOp & pkgCache::Dep::Or) && not(b->CompareOp & pkgCache::Dep::Or);
}

const APT::Solver::Clause *APT::Solver::RegisterClause(Clause &&clause)
{
   auto &clauses = (*this)[clause.reason].clauses;
   pkgCache::DepIterator dep(cache, clause.dep);

   // Merge dependencies on the same name into a single one, and restrict their solution space.
   // For example, given dependencies on
   //	 bar Depends: pkg (<< 3), pkg (>> 2)
   //	 foo Provides: pkg (= 1)
   // The solution must always be pkg (= 2) and not say pkg (= 3), foo.
   // FIXME: This would be nice to merge across or groups too, but we can't do that yet.
   if (not clause.negative && not dep.end() && not(dep->CompareOp & pkgCache::Dep::Or))
   {
      bool merged = false;
      for (auto const &earlierClause : clauses)
      {
	 if (earlierClause->negative)
	    continue;
	 // Skip dependencies with or groups or dependencies on different names
	 if (pkgCache::DepIterator earlierDep(cache, earlierClause->dep);
	     earlierDep.end() || (earlierDep->CompareOp & pkgCache::Dep::Or) ||
	     earlierDep.TargetPkg() != dep.TargetPkg())
	    continue;
	 if (std::none_of(earlierClause->solutions.begin(), earlierClause->solutions.end(), [&clause](auto earlierSol)
			  { return std::find(clause.solutions.begin(),
					     clause.solutions.end(),
					     earlierSol) != clause.solutions.end(); }))
	    continue;

	 if (earlierClause->optional == clause.optional)
	 {
	    std::erase_if(earlierClause->solutions, [&clause, this](auto earlierSol)
			  { return std::find(clause.solutions.begin(),
					     clause.solutions.end(),
					     earlierSol) == clause.solutions.end(); });

	    earlierClause->merged.push_front(clause);
	    merged = true;
	 }
	 else if (clause.optional)
	 {
	    // If say a Depends has fewer solution than a Recommends, remove the Recommend's extranous ones.
	    std::erase_if(clause.solutions, [&earlierClause, this](auto sol)
			  { return std::find(earlierClause->solutions.begin(),
					     earlierClause->solutions.end(),
					     sol) == earlierClause->solutions.end(); });

	    // Remove recursion here, such that we display correctly (if we ever display anywhere...)
	    auto earlierClauseCopy = *earlierClause;
	    clause.merged = std::move(earlierClauseCopy.merged);
	    clause.merged.push_front(earlierClauseCopy);
	 }
      }

      if (merged)
	 return nullptr;
   }

   clauses.push_back(std::make_unique<Clause>(std::move(clause)));
   auto const &inserted = clauses.back();
   for (auto var : inserted->solutions)
      (*this)[var].rclauses.push_back(inserted.get());
   return inserted.get();
}

void APT::Solver::Discover(Var var)
{
   assert(discoverQ.empty());
   discoverQ.push(var);

   while (not discoverQ.empty())
   {
      var = discoverQ.front();
      discoverQ.pop();

      // Package needs to be discovered before the version to be able to dedup shared dependencies
      if (auto Ver = var.Ver(cache); not Ver.end() && not(*this)[Ver.ParentPkg()].flags.discovered)
	 var = Var(Ver.ParentPkg());

      auto &state = (*this)[var];

      if (state.flags.discovered)
	 continue;

      state.flags.discovered = true;

      if (auto Pkg = var.Pkg(cache); not Pkg.end())
      {
	 Clause clause{Var(Pkg), Group::SelectVersion};
	 for (auto ver = Pkg.VersionList(); not ver.end(); ver++)
	    clause.solutions.push_back(Var(ver));

	 std::stable_sort(clause.solutions.begin(), clause.solutions.end(), CompareProviders3{cache, policy, Pkg, *this});
	 RegisterClause(std::move(clause));

	 RegisterCommonDependencies(Pkg);
      }
      else if (auto Ver = var.Ver(cache); not Ver.end())
      {
	 Clause clause{Var(Ver), Group::SelectVersion};
	 clause.solutions = {Var(Ver.ParentPkg())};
	 RegisterClause(std::move(clause));

	 for (auto OV = Ver.ParentPkg().VersionList(); not OV.end(); ++OV)
	 {
	    if (OV == Ver)
	       continue;

	    Clause clause{Var(Ver), Group::SelectVersion, false, true /* negative */};
	    clause.solutions = {Var(OV)};
	    RegisterClause(std::move(clause));
	 }

	 for (auto dep = Ver.DependsList(); not dep.end();)
	 {
	    // Compute a single dependency element (glob or)
	    pkgCache::DepIterator start;
	    pkgCache::DepIterator end;
	    dep.GlobOr(start, end); // advances dep

	    // This dependency is shared across all versions, skip it.
	    if (auto &pkgClauses = (*this)[Ver.ParentPkg()].clauses;
		std::any_of(pkgClauses.begin(), pkgClauses.end(), [this, start](auto &c)
			    { return c->dep && SameOrGroup(start, pkgCache::DepIterator(cache, c->dep)); }))
	       continue;

	    auto clause = TranslateOrGroup(start, end, Var(Ver));

	    RegisterClause(std::move(clause));
	 }
      }

      // Recursively discover everything else that is not already FALSE by fact (MUSTNOT at depth 0)
      for (auto const &clause : state.clauses)
	 for (auto const &var : clause->solutions)
	    if ((*this)[var].decision != Decision::MUSTNOT || (*this)[var].depth > 0)
	       discoverQ.push(var);
   }
}

void APT::Solver::RegisterCommonDependencies(pkgCache::PkgIterator Pkg)
{
   for (auto dep = Pkg.VersionList().DependsList(); not dep.end();)
   {
      pkgCache::DepIterator start, end;
      dep.GlobOr(start, end); // advances dep

      bool allHaveDep = true;
      for (auto ver = Pkg.VersionList()++; allHaveDep && not ver.end(); ver++)
      {
	 bool haveDep = false;
	 for (auto otherDep = ver.DependsList(); not haveDep && not otherDep.end();)
	 {
	    pkgCache::DepIterator otherStart, otherEnd;
	    otherDep.GlobOr(otherStart, otherEnd); // advances other dep
	    haveDep = SameOrGroup(start, otherStart);
	 }
	 if (not haveDep)
	    allHaveDep = false;
      }
      if (not allHaveDep)
	 continue;
      auto clause = TranslateOrGroup(start, end, Var(Pkg));
      RegisterClause(std::move(clause));
   }
}

APT::Solver::Clause APT::Solver::TranslateOrGroup(pkgCache::DepIterator start, pkgCache::DepIterator end, Var reason)
{
   auto TgtPkg = start.TargetPkg();

   // Non-important dependencies can only be installed if they are currently satisfied, see the check further
   // below once we have calculated all possible solutions.
   if (start.ParentPkg()->CurrentVer == 0 && not policy.IsImportantDep(start))
      return Clause{reason, Group::Satisfy, true};
   // Replaces and Enhances are not a real dependency.
   if (start->Type == pkgCache::Dep::Replaces || start->Type == pkgCache::Dep::Enhances)
      return Clause{reason, Group::Satisfy, true};
   if (unlikely(debug >= 3))
      std::cerr << "Found dependency critical " << reason.toString(cache) << " -> " << start.TargetPkg().FullName() << "\n";

   Clause clause{reason, Group::Satisfy, not start.IsCritical() /* optional */, start.IsNegative()};

   clause.dep = start;

   do
   {
      auto begin = clause.solutions.size();

      if (DeferVersionSelection && not start.IsNegative() && start.TargetPkg().ProvidesList().end() && start.IsSatisfied(start.TargetPkg()))
      {
	 clause.solutions.push_back(Var(start.TargetPkg()));
      }
      else
      {
	 auto all = start.AllTargets();

	 for (auto tgt = all; *tgt; ++tgt)
	 {
	    pkgCache::VerIterator tgti(cache, *tgt);

	    if (unlikely(debug >= 3))
	       std::cerr << "Adding work to  item " << reason.toString(cache) << " -> " << tgti.ParentPkg().FullName() << "=" << tgti.VerStr() << (clause.negative ? " (negative)" : "") << "\n";
	    clause.solutions.push_back(Var(pkgCache::VerIterator(cache, *tgt)));
	 }
	 delete[] all;

	 std::stable_sort(clause.solutions.begin() + begin, clause.solutions.end(), CompareProviders3{cache, policy, TgtPkg, *this});
      }
      if (start == end)
	 break;
      ++start;
   } while (1);

   // Move obsolete packages to the end, and (non-obsolete) installed packages to the front
   if (not FixPolicyBroken)
      std::stable_sort(clause.solutions.begin(), clause.solutions.end(), [this](Var a, Var b)
		       {
	    if (IsUpgrade)
	       if (auto obsoleteA = Obsolete(a.CastPkg(cache)), obsoleteB = Obsolete(b.CastPkg(cache)); obsoleteA != obsoleteB)
		  return obsoleteB;
	    if ((a.CastPkg(cache)->CurrentVer == 0 || b.CastPkg(cache)->CurrentVer == 0) && a.CastPkg(cache)->CurrentVer != b.CastPkg(cache)->CurrentVer)
	       return a.CastPkg(cache)->CurrentVer != 0;
	    return false; });

   if (std::all_of(clause.solutions.begin(), clause.solutions.end(), [this](auto var) -> auto
		   { return var.CastPkg(cache)->CurrentVer == 0; }))
      clause.group = Group::SatisfyNew;
   if (std::any_of(clause.solutions.begin(), clause.solutions.end(), [this](auto var) -> auto
		   { return Obsolete(var.CastPkg(cache), true); }))
      clause.group = Group::SatisfyObsolete;
   // Try to perserve satisfied Recommends. FIXME: We should check if the Recommends was there in the installed version?
   if (clause.optional && start.ParentPkg()->CurrentVer)
   {
      bool important = policy.IsImportantDep(start);
      auto importantToKeep = [this](pkgCache::DepIterator d)
      {
	 return policy.IsImportantDep(d) || (KeepRecommends && d->Type == pkgCache::Dep::Recommends) || (KeepSuggests && d->Type == pkgCache::Dep::Suggests);
      };
      bool satisfied = std::any_of(clause.solutions.begin(), clause.solutions.end(), [this](auto var)
				   { return var.Pkg(cache) ? var.Pkg(cache)->CurrentVer != nullptr : Var(var.CastPkg(cache).CurrentVer()) == var; });

      // Find the existing dependency
      pkgCache::DepIterator existing;
      for (auto D = start.ParentPkg().CurrentVer().DependsList(); not D.end(); D++)
	 if (not D.IsCritical() && importantToKeep(D) && D.TargetPkg() == start.TargetPkg())
	 {
	    existing = D;
	    break;
	 }

      if (not existing.end() && not important && importantToKeep(start) && satisfied)
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Try to keep satisfied: " << clause.toString(cache, true) << std::endl;
	 clause.group = Group::SatisfySuggests;
	 // Erase the non-installed solutions. We will process this last and try to keep the previously installed
	 // "best" solution installed.
	 clause.solutions.erase(std::remove_if(clause.solutions.begin(), clause.solutions.end(), [this](auto var)
					       { return var.CastPkg(cache)->CurrentVer == nullptr; }),
				clause.solutions.end());
      }
      else if (not important)
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Ignore unimportant clause: " << clause.toString(cache, true) << std::endl;
	 return Clause{reason, Group::Satisfy, true};
      }
      else if (not existing.end() && policy.IsImportantDep(existing) && not satisfied)
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Ignoring unsatisfied clause: " << clause.toString(cache, true) << std::endl;
	 return Clause{reason, Group::Satisfy, true};
      }
      else if (IsUpgrade && not existing.end() && satisfied)
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Promoting previously satisfied clause to hard dependency: " << clause.toString(cache, true) << std::endl;
	 clause.optional = false;
      }
      else if (
	 IsUpgrade && not(AllowRemove && AllowInstall)				    // promote Recommends to Depends in upgrade, not in dist-upgrade.
	 && reason.Ver() && reason.Ver(cache) != reason.CastPkg(cache).CurrentVer() // and if this an upgrade to an installed package
	 && (existing.end() || not policy.IsImportantDep(existing))		    // new Recommends, or upgraded from Suggests
      )
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Promoting new clause to hard dependency: " << clause.toString(cache) << std::endl;
	 clause.optional = false;
      }
      else if (not existing.end() && importantToKeep(start) && satisfied)
      {
	 if (unlikely(debug >= 3))
	    std::cerr << "Restricting existing Recommends to installed packages: " << clause.toString(cache, true) << std::endl;
	 // Erase the non-installed solutions. We will process this last and try to keep the previously installed
	 // "best" solution installed.
	 clause.solutions.erase(std::remove_if(clause.solutions.begin(), clause.solutions.end(), [this](auto var)
					       { return var.CastPkg(cache)->CurrentVer == nullptr; }),
				clause.solutions.end());
      }
   }

   return clause;
}

void APT::Solver::Push(Var var, Work work)
{
   if (unlikely(debug >= 2))
      std::cerr << "Trying choice for " << work.toString(cache) << std::endl;

   choices.push_back(solved.size());
   solved.push_back(Solved{var, std::move(work)});
}

void APT::Solver::UndoOne()
{
   auto solvedItem = solved.back();

   if (unlikely(debug >= 4))
      std::cerr << "Undoing a single decision\n";

   if (not solvedItem.assigned.empty())
   {
      if (unlikely(debug >= 4))
	 std::cerr << "Unassign " << solvedItem.assigned.toString(cache) << "\n";
      auto &state = (*this)[solvedItem.assigned];
      state.decision = Decision::NONE;
      state.reason = nullptr;
      state.depth = 0;
   }

   if (auto work = solvedItem.work)
   {
      if (unlikely(debug >= 4))
	 std::cerr << "Adding work item " << work->toString(cache) << std::endl;

      if (not AddWork(std::move(*work)))
	 abort();
   }

   solved.pop_back();

   // FIXME: Add the undo handling here once we have watchers.
}

bool APT::Solver::Pop()
{
   if (depth() == 0)
      return false;

   time_t now = time(nullptr);
   if (startTime == 0)
      startTime = now;
   if (now - startTime >= Timeout)
      return _error->Error("Solver timed out.");

   if (unlikely(debug >= 2))
      for (std::string msg; _error->PopMessage(msg);)
	 std::cerr << "Branch failed: " << msg << std::endl;

   _error->Discard();

   // Assume() actually failed to enqueue anything, abort here
   if (choices.back() == solved.size())
   {
      choices.pop_back();
      return true;
   }

   assert(choices.back() < solved.size());
   int itemsToUndo = solved.size() - choices.back();
   auto choice = solved[choices.back()].assigned;

   for (; itemsToUndo; --itemsToUndo)
      UndoOne();

   // We need to remove any work that is at a higher depth.
   // FIXME: We should just mark the entries as erased and only do a compaction
   //        of the heap once we have a lot of erased entries in it.
   choices.pop_back();
   work.erase(std::remove_if(work.begin(), work.end(), [this](Work &w) -> bool
			     { return w.depth > depth() || w.erased; }),
	      work.end());
   std::make_heap(work.begin(), work.end());

   if (unlikely(debug >= 2))
      std::cerr << "Backtracking to choice " << choice.toString(cache) << "\n";

   // FIXME: There should be a reason!
   if (not choice.empty() && not Enqueue(choice, false, {}))
      return false;

   if (unlikely(debug >= 2))
      std::cerr << "Backtracked to choice " << choice.toString(cache) << "\n";

   return true;
}

bool APT::Solver::AddWork(Work &&w)
{
   if (w.clause->negative)
   {
      for (auto var : w.clause->solutions)
	 if (not Enqueue(var, false, w.clause))
	    return false;
   }
   else
   {
      if (unlikely(debug >= 3 && w.clause->optional))
	 std::cerr << "Enqueuing Recommends " << w.clause->toString(cache) << std::endl;
      if (w.clause->solutions.size() == 1 && not w.clause->optional)
	 return Enqueue(w.clause->solutions[0], true, w.clause);

      w.size = std::count_if(w.clause->solutions.begin(), w.clause->solutions.end(), [this](auto V)
			     { return (*this)[V].decision != Decision::MUSTNOT; });
      work.push_back(std::move(w));
      std::push_heap(work.begin(), work.end());
   }
   return true;
}

bool APT::Solver::Solve()
{
   _error->PushToStack();
   DEFER([&]() { _error->MergeWithStack(); });
   startTime = time(nullptr);
   while (true)
   {
      while (not Propagate())
      {
	 if (not Pop())
	    return false;
      }

      if (work.empty())
	 break;

      // *NOW* we can pop the item.
      std::pop_heap(work.begin(), work.end());

      // This item has been replaced with a new one. Remove it.
      if (work.back().erased)
      {
	 work.pop_back();
	 continue;
      }
      auto item = std::move(work.back());
      work.pop_back();
      solved.push_back(Solved{Var(), item});

      if (std::any_of(item.clause->solutions.begin(), item.clause->solutions.end(), [this](auto ver)
		      { return (*this)[ver].decision == Decision::MUST; }))
      {
	 if (unlikely(debug >= 2))
	    std::cerr << "ELIDED " << item.toString(cache) << std::endl;
	 continue;
      }

      if (unlikely(debug >= 1))
	 std::cerr << item.toString(cache) << std::endl;

      bool foundSolution = false;
      for (auto &sol : item.clause->solutions)
      {
	 if ((*this)[sol].decision == Decision::MUSTNOT)
	 {
	    if (unlikely(debug >= 3))
	       std::cerr << "(existing conflict: " << sol.toString(cache) << ")\n";
	    continue;
	 }
	 if (item.size > 1 || item.clause->optional)
	 {
	    Push(sol, item);
	 }
	 if (unlikely(debug >= 3))
	    std::cerr << "(try it: " << sol.toString(cache) << ")\n";
	 if (not Enqueue(sol, true, item.clause) && not Pop())
	    return false;
	 foundSolution = true;
	 break;
      }
      if (not foundSolution && not item.clause->optional)
      {
	 std::ostringstream err;

	 err << "Unable to satisfy dependencies. Reached two conflicting decisions:" << "\n";
	 std::unordered_set<Var> seen;
	 err << "1. " << LongWhyStr(item.clause->reason, true, (*this)[item.clause->reason].reason, "   ", seen).substr(3) << "\n";
	 err << "2. " << LongWhyStr(item.clause->reason, false, item.clause, "   ", seen).substr(3);
	 _error->Error("%s", err.str().c_str());
	 if (not Pop())
	    return false;
      }
   }

   return true;
}

// \brief Apply the selections from the dep cache to the solver
bool APT::Solver::FromDepCache(pkgDepCache &depcache)
{
   DefaultRootSetFunc2 rootSet(&cache);
   std::vector<Var> manualPackages;

   // Enforce strict pinning rules by rejecting all forbidden versions.
   if (StrictPinning)
   {
      for (auto P = cache.PkgBegin(); not P.end(); P++)
      {
	 bool isForced = depcache[P].Protect() && depcache[P].Install();
	 bool isPhasing = IsUpgrade && depcache.PhasingApplied(P) && not isForced;
	 for (auto V = P.VersionList(); not V.end(); ++V)
	    if (P.CurrentVer() != V && (depcache.GetCandidateVersion(P) != V || isPhasing))
	       if (not Enqueue(Var(V), false, {}))
		  return false;
      }
   }

   // Clause discovery depends on the manual flag, so we need to set the manual flag first before we discover any packages
   for (auto P = cache.PkgBegin(); not P.end(); P++)
      if (P->CurrentVer && not(depcache[P].Flags & pkgCache::Flag::Auto) && (depcache[P].Keep() || depcache[P].Install()))
	 (*this)[P].flags.manual = true;

   for (auto P = cache.PkgBegin(); not P.end(); P++)
   {
      if (P->VersionList == nullptr)
	 continue;

      auto state = depcache[P];
      if (P->SelectedState == pkgCache::State::Hold && not state.Protect())
      {
	 if (unlikely(debug >= 1))
	    std::cerr << "Hold " << P.FullName() << "\n";
	 if (P->CurrentVer ? not Enqueue(Var(P.CurrentVer()), true) : not Enqueue(Var(P), false))
	    return false;
      }
      else if (state.Delete()						  // Normal delete request.
	       || (not P->CurrentVer && state.Keep() && state.Protect())  // Delete request of not installed package.
	       || (not P->CurrentVer && state.Keep() && not AllowInstall) // New package installs not allowed.
      )
      {
	 if (unlikely(debug >= 1))
	    std::cerr << "Delete " << P.FullName() << "\n";
	 if (not Enqueue(Var(P), false))
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
	    if (depcache[P].Keep() ? not Enqueue(Var(P), true) : not Enqueue(Var(depcache.GetCandidateVersion(P)), true))
	       return false;
	 }
	 else
	 {
	    Clause w{Var(), Group, isOptional};
	    w.solutions.push_back(Var(P));
	    auto insertedW = RegisterClause(std::move(w));
	    if (insertedW && not AddWork(Work{insertedW, depth()}))
	       return false;

	    if (not isAuto)
	       manualPackages.push_back(Var(P));

	    // Given A->A2|A1, B->B1|B2; Bn->An, if we select `not A1`, we
	    // should try to install A2 before trying B so we end up with
	    // A2, B2, instead of removing A1 to keep B1 installed. This
	    // requires some special casing in Work::operator< above.
	    // Compare test-bug-712116-dpkg-pre-install-pkgs-hook-multiarch
	    Clause shortcircuit{Var(), Group, isOptional};
	    for (auto V = P.VersionList(); not V.end(); ++V)
	       shortcircuit.solutions.push_back(Var(V));
	    std::stable_sort(shortcircuit.solutions.begin(), shortcircuit.solutions.end(), CompareProviders3{cache, policy, P, *this});
	    auto insertedShort = RegisterClause(std::move(shortcircuit));
	    if (insertedShort && not AddWork(Work{insertedShort, depth()}))
	       return false;

	    // Discovery here is needed so the shortcircuit clause can actually become unit.
	    if (P.VersionList() && P.VersionList()->NextVer)
	       Discover(Var(P));
	 }
      }
      else if (IsUpgrade && AllowRemove && AllowInstall && (P->Flags & pkgCache::Flag::Essential))
      {
	 Clause w{Var(), Group::InstallManual, false};
	 auto G = P.Group();
	 for (auto P = G.PackageList(); not P.end(); P = G.NextPkg(P))
	    if (P->Flags & pkgCache::Flag::Essential)
	       w.solutions.push_back(Var(P));
	 std::stable_sort(w.solutions.begin(), w.solutions.end(), CompareProviders3{cache, policy, P, *this});
	 if (unlikely(debug >= 1))
	    std::cerr << "Install essential package " << P << std::endl;
	 auto inserted = RegisterClause(std::move(w));
	 if (inserted && not AddWork(Work{inserted, depth()}))
	    return false;
      }
   }

   if (not Propagate())
      return false;

   std::stable_sort(manualPackages.begin(), manualPackages.end(), CompareProviders3{cache, policy, {}, *this});
   for (auto assumption : manualPackages)
   {
      if (not Assume(assumption, true, {}) || not Propagate())
	 if (not Pop())
	    abort();
   }

   return true;
}

bool APT::Solver::ToDepCache(pkgDepCache &depcache) const
{
   FastContiguousCacheMap<pkgCache::Package, bool> movedManual(cache);
   pkgDepCache::ActionGroup group(depcache);
   for (auto P = cache.PkgBegin(); not P.end(); P++)
   {
      depcache[P].Marked = 0;
      depcache[P].Garbage = 0;
      if ((*this)[P].decision == Decision::MUST)
      {
	 pkgCache::VerIterator cand;
	 for (auto V = P.VersionList(); cand.end() && not V.end(); V++)
	    if ((*this)[V].decision == Decision::MUST)
	       cand = V;

	 auto reasonClause = (*this)[cand].reason;
	 auto reason = reasonClause ? reasonClause->reason : Var();
	 if (auto RP = reason.Pkg(); RP == P.MapPointer())
	    reason = (*this)[P].reason ? (*this)[P].reason->reason : Var();

	 if (cand != P.CurrentVer())
	 {
	    bool automatic = (not reason.empty() || (depcache[P].Flags & pkgCache::Flag::Auto)) && not movedManual[P];
	    depcache.SetCandidateVersion(cand);
	    depcache.MarkInstall(P, false, 0, not automatic);

	    // Set the automatic bit for new packages or move it on upgrades to oldlibs
	    if (not P->CurrentVer)
	       depcache.MarkAuto(P, automatic);
	    else if (not(depcache[P].Flags & pkgCache::Flag::Auto) && P.CurrentVer()->Section && cand->Section && not _config->SectionInSubTree("APT::Move-Autobit-Sections", P.CurrentVer().Section()) && _config->SectionInSubTree("APT::Move-Autobit-Sections", cand.Section()))
	    {
	       bool moved = false;
	       for (auto const &clause : (*this)[cand].clauses)
		  for (auto sol : clause->solutions)
		  {
		     // New installs move the auto-bit. TODO: Should we look at whether clause is the reason for installing it?
		     if (sol.CastPkg(cache) == P || sol.CastPkg(cache)->CurrentVer)
			continue;
		     std::cerr << "Move  manual bit from " << P.FullName() << " to " << sol.CastPkg(cache).Name() << std::endl;
		     movedManual[sol.CastPkg(cache)] = true;
		     depcache.MarkAuto(sol.CastPkg(cache), false);
		     moved = true;
		  }
	       if (moved)
		  depcache.MarkAuto(P, true);
	    }

	 }
	 else
	    depcache.MarkKeep(P, false, reason.empty() && not(depcache[P].Flags & pkgCache::Flag::Auto));

	 depcache[P].Marked = 1;
	 depcache[P].Garbage = 0;
      }
      else if (P->CurrentVer || depcache[P].Install())
      {
	 depcache.MarkDelete(P, false, 0, not(*this)[P].reason);
	 depcache[P].Marked = 0;
	 depcache[P].Garbage = 1;
      }
   }
   return true;
}

// Command-line
std::string APT::Solver::InternalCliWhy(pkgDepCache &cache, pkgCache::PkgIterator pkg, bool decision)
{
   APT::Solver solver(cache.GetCache(), cache.GetPolicy(), EDSP::Request::Flags(0));
   // In case nothing has a positive dependency on pkg it may not actually be discovered in a `why-not`
   // scenario, so make sure we discover it explicitly.
   solver.Discover(Var(pkg));
   if (not solver.FromDepCache(cache) || not solver.Solve())
      return "";
   std::unordered_set<Var> seen;
   std::string buf;
   if (solver[Var(pkg)].decision == Decision::NONE)
      return strprintf(buf, "%s is undecided\n", pkg.FullName().c_str()), buf;
   if (decision && solver[Var(pkg)].decision != Decision::MUST)
      return strprintf(buf, "%s is not actually marked for install\n", pkg.FullName().c_str()), buf;
   if (not decision && solver[Var(pkg)].decision == Decision::MUST)
      return strprintf(buf, "%s is actually marked for install\n", pkg.FullName().c_str()), buf;
   return solver.LongWhyStr(Var(pkg), decision, solver[Var(pkg)].reason, "", seen);
}

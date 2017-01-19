// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.cc,v 1.25 2001/05/27 05:36:04 jgg Exp $
/* ######################################################################

   Dependency Cache - Caches Dependency information.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include<config.h>

#include <apt-pkg/depcache.h>
#include <apt-pkg/versionmatch.h>
#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/macros.h>

#include <stdio.h>
#include <string.h>
#include <list>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <iostream>
#include <set>

#include <sys/stat.h>

#include <apti18n.h>
									/*}}}*/

using std::string;

// helper for Install-Recommends-Sections and Never-MarkAuto-Sections	/*{{{*/
static bool 
ConfigValueInSubTree(const char* SubTree, const char *needle)
{
   Configuration::Item const *Opts;
   Opts = _config->Tree(SubTree);
   if (Opts != 0 && Opts->Child != 0)
   {
      Opts = Opts->Child;
      for (; Opts != 0; Opts = Opts->Next)
      {
	 if (Opts->Value.empty() == true)
	    continue;
	 if (strcmp(needle, Opts->Value.c_str()) == 0)
	    return true;
      }
   }
   return false;
}
									/*}}}*/
pkgDepCache::ActionGroup::ActionGroup(pkgDepCache &cache) :		/*{{{*/
  d(NULL), cache(cache), released(false)
{
  ++cache.group_level;
}

void pkgDepCache::ActionGroup::release()
{
  if(!released)
    {
      if(cache.group_level == 0)
	std::cerr << "W: Unbalanced action groups, expect badness" << std::endl;
      else
	{
	  --cache.group_level;

	  if(cache.group_level == 0)
	    cache.MarkAndSweep();
	}

      released = true;
    }
}

pkgDepCache::ActionGroup::~ActionGroup()
{
  release();
}
									/*}}}*/
// DepCache::pkgDepCache - Constructors					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::pkgDepCache(pkgCache * const pCache,Policy * const Plcy) :
  group_level(0), Cache(pCache), PkgState(0), DepState(0),
   iUsrSize(0), iDownloadSize(0), iInstCount(0), iDelCount(0), iKeepCount(0),
   iBrokenCount(0), iPolicyBrokenCount(0), iBadCount(0), d(NULL)
{
   DebugMarker = _config->FindB("Debug::pkgDepCache::Marker", false);
   DebugAutoInstall = _config->FindB("Debug::pkgDepCache::AutoInstall", false);
   delLocalPolicy = 0;
   LocalPolicy = Plcy;
   if (LocalPolicy == 0)
      delLocalPolicy = LocalPolicy = new Policy;
}
									/*}}}*/
// DepCache::~pkgDepCache - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::~pkgDepCache()
{
   delete [] PkgState;
   delete [] DepState;
   delete delLocalPolicy;
}
									/*}}}*/
// DepCache::Init - Generate the initial extra structures.		/*{{{*/
// ---------------------------------------------------------------------
/* This allocats the extension buffers and initializes them. */
bool pkgDepCache::Init(OpProgress * const Prog)
{
   // Suppress mark updates during this operation (just in case) and
   // run a mark operation when Init terminates.
   ActionGroup actions(*this);

   delete [] PkgState;
   delete [] DepState;
   PkgState = new StateCache[Head().PackageCount];
   DepState = new unsigned char[Head().DependsCount];
   memset(PkgState,0,sizeof(*PkgState)*Head().PackageCount);
   memset(DepState,0,sizeof(*DepState)*Head().DependsCount);

   if (Prog != 0)
   {
      Prog->OverallProgress(0,2*Head().PackageCount,Head().PackageCount,
			    _("Building dependency tree"));
      Prog->SubProgress(Head().PackageCount,_("Candidate versions"));
   }

   /* Set the current state of everything. In this state all of the
      packages are kept exactly as is. See AllUpgrade */
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; ++I, ++Done)
   {
      if (Prog != 0 && Done%20 == 0)
	 Prog->Progress(Done);

      // Find the proper cache slot
      StateCache &State = PkgState[I->ID];
      State.iFlags = 0;

      // Figure out the install version
      State.CandidateVer = LocalPolicy->GetCandidateVer(I);
      State.InstallVer = I.CurrentVer();
      State.Mode = ModeKeep;

      State.Update(I,*this);
   }

   if (Prog != 0)
   {
      Prog->OverallProgress(Head().PackageCount,2*Head().PackageCount,
			    Head().PackageCount,
			    _("Building dependency tree"));
      Prog->SubProgress(Head().PackageCount,_("Dependency generation"));
   }

   Update(Prog);

   if(Prog != 0)
      Prog->Done();

   return true;
}
									/*}}}*/
bool pkgDepCache::readStateFile(OpProgress * const Prog)		/*{{{*/
{
   FileFd state_file;
   string const state = _config->FindFile("Dir::State::extended_states");
   if(RealFileExists(state)) {
      state_file.Open(state, FileFd::ReadOnly);
      off_t const file_size = state_file.Size();
      if(Prog != NULL)
	 Prog->OverallProgress(0, file_size, 1,
			       _("Reading state information"));

      pkgTagFile tagfile(&state_file);
      pkgTagSection section;
      off_t amt = 0;
      bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);
      while(tagfile.Step(section)) {
	 string const pkgname = section.FindS("Package");
	 string pkgarch = section.FindS("Architecture");
	 if (pkgarch.empty() == true)
	    pkgarch = "any";
	 pkgCache::PkgIterator pkg = Cache->FindPkg(pkgname, pkgarch);
	 // Silently ignore unknown packages and packages with no actual version.
	 if(pkg.end() == true || pkg->VersionList == 0)
	    continue;

	 short const reason = section.FindI("Auto-Installed", 0);
	 if(reason > 0)
	 {
	    PkgState[pkg->ID].Flags |= Flag::Auto;
	    if (unlikely(debug_autoremove))
	       std::clog << "Auto-Installed : " << pkg.FullName() << std::endl;
	    if (pkgarch == "any")
	    {
	       pkgCache::GrpIterator G = pkg.Group();
	       for (pkg = G.NextPkg(pkg); pkg.end() != true; pkg = G.NextPkg(pkg))
		  if (pkg->VersionList != 0)
		     PkgState[pkg->ID].Flags |= Flag::Auto;
	    }
	 }
	 amt += section.size();
	 if(Prog != NULL)
	    Prog->OverallProgress(amt, file_size, 1, 
				  _("Reading state information"));
      }
      if(Prog != NULL)
	 Prog->OverallProgress(file_size, file_size, 1,
			       _("Reading state information"));
   }

   return true;
}
									/*}}}*/
bool pkgDepCache::writeStateFile(OpProgress * const /*prog*/, bool const InstalledOnly)	/*{{{*/
{
   bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);
   
   if(debug_autoremove)
      std::clog << "pkgDepCache::writeStateFile()" << std::endl;

   FileFd StateFile;
   string const state = _config->FindFile("Dir::State::extended_states");
   if (CreateAPTDirectoryIfNeeded(_config->FindDir("Dir::State"), flNotFile(state)) == false)
      return false;

   // if it does not exist, create a empty one
   if(!RealFileExists(state))
   {
      StateFile.Open(state, FileFd::WriteAtomic);
      StateFile.Close();
   }

   // open it
   if(!StateFile.Open(state, FileFd::ReadOnly))
      return _error->Error(_("Failed to open StateFile %s"),
			   state.c_str());

   FileFd OutFile(state, FileFd::ReadWrite | FileFd::Atomic);
   if (OutFile.IsOpen() == false || OutFile.Failed() == true)
      return _error->Error(_("Failed to write temporary StateFile %s"), state.c_str());

   // first merge with the existing sections
   pkgTagFile tagfile(&StateFile);
   pkgTagSection section;
   std::set<string> pkgs_seen;
   while(tagfile.Step(section)) {
	 string const pkgname = section.FindS("Package");
	 string pkgarch = section.FindS("Architecture");
	 if (pkgarch.empty() == true)
	    pkgarch = "native";
	 // Silently ignore unknown packages and packages with no actual
	 // version.
	 pkgCache::PkgIterator pkg = Cache->FindPkg(pkgname, pkgarch);
	 if(pkg.end() || pkg.VersionList().end())
	    continue;
	 StateCache const &P = PkgState[pkg->ID];
	 bool newAuto = (P.Flags & Flag::Auto);
	 // reset to default (=manual) not installed or now-removed ones if requested
	 if (InstalledOnly && (
	     (pkg->CurrentVer == 0 && P.Mode != ModeInstall) ||
	     (pkg->CurrentVer != 0 && P.Mode == ModeDelete)))
	    newAuto = false;
	 if (newAuto == false)
	 {
	    // The section is obsolete if it contains no other tag
	    auto const count = section.Count();
	    if (count < 2 ||
		(count == 2 && section.Exists("Auto-Installed")) ||
		(count == 3 && section.Exists("Auto-Installed") && section.Exists("Architecture")))
	    {
	       if(debug_autoremove)
		  std::clog << "Drop obsolete section with " << count << " fields for " << APT::PrettyPkg(this, pkg) << std::endl;
	       continue;
	    }
	 }

	 if(debug_autoremove)
	    std::clog << "Update existing AutoInstall to " << newAuto << " for " << APT::PrettyPkg(this, pkg) << std::endl;

	 std::vector<pkgTagSection::Tag> rewrite;
	 rewrite.push_back(pkgTagSection::Tag::Rewrite("Architecture", pkg.Arch()));
	 rewrite.push_back(pkgTagSection::Tag::Rewrite("Auto-Installed", newAuto ? "1" : "0"));
	 section.Write(OutFile, NULL, rewrite);
	 if (OutFile.Write("\n", 1) == false)
	    return false;
	 pkgs_seen.insert(pkg.FullName());
   }

   // then write the ones we have not seen yet
   for(pkgCache::PkgIterator pkg=Cache->PkgBegin(); !pkg.end(); ++pkg) {
      StateCache const &P = PkgState[pkg->ID];
      if(P.Flags & Flag::Auto) {
	 if (pkgs_seen.find(pkg.FullName()) != pkgs_seen.end()) {
	    if(debug_autoremove)
	       std::clog << "Skipping already written " << APT::PrettyPkg(this, pkg) << std::endl;
	    continue;
	 }
	 // skip not installed ones if requested
	 if (InstalledOnly && (
	     (pkg->CurrentVer == 0 && P.Mode != ModeInstall) ||
	     (pkg->CurrentVer != 0 && P.Mode == ModeDelete)))
	    continue;
	 if(debug_autoremove)
	    std::clog << "Writing new AutoInstall: " << APT::PrettyPkg(this, pkg) << std::endl;
	 std::string stanza = "Package: ";
	 stanza.append(pkg.Name())
	      .append("\nArchitecture: ").append(pkg.Arch())
	      .append("\nAuto-Installed: 1\n\n");
	 if (OutFile.Write(stanza.c_str(), stanza.length()) == false)
	    return false;
      }
   }
   if (StateFile.Failed())
   {
      OutFile.OpFail();
      return false;
   }
   if (OutFile.Close() == false)
      return false;
   chmod(state.c_str(), 0644);
   return true;
}
									/*}}}*/
// DepCache::CheckDep - Checks a single dependency			/*{{{*/
// ---------------------------------------------------------------------
/* This first checks the dependency against the main target package and
   then walks along the package provides list and checks if each provides 
   will be installed then checks the provides against the dep. Res will be 
   set to the package which was used to satisfy the dep. */
bool pkgDepCache::CheckDep(DepIterator const &Dep,int const Type,PkgIterator &Res)
{
   Res = Dep.TargetPkg();

   /* Check simple depends. A depends -should- never self match but 
      we allow it anyhow because dpkg does. Technically it is a packaging
      bug. Conflicts may never self match */
   if (Dep.IsIgnorable(Res) == false)
   {
      // Check the base package
      if (Type == NowVersion)
      {
	 if (Res->CurrentVer != 0 && Dep.IsSatisfied(Res.CurrentVer()) == true)
	    return true;
      }
      else if (Type == InstallVersion)
      {
	 if (PkgState[Res->ID].InstallVer != 0 &&
	       Dep.IsSatisfied(PkgState[Res->ID].InstVerIter(*this)) == true)
	    return true;
      }
      else if (Type == CandidateVersion)
	 if (PkgState[Res->ID].CandidateVer != 0 &&
	       Dep.IsSatisfied(PkgState[Res->ID].CandidateVerIter(*this)) == true)
	    return true;
   }

   if (Dep->Type == Dep::Obsoletes)
      return false;

   // Check the providing packages
   PrvIterator P = Dep.TargetPkg().ProvidesList();
   for (; P.end() != true; ++P)
   {
      if (Dep.IsIgnorable(P) == true)
	 continue;

      // Check if the provides is a hit
      if (Type == NowVersion)
      {
	 if (P.OwnerPkg().CurrentVer() != P.OwnerVer())
	    continue;
      }
      else if (Type == InstallVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.InstallVer != (Version *)P.OwnerVer())
	    continue;
      }
      else if (Type == CandidateVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.CandidateVer != (Version *)P.OwnerVer())
	    continue;
      }

      // Compare the versions.
      if (Dep.IsSatisfied(P) == true)
      {
	 Res = P.OwnerPkg();
	 return true;
      }
   }

   return false;
}
									/*}}}*/
// DepCache::AddSizes - Add the packages sizes to the counters		/*{{{*/
// ---------------------------------------------------------------------
/* Call with Inverse = true to preform the inverse opration */
void pkgDepCache::AddSizes(const PkgIterator &Pkg, bool const Inverse)
{
   StateCache &P = PkgState[Pkg->ID];
   
   if (Pkg->VersionList == 0)
      return;
   
   if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure && 
       P.Keep() == true)
      return;
   
   // Compute the size data
   if (P.NewInstall() == true)
   {
      if (Inverse == false) {
	 iUsrSize += P.InstVerIter(*this)->InstalledSize;
	 iDownloadSize += P.InstVerIter(*this)->Size;
      } else {
	 iUsrSize -= P.InstVerIter(*this)->InstalledSize;
	 iDownloadSize -= P.InstVerIter(*this)->Size;
      }
      return;
   }
   
   // Upgrading
   if (Pkg->CurrentVer != 0 && 
       (P.InstallVer != (Version *)Pkg.CurrentVer() || 
	(P.iFlags & ReInstall) == ReInstall) && P.InstallVer != 0)
   {
      if (Inverse == false) {
	 iUsrSize -= Pkg.CurrentVer()->InstalledSize;
	 iUsrSize += P.InstVerIter(*this)->InstalledSize;
	 iDownloadSize += P.InstVerIter(*this)->Size;
      } else {
	 iUsrSize -= P.InstVerIter(*this)->InstalledSize;
	 iUsrSize += Pkg.CurrentVer()->InstalledSize;
	 iDownloadSize -= P.InstVerIter(*this)->Size;
      }
      return;
   }
   
   // Reinstall
   if (Pkg.State() == pkgCache::PkgIterator::NeedsUnpack &&
       P.Delete() == false)
   {
      if (Inverse == false)
	 iDownloadSize += P.InstVerIter(*this)->Size;
      else
	 iDownloadSize -= P.InstVerIter(*this)->Size;
      return;
   }
   
   // Removing
   if (Pkg->CurrentVer != 0 && P.InstallVer == 0)
   {
      if (Inverse == false)
	 iUsrSize -= Pkg.CurrentVer()->InstalledSize;
      else
	 iUsrSize += Pkg.CurrentVer()->InstalledSize;
      return;
   }   
}
									/*}}}*/
// DepCache::AddStates - Add the package to the state counter		/*{{{*/
// ---------------------------------------------------------------------
/* This routine is tricky to use, you must make sure that it is never 
   called twice for the same package. This means the Remove/Add section
   should be as short as possible and not encompass any code that will 
   calld Remove/Add itself. Remember, dependencies can be circular so
   while processing a dep for Pkg it is possible that Add/Remove
   will be called on Pkg */
void pkgDepCache::AddStates(const PkgIterator &Pkg, bool const Invert)
{
   signed char const Add = (Invert == false) ? 1 : -1;
   StateCache &State = PkgState[Pkg->ID];

   // The Package is broken (either minimal dep or policy dep)
   if ((State.DepState & DepInstMin) != DepInstMin)
      iBrokenCount += Add;
   if ((State.DepState & DepInstPolicy) != DepInstPolicy)
      iPolicyBrokenCount += Add;

   // Bad state
   if (Pkg.State() != PkgIterator::NeedsNothing)
      iBadCount += Add;

   // Not installed
   if (Pkg->CurrentVer == 0)
   {
      if (State.Mode == ModeDelete &&
	  (State.iFlags & Purge) == Purge && Pkg.Purge() == false)
	 iDelCount += Add;

      if (State.Mode == ModeInstall)
	 iInstCount += Add;
      return;
   }

   // Installed, no upgrade
   if (State.Status == 0)
   {
      if (State.Mode == ModeDelete)
	 iDelCount += Add;
      else
	 if ((State.iFlags & ReInstall) == ReInstall)
	    iInstCount += Add;
      return;
   }

   // Alll 3 are possible
   if (State.Mode == ModeDelete)
      iDelCount += Add;
   else if (State.Mode == ModeKeep)
      iKeepCount += Add;
   else if (State.Mode == ModeInstall)
      iInstCount += Add;
}
									/*}}}*/
// DepCache::BuildGroupOrs - Generate the Or group dep data		/*{{{*/
// ---------------------------------------------------------------------
/* The or group results are stored in the last item of the or group. This
   allows easy detection of the state of a whole or'd group. */
void pkgDepCache::BuildGroupOrs(VerIterator const &V)
{
   unsigned char Group = 0;
   for (DepIterator D = V.DependsList(); D.end() != true; ++D)
   {
      // Build the dependency state.
      unsigned char &State = DepState[D->ID];

      /* Invert for Conflicts. We have to do this twice to get the
         right sense for a conflicts group */
      if (D.IsNegative() == true)
	 State = ~State;

      // Add to the group if we are within an or..
      State &= 0x7;
      Group |= State;
      State |= Group << 3;
      if ((D->CompareOp & Dep::Or) != Dep::Or)
	 Group = 0;

      // Invert for Conflicts
      if (D.IsNegative() == true)
	 State = ~State;
   }
}
									/*}}}*/
// DepCache::VersionState - Perform a pass over a dependency list	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to run over a dependency list and determine the dep
   state of the list, filtering it through both a Min check and a Policy
   check. The return result will have SetMin/SetPolicy low if a check
   fails. It uses the DepState cache for it's computations. */
unsigned char pkgDepCache::VersionState(DepIterator D, unsigned char const Check,
				       unsigned char const SetMin,
				       unsigned char const SetPolicy) const
{
   unsigned char Dep = 0xFF;
   while (D.end() != true)
   {
      // the last or-dependency has the state of all previous or'ed
      DepIterator Start, End;
      D.GlobOr(Start, End);
      // ignore if we are called with Dep{Install,…} or DepG{Install,…}
      // the later would be more correct, but the first is what we get
      unsigned char const State = DepState[End->ID] | (DepState[End->ID] >> 3);

      // Minimum deps that must be satisfied to have a working package
      if (Start.IsCritical() == true)
      {
	 if ((State & Check) != Check)
	    return Dep &= ~(SetMin | SetPolicy);
      }
      // Policy deps that must be satisfied to install the package
      else if (IsImportantDep(Start) == true &&
	  (State & Check) != Check)
	 Dep &= ~SetPolicy;
   }
   return Dep;
}
									/*}}}*/
// DepCache::DependencyState - Compute the 3 results for a dep		/*{{{*/
// ---------------------------------------------------------------------
/* This is the main dependency computation bit. It computes the 3 main
   results for a dependencys, Now, Install and Candidate. Callers must
   invert the result if dealing with conflicts. */
unsigned char pkgDepCache::DependencyState(DepIterator const &D)
{
   unsigned char State = 0;

   if (CheckDep(D,NowVersion) == true)
      State |= DepNow;
   if (CheckDep(D,InstallVersion) == true)
      State |= DepInstall;
   if (CheckDep(D,CandidateVersion) == true)
      State |= DepCVer;

   return State;
}
									/*}}}*/
// DepCache::UpdateVerState - Compute the Dep member of the state	/*{{{*/
// ---------------------------------------------------------------------
/* This determines the combined dependency representation of a package
   for its two states now and install. This is done by using the pre-generated
   dependency information. */
void pkgDepCache::UpdateVerState(PkgIterator const &Pkg)
{   
   // Empty deps are always true
   StateCache &State = PkgState[Pkg->ID];
   State.DepState = 0xFF;
   
   // Check the Current state
   if (Pkg->CurrentVer != 0)
   {
      DepIterator D = Pkg.CurrentVer().DependsList();
      State.DepState &= VersionState(D,DepNow,DepNowMin,DepNowPolicy);
   }
   
   /* Check the candidate state. We do not compare against the whole as
      a candidate state but check the candidate version against the 
      install states */
   if (State.CandidateVer != 0)
   {
      DepIterator D = State.CandidateVerIter(*this).DependsList();
      State.DepState &= VersionState(D,DepInstall,DepCandMin,DepCandPolicy);
   }
   
   // Check target state which can only be current or installed
   if (State.InstallVer != 0)
   {
      DepIterator D = State.InstVerIter(*this).DependsList();
      State.DepState &= VersionState(D,DepInstall,DepInstMin,DepInstPolicy);
   }
}
									/*}}}*/
// DepCache::Update - Figure out all the state information		/*{{{*/
// ---------------------------------------------------------------------
/* This will figure out the state of all the packages and all the 
   dependencies based on the current policy. */
void pkgDepCache::Update(OpProgress * const Prog)
{   
   iUsrSize = 0;
   iDownloadSize = 0;
   iInstCount = 0;
   iDelCount = 0;
   iKeepCount = 0;
   iBrokenCount = 0;
   iPolicyBrokenCount = 0;
   iBadCount = 0;

   // Perform the depends pass
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; ++I, ++Done)
   {
      if (Prog != 0 && Done%20 == 0)
	 Prog->Progress(Done);
      for (VerIterator V = I.VersionList(); V.end() != true; ++V)
      {
	 unsigned char Group = 0;

	 for (DepIterator D = V.DependsList(); D.end() != true; ++D)
	 {
	    // Build the dependency state.
	    unsigned char &State = DepState[D->ID];
	    State = DependencyState(D);

	    // Add to the group if we are within an or..
	    Group |= State;
	    State |= Group << 3;
	    if ((D->CompareOp & Dep::Or) != Dep::Or)
	       Group = 0;

	    // Invert for Conflicts
	    if (D.IsNegative() == true)
	       State = ~State;
	 }
      }

      // Compute the package dependency state and size additions
      AddSizes(I);
      UpdateVerState(I);
      AddStates(I);
   }

   if (Prog != 0)
      Prog->Progress(Done);

   readStateFile(Prog);
}
									/*}}}*/
// DepCache::Update - Update the deps list of a package	   		/*{{{*/
// ---------------------------------------------------------------------
/* This is a helper for update that only does the dep portion of the scan. 
   It is mainly meant to scan reverse dependencies. */
void pkgDepCache::Update(DepIterator D)
{
   // Update the reverse deps
   for (;D.end() != true; ++D)
   {      
      unsigned char &State = DepState[D->ID];
      State = DependencyState(D);
    
      // Invert for Conflicts
      if (D.IsNegative() == true)
	 State = ~State;

      RemoveStates(D.ParentPkg());
      BuildGroupOrs(D.ParentVer());
      UpdateVerState(D.ParentPkg());
      AddStates(D.ParentPkg());
   }
}
									/*}}}*/
// DepCache::Update - Update the related deps of a package		/*{{{*/
// ---------------------------------------------------------------------
/* This is called whenever the state of a package changes. It updates
   all cached dependencies related to this package. */
void pkgDepCache::Update(PkgIterator const &Pkg)
{   
   // Recompute the dep of the package
   RemoveStates(Pkg);
   UpdateVerState(Pkg);
   AddStates(Pkg);
   
   // Update the reverse deps
   Update(Pkg.RevDependsList());

   // Update the provides map for the current ver
   if (Pkg->CurrentVer != 0)
      for (PrvIterator P = Pkg.CurrentVer().ProvidesList(); 
	   P.end() != true; ++P)
	 Update(P.ParentPkg().RevDependsList());

   // Update the provides map for the candidate ver
   if (PkgState[Pkg->ID].CandidateVer != 0)
      for (PrvIterator P = PkgState[Pkg->ID].CandidateVerIter(*this).ProvidesList();
	   P.end() != true; ++P)
	 Update(P.ParentPkg().RevDependsList());
}
									/*}}}*/
// DepCache::MarkKeep - Put the package in the keep state		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDepCache::MarkKeep(PkgIterator const &Pkg, bool Soft, bool FromUser,
                           unsigned long Depth)
{
   if (IsModeChangeOk(ModeKeep, Pkg, Depth, FromUser) == false)
      return false;

   /* Reject an attempt to keep a non-source broken installed package, those
      must be upgraded */
   if (Pkg.State() == PkgIterator::NeedsUnpack && 
       Pkg.CurrentVer().Downloadable() == false)
      return false;

   /* We changed the soft state all the time so the UI is a bit nicer
      to use */
   StateCache &P = PkgState[Pkg->ID];

   // Check that it is not already kept
   if (P.Mode == ModeKeep)
      return true;

   if (Soft == true)
      P.iFlags |= AutoKept;
   else
      P.iFlags &= ~AutoKept;

   ActionGroup group(*this);

#if 0 // resetting the autoflag here means we lose the 
      // auto-mark information if a user selects a package for removal
      // but changes  his mind then and sets it for keep again
      // - this makes sense as default when all Garbage dependencies
      //   are automatically marked for removal (as aptitude does).
      //   setting a package for keep then makes it no longer autoinstalled
      //   for all other use-case this action is rather surprising
   if(FromUser && !P.Marked)
     P.Flags &= ~Flag::Auto;
#endif

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << "MarkKeep " << APT::PrettyPkg(this, Pkg) << " FU=" << FromUser << std::endl;

   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   P.Mode = ModeKeep;
   if (Pkg->CurrentVer == 0)
      P.InstallVer = 0;
   else
      P.InstallVer = Pkg.CurrentVer();

   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);

   return true;
}
									/*}}}*/
// DepCache::MarkDelete - Put the package in the delete state		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDepCache::MarkDelete(PkgIterator const &Pkg, bool rPurge,
                             unsigned long Depth, bool FromUser)
{
   if (IsModeChangeOk(ModeDelete, Pkg, Depth, FromUser) == false)
      return false;

   StateCache &P = PkgState[Pkg->ID];

   // Check that it is not already marked for delete
   if ((P.Mode == ModeDelete || P.InstallVer == 0) && 
       (Pkg.Purge() == true || rPurge == false))
      return true;

   // check if we are allowed to remove the package
   if (IsDeleteOk(Pkg,rPurge,Depth,FromUser) == false)
      return false;

   P.iFlags &= ~(AutoKept | Purge);
   if (rPurge == true)
      P.iFlags |= Purge;

   ActionGroup group(*this);

   if (FromUser == false)
   {
      VerIterator const PV = P.InstVerIter(*this);
      if (PV.end() == false)
      {
	 // removed metapackages mark their dependencies as manual to prevent in "desktop depends browser, texteditor"
	 // the removal of browser to suggest the removal of desktop and texteditor.
	 // We ignore the auto-bit here as we can't deal with metapackage cascardes otherwise.
	 // We do not check for or-groups here as we don't know which package takes care of
	 // providing the feature the user likes e.g.:  browser1 | browser2 | browser3
	 // Temporary removals are effected by this as well, which is bad, but unlikely in practice
	 bool const PinNeverMarkAutoSection = (PV->Section != 0 && ConfigValueInSubTree("APT::Never-MarkAuto-Sections", PV.Section()));
	 if (PinNeverMarkAutoSection)
	 {
	    for (DepIterator D = PV.DependsList(); D.end() != true; ++D)
	    {
	       if (D.IsMultiArchImplicit() == true || D.IsNegative() == true || IsImportantDep(D) == false)
		  continue;

	       pkgCacheFile CacheFile(this);
	       APT::VersionList verlist = APT::VersionList::FromDependency(CacheFile, D, APT::CacheSetHelper::INSTALLED);
	       for (auto const &V : verlist)
	       {
		  PkgIterator const DP = V.ParentPkg();
		  if(DebugAutoInstall == true)
		     std::clog << OutputInDepth(Depth) << "Setting " << DP.FullName(false) << " NOT as auto-installed (direct "
			<< D.DepType() << " of " << Pkg.FullName(false) << " which is in APT::Never-MarkAuto-Sections)" << std::endl;

		  MarkAuto(DP, false);
	       }
	    }
	 }
      }
   }

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << (rPurge ? "MarkPurge " : "MarkDelete ") << APT::PrettyPkg(this, Pkg) << " FU=" << FromUser << std::endl;

   RemoveSizes(Pkg);
   RemoveStates(Pkg);
   
   if (Pkg->CurrentVer == 0 && (Pkg.Purge() == true || rPurge == false))
      P.Mode = ModeKeep;
   else
      P.Mode = ModeDelete;
   P.InstallVer = 0;

   AddStates(Pkg);   
   Update(Pkg);
   AddSizes(Pkg);

   return true;
}
									/*}}}*/
// DepCache::IsDeleteOk - check if it is ok to remove this package	/*{{{*/
// ---------------------------------------------------------------------
/* The default implementation tries to prevent deletion of install requests.
   dpkg holds are enforced by the private IsModeChangeOk */
bool pkgDepCache::IsDeleteOk(PkgIterator const &Pkg,bool rPurge,
			      unsigned long Depth, bool FromUser)
{
   return IsDeleteOkProtectInstallRequests(Pkg, rPurge, Depth, FromUser);
}
bool pkgDepCache::IsDeleteOkProtectInstallRequests(PkgIterator const &Pkg,
      bool const /*rPurge*/, unsigned long const Depth, bool const FromUser)
{
   if (FromUser == false && Pkg->CurrentVer == 0)
   {
      StateCache &P = PkgState[Pkg->ID];
      if (P.InstallVer != 0 && P.Status == 2 && (P.Flags & Flag::Auto) != Flag::Auto)
      {
	 if (DebugMarker == true)
	    std::clog << OutputInDepth(Depth) << "Manual install request prevents MarkDelete of " << APT::PrettyPkg(this, Pkg) << std::endl;
	 return false;
      }
   }
   return true;
}
									/*}}}*/
// DepCache::IsModeChangeOk - check if it is ok to change the mode	/*{{{*/
// ---------------------------------------------------------------------
/* this is used by all Mark methods on the very first line to check sanity
   and prevents mode changes for packages on hold for example.
   If you want to check Mode specific stuff you can use the virtual public
   Is<Mode>Ok methods instead */
static char const* PrintMode(char const mode)
{
	 switch (mode)
	 {
	 case pkgDepCache::ModeInstall: return "Install";
	 case pkgDepCache::ModeKeep: return "Keep";
	 case pkgDepCache::ModeDelete: return "Delete";
	 case pkgDepCache::ModeGarbage: return "Garbage";
	 default: return "UNKNOWN";
	 }
}
bool pkgDepCache::IsModeChangeOk(ModeList const mode, PkgIterator const &Pkg,
				 unsigned long const Depth, bool const FromUser)
{
   // we are not trying to hard…
   if (unlikely(Depth > 100))
      return false;

   // general sanity
   if (unlikely(Pkg.end() == true || Pkg->VersionList == 0))
      return false;

   // the user is always right
   if (FromUser == true)
      return true;

   StateCache &P = PkgState[Pkg->ID];
   // not changing the mode is obviously also fine as we might want to call
   // e.g. MarkInstall multiple times with different arguments for the same package
   if (P.Mode == mode)
      return true;

   // if previous state was set by user only user can reset it
   if ((P.iFlags & Protected) == Protected)
   {
      if (unlikely(DebugMarker == true))
	 std::clog << OutputInDepth(Depth) << "Ignore Mark" << PrintMode(mode)
		   << " of " << APT::PrettyPkg(this, Pkg) << " as its mode (" << PrintMode(P.Mode)
		   << ") is protected" << std::endl;
      return false;
   }
   // enforce dpkg holds
   else if (mode != ModeKeep && Pkg->SelectedState == pkgCache::State::Hold &&
	    _config->FindB("APT::Ignore-Hold",false) == false)
   {
      if (unlikely(DebugMarker == true))
	 std::clog << OutputInDepth(Depth) << "Hold prevents Mark" << PrintMode(mode)
		   << " of " << APT::PrettyPkg(this, Pkg) << std::endl;
      return false;
   }

   return true;
}
									/*}}}*/
// DepCache::MarkInstall - Put the package in the install state		/*{{{*/
// ---------------------------------------------------------------------
/* */
struct CompareProviders {
   pkgCache::PkgIterator const Pkg;
   explicit CompareProviders(pkgCache::DepIterator const &Dep) : Pkg(Dep.TargetPkg()) {};
   //bool operator() (APT::VersionList::iterator const &AV, APT::VersionList::iterator const &BV)
   bool operator() (pkgCache::VerIterator const &AV, pkgCache::VerIterator const &BV)
   {
      pkgCache::PkgIterator const A = AV.ParentPkg();
      pkgCache::PkgIterator const B = BV.ParentPkg();
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
	    return instA == false;
      }
      if ((A->CurrentVer == 0 || B->CurrentVer == 0) && A->CurrentVer != B->CurrentVer)
	 return A->CurrentVer == 0;
      // Prefer packages in the same group as the target; e.g. foo:i386, foo:amd64
      if (A->Group != B->Group)
      {
	 if (A->Group == Pkg->Group && B->Group != Pkg->Group)
	    return false;
	 else if (B->Group == Pkg->Group && A->Group != Pkg->Group)
	    return true;
      }
      // we like essentials
      if ((A->Flags & pkgCache::Flag::Essential) != (B->Flags & pkgCache::Flag::Essential))
      {
	 if ((A->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    return false;
	 else if ((B->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	    return true;
      }
      if ((A->Flags & pkgCache::Flag::Important) != (B->Flags & pkgCache::Flag::Important))
      {
	 if ((A->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	    return false;
	 else if ((B->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	    return true;
      }
      // prefer native architecture
      if (strcmp(A.Arch(), B.Arch()) != 0)
      {
	 if (strcmp(A.Arch(), A.Cache()->NativeArch()) == 0)
	    return false;
	 else if (strcmp(B.Arch(), B.Cache()->NativeArch()) == 0)
	    return true;
	 std::vector<std::string> archs = APT::Configuration::getArchitectures();
	 for (std::vector<std::string>::const_iterator a = archs.begin(); a != archs.end(); ++a)
	    if (*a == A.Arch())
	       return false;
	    else if (*a == B.Arch())
	       return true;
      }
      // higher priority seems like a good idea
      if (AV->Priority != BV->Priority)
	 return AV->Priority > BV->Priority;
      // unable to decide…
      return A->ID < B->ID;
   }
};
bool pkgDepCache::MarkInstall(PkgIterator const &Pkg,bool AutoInst,
			      unsigned long Depth, bool FromUser,
			      bool ForceImportantDeps)
{
   if (IsModeChangeOk(ModeInstall, Pkg, Depth, FromUser) == false)
      return false;

   StateCache &P = PkgState[Pkg->ID];

   // See if there is even any possible installation candidate
   if (P.CandidateVer == 0)
      return false;

   /* Check that it is not already marked for install and that it can be 
      installed */
   if ((P.InstPolicyBroken() == false && P.InstBroken() == false) && 
       (P.Mode == ModeInstall ||
	P.CandidateVer == (Version *)Pkg.CurrentVer()))
   {
      if (P.CandidateVer == (Version *)Pkg.CurrentVer() && P.InstallVer == 0)
	 return MarkKeep(Pkg, false, FromUser, Depth+1);
      return true;
   }

   // check if we are allowed to install the package
   if (IsInstallOk(Pkg,AutoInst,Depth,FromUser) == false)
      return false;

   ActionGroup group(*this);
   P.iFlags &= ~AutoKept;

   /* Target the candidate version and remove the autoflag. We reset the
      autoflag below if this was called recursively. Otherwise the user
      should have the ability to de-auto a package by changing its state */
   RemoveSizes(Pkg);
   RemoveStates(Pkg);
   
   P.Mode = ModeInstall;
   P.InstallVer = P.CandidateVer;

   if(FromUser)
     {
       // Set it to manual if it's a new install or already installed,
       // but only if its not marked by the autoremover (aptitude depend on this behavior)
       // or if we do automatic installation (aptitude never does it)
       if(P.Status == 2 || (Pkg->CurrentVer != 0 && (AutoInst == true || P.Marked == false)))
	 P.Flags &= ~Flag::Auto;
     }
   else
     {
       // Set it to auto if this is a new install.
       if(P.Status == 2)
	 P.Flags |= Flag::Auto;
     }
   if (P.CandidateVer == (Version *)Pkg.CurrentVer())
      P.Mode = ModeKeep;
       
   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);

   if (AutoInst == false || _config->Find("APT::Solver", "internal") != "internal")
      return true;

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << "MarkInstall " << APT::PrettyPkg(this, Pkg) << " FU=" << FromUser << std::endl;

   bool MoveAutoBitToDependencies = false;
   VerIterator const PV = P.InstVerIter(*this);
   if (unlikely(PV.end() == true))
      return false;
   else if (PV->Section != 0 && (P.Flags & Flag::Auto) != Flag::Auto)
   {
      VerIterator const CurVer = Pkg.CurrentVer();
      if (CurVer.end() == false && CurVer->Section != 0 && strcmp(CurVer.Section(), PV.Section()) != 0)
      {
	 bool const CurVerInMoveSection = ConfigValueInSubTree("APT::Move-Autobit-Sections", CurVer.Section());
	 bool const InstVerInMoveSection = ConfigValueInSubTree("APT::Move-Autobit-Sections", PV.Section());
	 MoveAutoBitToDependencies = (CurVerInMoveSection == false && InstVerInMoveSection == true);
	 if (MoveAutoBitToDependencies == true)
	 {
	    if(DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "Setting " << Pkg.FullName(false) << " as auto-installed, moving manual to its dependencies" << std::endl;
	    MarkAuto(Pkg, true);
	 }
      }
   }

   DepIterator Dep = PV.DependsList();
   for (; Dep.end() != true;)
   {
      // Grok or groups
      DepIterator Start = Dep;
      bool Result = true;
      unsigned Ors = 0;
      for (bool LastOR = true; Dep.end() == false && LastOR == true; ++Dep, ++Ors)
      {
	 LastOR = (Dep->CompareOp & Dep::Or) == Dep::Or;

	 if ((DepState[Dep->ID] & DepInstall) == DepInstall)
	    Result = false;
      }
      
      // Dep is satisfied okay.
      if (Result == false)
	 continue;

      /* Check if this dep should be consider for install. If it is a user
         defined important dep and we are installed a new package then
	 it will be installed. Otherwise we only check for important
         deps that have changed from the installed version */
      if (IsImportantDep(Start) == false)
	 continue;

      /* If we are in an or group locate the first or that can
         succeed. We have already cached this… */
      for (; Ors > 1 && (DepState[Start->ID] & DepCVer) != DepCVer; --Ors)
	 ++Start;

      /* unsatisfiable dependency: IsInstallOkDependenciesSatisfiableByCandidates
         would have prevented us to get here if not overridden, so just skip
	 over the problem here as the front-end will know what it is doing */
      if (Ors == 1 && (DepState[Start->ID] &DepCVer) != DepCVer && Start.IsNegative() == false)
	 continue;

      /* Check if any ImportantDep() (but not Critical) were added
       * since we installed the package.  Also check for deps that
       * were satisfied in the past: for instance, if a version
       * restriction in a Recommends was tightened, upgrading the
       * package should follow that Recommends rather than causing the
       * dependency to be removed. (bug #470115)
       */
      if (Pkg->CurrentVer != 0 && ForceImportantDeps == false && Start.IsCritical() == false)
      {
	 bool isNewImportantDep = true;
	 bool isPreviouslySatisfiedImportantDep = false;
	 for (DepIterator D = Pkg.CurrentVer().DependsList(); D.end() != true; ++D)
	 {
	    //FIXME: Should we handle or-group better here?
	    // We do not check if the package we look for is part of the same or-group
	    // we might find while searching, but could that really be a problem?
	    if (D.IsCritical() == true || IsImportantDep(D) == false ||
		Start.TargetPkg() != D.TargetPkg())
	       continue;

	    isNewImportantDep = false;

	    while ((D->CompareOp & Dep::Or) != 0)
	       ++D;

	    isPreviouslySatisfiedImportantDep = (((*this)[D] & DepGNow) != 0);
	    if (isPreviouslySatisfiedImportantDep == true)
	       break;
	 }

	 if(isNewImportantDep == true)
	 {
	    if (DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "new important dependency: "
			 << Start.TargetPkg().FullName() << std::endl;
	 }
	 else if(isPreviouslySatisfiedImportantDep == true)
	 {
	    if (DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "previously satisfied important dependency on "
			 << Start.TargetPkg().FullName() << std::endl;
	 }
	 else
	 {
	    if (DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "ignore old unsatisfied important dependency on "
			 << Start.TargetPkg().FullName() << std::endl;
	    continue;
	 }
      }

      /* This bit is for processing the possibility of an install/upgrade
         fixing the problem for "positive" dependencies */
      if (Start.IsNegative() == false && (DepState[Start->ID] & DepCVer) == DepCVer)
      {
	 pkgCacheFile CacheFile(this);
	 APT::VersionList verlist = APT::VersionList::FromDependency(CacheFile, Start, APT::CacheSetHelper::CANDIDATE);
	 CompareProviders comp(Start);

	 do {
	    APT::VersionList::iterator InstVer = std::max_element(verlist.begin(), verlist.end(), comp);

	    if (InstVer == verlist.end())
	       break;

	    pkgCache::PkgIterator InstPkg = InstVer.ParentPkg();
	    if(DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "Installing " << InstPkg.Name()
			 << " as " << Start.DepType() << " of " << Pkg.Name()
			 << std::endl;
	    if (MarkInstall(InstPkg, true, Depth + 1, false, ForceImportantDeps) == false)
	    {
	       verlist.erase(InstVer);
	       continue;
	    }

	    // now check if we should consider it a automatic dependency or not
	    if(InstPkg->CurrentVer == 0 && MoveAutoBitToDependencies)
	    {
	       if(DebugAutoInstall == true)
		  std::clog << OutputInDepth(Depth) << "Setting " << InstPkg.FullName(false) << " NOT as auto-installed (direct "
		     << Start.DepType() << " of " << Pkg.FullName(false) << " which is manual and in APT::Move-Autobit-Sections)" << std::endl;
	       MarkAuto(InstPkg, false);
	    }


	    break;
	 } while(true);
	 continue;
      }
      /* Negative dependencies have no or-group
	 If the dependency isn't versioned, we try if an upgrade might solve the problem.
	 Otherwise we remove the offender if needed */
      else if (Start.IsNegative() == true && Start->Type != pkgCache::Dep::Obsoletes)
      {
	 std::unique_ptr<Version *[]> List(Start.AllTargets());
	 pkgCache::PkgIterator TrgPkg = Start.TargetPkg();
	 for (Version **I = List.get(); *I != 0; I++)
	 {
	    VerIterator Ver(*this,*I);
	    PkgIterator Pkg = Ver.ParentPkg();

	    /* The List includes all packages providing this dependency,
	       even providers which are not installed, so skip them. */
	    if (PkgState[Pkg->ID].InstallVer == 0)
	       continue;

            /* Ignore negative dependencies that we are not going to 
               get installed */
            if (PkgState[Pkg->ID].InstallVer != *I)
               continue;

	    if ((Start->Version != 0 || TrgPkg != Pkg) &&
		PkgState[Pkg->ID].CandidateVer != PkgState[Pkg->ID].InstallVer &&
		PkgState[Pkg->ID].CandidateVer != *I &&
		MarkInstall(Pkg,true,Depth + 1, false, ForceImportantDeps) == true)
	       continue;
	    else if (Start->Type == pkgCache::Dep::Conflicts || 
                     Start->Type == pkgCache::Dep::DpkgBreaks) 
            {
               if(DebugAutoInstall == true)
                  std::clog << OutputInDepth(Depth) 
                            << " Removing: " << Pkg.Name()
                            << std::endl;
               if (MarkDelete(Pkg,false,Depth + 1, false) == false)
                  break;
            }
	 }
	 continue;
      }
   }

   return Dep.end() == true;
}
									/*}}}*/
// DepCache::IsInstallOk - check if it is ok to install this package	/*{{{*/
// ---------------------------------------------------------------------
/* The default implementation checks if the installation of an M-A:same
   package would lead us into a version-screw and if so forbids it.
   dpkg holds are enforced by the private IsModeChangeOk */
bool pkgDepCache::IsInstallOk(PkgIterator const &Pkg,bool AutoInst,
			      unsigned long Depth, bool FromUser)
{
   return IsInstallOkMultiArchSameVersionSynced(Pkg,AutoInst, Depth, FromUser) &&
      IsInstallOkDependenciesSatisfiableByCandidates(Pkg,AutoInst, Depth, FromUser);
}
bool pkgDepCache::IsInstallOkMultiArchSameVersionSynced(PkgIterator const &Pkg,
      bool const /*AutoInst*/, unsigned long const Depth, bool const FromUser)
{
   if (FromUser == true) // as always: user is always right
      return true;

   // if we have checked before and it was okay, it will still be okay
   if (PkgState[Pkg->ID].Mode == ModeInstall &&
	 PkgState[Pkg->ID].InstallVer == PkgState[Pkg->ID].CandidateVer)
      return true;

   // ignore packages with none-M-A:same candidates
   VerIterator const CandVer = PkgState[Pkg->ID].CandidateVerIter(*this);
   if (unlikely(CandVer.end() == true) || CandVer == Pkg.CurrentVer() ||
	 (CandVer->MultiArch & pkgCache::Version::Same) != pkgCache::Version::Same)
      return true;

   GrpIterator const Grp = Pkg.Group();
   for (PkgIterator P = Grp.PackageList(); P.end() == false; P = Grp.NextPkg(P))
   {
      // not installed or self-check: fine by definition
      if (P->CurrentVer == 0 || P == Pkg)
	 continue;

      // not having a candidate or being in sync
      // (simple string-compare as stuff like '1' == '0:1-0' can't happen here)
      VerIterator CV = PkgState[P->ID].CandidateVerIter(*this);
      if (CV.end() == true || strcmp(CandVer.VerStr(), CV.VerStr()) == 0)
	 continue;

      // packages losing M-A:same can be out-of-sync
      if ((CV->MultiArch & pkgCache::Version::Same) != pkgCache::Version::Same)
	 continue;

      // not downloadable means the package is obsolete, so allow out-of-sync
      if (CV.Downloadable() == false)
	 continue;

      PkgState[Pkg->ID].iFlags |= AutoKept;
      if (unlikely(DebugMarker == true))
	 std::clog << OutputInDepth(Depth) << "Ignore MarkInstall of " << APT::PrettyPkg(this, Pkg)
	    << " as it is not in sync with its M-A:same sibling " << APT::PrettyPkg(this, P)
	    << " (" << CandVer.VerStr() << " != " << CV.VerStr() << ")" << std::endl;
      return false;
   }

   return true;
}
bool pkgDepCache::IsInstallOkDependenciesSatisfiableByCandidates(PkgIterator const &Pkg,
      bool const AutoInst, unsigned long const Depth, bool const /*FromUser*/)
{
   if (AutoInst == false)
      return true;

   VerIterator const CandVer = PkgState[Pkg->ID].CandidateVerIter(*this);
   if (unlikely(CandVer.end() == true) || CandVer == Pkg.CurrentVer())
      return true;

   for (DepIterator Dep = CandVer.DependsList(); Dep.end() != true;)
   {
      // Grok or groups
      DepIterator Start = Dep;
      bool Result = true;
      unsigned Ors = 0;
      for (bool LastOR = true; Dep.end() == false && LastOR == true; ++Dep, ++Ors)
      {
	 LastOR = (Dep->CompareOp & Dep::Or) == Dep::Or;

	 if ((DepState[Dep->ID] & DepInstall) == DepInstall)
	    Result = false;
      }

      if (Start.IsCritical() == false || Start.IsNegative() == true || Result == false)
	 continue;

      /* If we are in an or group locate the first or that can succeed.
         We have already cached this… */
      for (; Ors > 1 && (DepState[Start->ID] & DepCVer) != DepCVer; --Ors)
	 ++Start;

      if (Ors == 1 && (DepState[Start->ID] &DepCVer) != DepCVer)
      {
	 if (DebugAutoInstall == true)
	    std::clog << OutputInDepth(Depth) << APT::PrettyDep(this, Start) << " can't be satisfied!" << std::endl;

	 // the dependency is critical, but can't be installed, so discard the candidate
	 // as the problemresolver will trip over it otherwise trying to install it (#735967)
	 if (Pkg->CurrentVer != 0 && (PkgState[Pkg->ID].iFlags & Protected) != Protected)
         {
	    SetCandidateVersion(Pkg.CurrentVer());
            StateCache &State = PkgState[Pkg->ID];
	    if (State.Mode != ModeDelete)
	    {
	       State.Mode = ModeKeep;
	       State.Update(Pkg, *this);
	    }
         }
	 return false;
      }
   }

   return true;
}
									/*}}}*/
// DepCache::SetReInstall - Set the reinstallation flag			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::SetReInstall(PkgIterator const &Pkg,bool To)
{
   if (unlikely(Pkg.end() == true))
      return;

   APT::PackageList pkglist;
   if (Pkg->CurrentVer != 0 &&
       (Pkg.CurrentVer()-> MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
   {
      pkgCache::GrpIterator Grp = Pkg.Group();
      for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() == false; P = Grp.NextPkg(P))
      {
	 if (P->CurrentVer != 0)
	    pkglist.insert(P);
      }
   }
   else
      pkglist.insert(Pkg);

   ActionGroup group(*this);

   for (APT::PackageList::const_iterator Pkg = pkglist.begin(); Pkg != pkglist.end(); ++Pkg)
   {
      RemoveSizes(Pkg);
      RemoveStates(Pkg);

      StateCache &P = PkgState[Pkg->ID];
      if (To == true)
	 P.iFlags |= ReInstall;
      else
	 P.iFlags &= ~ReInstall;

      AddStates(Pkg);
      AddSizes(Pkg);
   }
}
									/*}}}*/
pkgCache::VerIterator pkgDepCache::GetCandidateVersion(PkgIterator const &Pkg)/*{{{*/
{
   return PkgState[Pkg->ID].CandidateVerIter(*this);
}
									/*}}}*/
// DepCache::SetCandidateVersion - Change the candidate version		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::SetCandidateVersion(VerIterator TargetVer)
{
   pkgCache::PkgIterator Pkg = TargetVer.ParentPkg();
   StateCache &P = PkgState[Pkg->ID];

   if (P.CandidateVer == TargetVer)
      return;

   ActionGroup group(*this);

   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   if (P.CandidateVer == P.InstallVer && P.Install() == true)
      P.InstallVer = (Version *)TargetVer;
   P.CandidateVer = (Version *)TargetVer;
   P.Update(Pkg,*this);
   
   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);

}
									/*}}}*/
// DepCache::SetCandidateRelease - Change the candidate version		/*{{{*/
// ---------------------------------------------------------------------
/* changes the candidate of a package and walks over all its dependencies
   to check if it needs to change the candidate of the dependency, too,
   to reach a installable versionstate */
bool pkgDepCache::SetCandidateRelease(pkgCache::VerIterator TargetVer,
					std::string const &TargetRel)
{
   std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > Changed;
   return SetCandidateRelease(TargetVer, TargetRel, Changed);
}
bool pkgDepCache::SetCandidateRelease(pkgCache::VerIterator TargetVer,
					std::string const &TargetRel,
					std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> > &Changed)
{
   ActionGroup group(*this);
   SetCandidateVersion(TargetVer);

   if (TargetRel == "installed" || TargetRel == "candidate") // both doesn't make sense in this context
      return true;

   pkgVersionMatch Match(TargetRel, pkgVersionMatch::Release);
   // save the position of the last element we will not undo - if we have to
   std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> >::iterator newChanged = --(Changed.end());

   for (pkgCache::DepIterator D = TargetVer.DependsList(); D.end() == false; ++D)
   {
      if (D->Type != pkgCache::Dep::PreDepends && D->Type != pkgCache::Dep::Depends &&
	  ((D->Type != pkgCache::Dep::Recommends && D->Type != pkgCache::Dep::Suggests) ||
	   IsImportantDep(D) == false))
	 continue;

      // walk over an or-group and check if we need to do anything
      // for simpilicity no or-group is handled as a or-group including one dependency
      pkgCache::DepIterator Start = D;
      bool itsFine = false;
      for (bool stillOr = true; stillOr == true; ++Start)
      {
	 stillOr = (Start->CompareOp & Dep::Or) == Dep::Or;
	 pkgCache::PkgIterator const P = Start.TargetPkg();
	 // virtual packages can't be a solution
	 if (P.end() == true || (P->ProvidesList == 0 && P->VersionList == 0))
	    continue;
	 pkgCache::VerIterator const Cand = PkgState[P->ID].CandidateVerIter(*this);
	 // no versioned dependency - but is it installable?
	 if (Start.TargetVer() == 0 || Start.TargetVer()[0] == '\0')
	 {
	    // Check if one of the providers is installable
	    if (P->ProvidesList != 0)
	    {
	       pkgCache::PrvIterator Prv = P.ProvidesList();
	       for (; Prv.end() == false; ++Prv)
	       {
		  pkgCache::VerIterator const C = PkgState[Prv.OwnerPkg()->ID].CandidateVerIter(*this);
		  if (C.end() == true || C != Prv.OwnerVer() ||
		      (VersionState(C.DependsList(), DepInstall, DepCandMin, DepCandPolicy) & DepCandMin) != DepCandMin)
		     continue;
		  break;
	       }
	       if (Prv.end() == true)
		  continue;
	    }
	    // no providers, so check if we have an installable candidate version
	    else if (Cand.end() == true ||
		(VersionState(Cand.DependsList(), DepInstall, DepCandMin, DepCandPolicy) & DepCandMin) != DepCandMin)
	       continue;
	    itsFine = true;
	    break;
	 }
	 if (Cand.end() == true)
	    continue;
	 // check if the current candidate is enough for the versioned dependency - and installable?
	 if (Start.IsSatisfied(Cand) == true &&
	     (VersionState(Cand.DependsList(), DepInstall, DepCandMin, DepCandPolicy) & DepCandMin) == DepCandMin)
	 {
	    itsFine = true;
	    break;
	 }
      }

      if (itsFine == true) {
	 // something in the or-group was fine, skip all other members
	 for (; (D->CompareOp & Dep::Or) == Dep::Or; ++D);
	 continue;
      }

      // walk again over the or-group and check each if a candidate switch would help
      itsFine = false;
      for (bool stillOr = true; stillOr == true; ++D)
      {
	 stillOr = (D->CompareOp & Dep::Or) == Dep::Or;
	 // changing candidate will not help if the dependency is not versioned
	 if (D.TargetVer() == 0 || D.TargetVer()[0] == '\0')
	 {
	    if (stillOr == true)
	       continue;
	    break;
	 }

	 pkgCache::VerIterator V;
	 if (TargetRel == "newest")
	    V = D.TargetPkg().VersionList();
	 else
	    V = Match.Find(D.TargetPkg());

	 // check if the version from this release could satisfy the dependency
	 if (V.end() == true || D.IsSatisfied(V) == false)
	 {
	    if (stillOr == true)
	       continue;
	    break;
	 }

	 pkgCache::VerIterator oldCand = PkgState[D.TargetPkg()->ID].CandidateVerIter(*this);
	 if (V == oldCand)
	 {
	    // Do we already touched this Version? If so, their versioned dependencies are okay, no need to check again
	    for (std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> >::const_iterator c = Changed.begin();
		 c != Changed.end(); ++c)
	    {
	       if (c->first->ParentPkg != V->ParentPkg)
		  continue;
	       itsFine = true;
	       break;
	    }
	 }

	 if (itsFine == false)
	 {
	    // change the candidate
	    Changed.push_back(make_pair(V, TargetVer));
	    if (SetCandidateRelease(V, TargetRel, Changed) == false)
	    {
	       if (stillOr == false)
		  break;
	       // undo the candidate changing
	       SetCandidateVersion(oldCand);
	       Changed.pop_back();
	       continue;
	    }
	    itsFine = true;
	 }

	 // something in the or-group was fine, skip all other members
	 for (; (D->CompareOp & Dep::Or) == Dep::Or; ++D);
	 break;
      }

      if (itsFine == false && (D->Type == pkgCache::Dep::PreDepends || D->Type == pkgCache::Dep::Depends))
      {
	 // undo all changes which aren't lead to a solution
	 for (std::list<std::pair<pkgCache::VerIterator, pkgCache::VerIterator> >::const_iterator c = ++newChanged;
	      c != Changed.end(); ++c)
	    SetCandidateVersion(c->first);
	 Changed.erase(newChanged, Changed.end());
	 return false;
      }
   }
   return true;
}
									/*}}}*/
// DepCache::MarkAuto - set the Auto flag for a package			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkAuto(const PkgIterator &Pkg, bool Auto)
{
  StateCache &state = PkgState[Pkg->ID];

  ActionGroup group(*this);

  if(Auto)
    state.Flags |= Flag::Auto;
  else
    state.Flags &= ~Flag::Auto;
}
									/*}}}*/
// StateCache::Update - Compute the various static display things	/*{{{*/
// ---------------------------------------------------------------------
/* This is called whenever the Candidate version changes. */
void pkgDepCache::StateCache::Update(PkgIterator Pkg,pkgCache &Cache)
{
   // Some info
   VerIterator Ver = CandidateVerIter(Cache);
   
   // Use a null string or the version string
   if (Ver.end() == true)
      CandVersion = "";
   else
      CandVersion = Ver.VerStr();
   
   // Find the current version
   CurVersion = "";
   if (Pkg->CurrentVer != 0)
      CurVersion = Pkg.CurrentVer().VerStr();

   // Figure out if its up or down or equal
   Status = Ver.CompareVer(Pkg.CurrentVer());
   if (Pkg->CurrentVer == 0 || Pkg->VersionList == 0 || CandidateVer == 0)
     Status = 2;      
}
									/*}}}*/
// StateCache::StripEpoch - Remove the epoch specifier from the version	/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgDepCache::StateCache::StripEpoch(const char *Ver)
{
   if (Ver == 0)
      return 0;

   // Strip any epoch
   char const * const I = strchr(Ver, ':');
   if (I == nullptr)
      return Ver;
   return I + 1;
}
									/*}}}*/
// Policy::GetCandidateVer - Returns the Candidate install version	/*{{{*/
// ---------------------------------------------------------------------
/* The default just returns the highest available version that is not
   a source and automatic. */
pkgCache::VerIterator pkgDepCache::Policy::GetCandidateVer(PkgIterator const &Pkg)
{
   /* Not source/not automatic versions cannot be a candidate version 
      unless they are already installed */
   VerIterator Last;
   
   for (VerIterator I = Pkg.VersionList(); I.end() == false; ++I)
   {
      if (Pkg.CurrentVer() == I)
	 return I;
      
      for (VerFileIterator J = I.FileList(); J.end() == false; ++J)
      {
	 if (J.File().Flagged(Flag::NotSource))
	    continue;

	 /* Stash the highest version of a not-automatic source, we use it
	    if there is nothing better */
	 if (J.File().Flagged(Flag::NotAutomatic) ||
	     J.File().Flagged(Flag::ButAutomaticUpgrades))
	 {
	    if (Last.end() == true)
	       Last = I;
	    continue;
	 }

	 return I;
      }
   }
   
   return Last;
}
									/*}}}*/
// Policy::IsImportantDep - True if the dependency is important		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDepCache::Policy::IsImportantDep(DepIterator const &Dep) const
{
   if(Dep.IsCritical())
      return true;
   else if(Dep->Type == pkgCache::Dep::Recommends)
   {
      if (InstallRecommends)
	 return true;
      // we support a special mode to only install-recommends for certain
      // sections
      // FIXME: this is a meant as a temporarly solution until the
      //        recommends are cleaned up
      const char *sec = Dep.ParentVer().Section();
      if (sec && ConfigValueInSubTree("APT::Install-Recommends-Sections", sec))
	 return true;
   }
   else if(Dep->Type == pkgCache::Dep::Suggests)
      return InstallSuggests;

   return false;
}
									/*}}}*/
// Policy::GetPriority - Get the priority of the package pin		/*{{{*/
APT_CONST signed short pkgDepCache::Policy::GetPriority(pkgCache::PkgIterator const &/*Pkg*/)
{ return 0; }
APT_CONST signed short pkgDepCache::Policy::GetPriority(pkgCache::VerIterator const &/*Ver*/, bool /*ConsiderFiles*/)
{ return 0; }
APT_CONST signed short pkgDepCache::Policy::GetPriority(pkgCache::PkgFileIterator const &/*File*/)
{ return 0; }
									/*}}}*/
pkgDepCache::InRootSetFunc *pkgDepCache::GetRootSetFunc()		/*{{{*/
{
  DefaultRootSetFunc *f = new DefaultRootSetFunc;
  if(f->wasConstructedSuccessfully())
    return f;
  else
    {
      delete f;
      return NULL;
    }
}
									/*}}}*/
bool pkgDepCache::MarkFollowsRecommends()
{
  return _config->FindB("APT::AutoRemove::RecommendsImportant", true);
}

bool pkgDepCache::MarkFollowsSuggests()
{
  return _config->FindB("APT::AutoRemove::SuggestsImportant", true);
}

// pkgDepCache::MarkRequired - the main mark algorithm			/*{{{*/
static bool IsPkgInBoringState(pkgCache::PkgIterator const &Pkg, pkgDepCache::StateCache const * const PkgState)
{
   if (Pkg->CurrentVer == 0)
   {
      if (PkgState[Pkg->ID].Keep())
	 return true;
   }
   else
   {
      if (PkgState[Pkg->ID].Delete())
	 return true;
   }
   return false;
}
bool pkgDepCache::MarkRequired(InRootSetFunc &userFunc)
{
   if (_config->Find("APT::Solver", "internal") != "internal")
      return true;

   bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);

   // init the states
   auto const PackagesCount = Head().PackageCount;
   for(auto i = decltype(PackagesCount){0}; i < PackagesCount; ++i)
   {
      PkgState[i].Marked  = false;
      PkgState[i].Garbage = false;
   }
   if (debug_autoremove)
      for(PkgIterator p = PkgBegin(); !p.end(); ++p)
	 if(PkgState[p->ID].Flags & Flag::Auto)
	    std::clog << "AutoDep: " << p.FullName() << std::endl;

   bool const follow_recommends = MarkFollowsRecommends();
   bool const follow_suggests   = MarkFollowsSuggests();

   // do the mark part, this is the core bit of the algorithm
   for (PkgIterator P = PkgBegin(); !P.end(); ++P)
   {
      if (PkgState[P->ID].Marked || IsPkgInBoringState(P, PkgState))
	 continue;

      if ((PkgState[P->ID].Flags & Flag::Auto) == 0)
	 ;
      else if ((P->Flags & Flag::Essential) || (P->Flags & Flag::Important))
	 ;
      // be nice even then a required package violates the policy (#583517)
      // and do the full mark process also for required packages
      else if (P->CurrentVer != 0 && P.CurrentVer()->Priority == pkgCache::State::Required)
	 ;
      else if (userFunc.InRootSet(P))
	 ;
      // packages which can't be changed (like holds) can't be garbage
      else if (IsModeChangeOk(ModeGarbage, P, 0, false) == false)
	 ;
      else
	 continue;

      if (PkgState[P->ID].Install())
	 MarkPackage(P, PkgState[P->ID].InstVerIter(*this),
	       follow_recommends, follow_suggests);
      else
	 MarkPackage(P, P.CurrentVer(),
	       follow_recommends, follow_suggests);
   }

   return true;
}
									/*}}}*/
// MarkPackage - mark a single package in Mark-and-Sweep		/*{{{*/
void pkgDepCache::MarkPackage(const pkgCache::PkgIterator &Pkg,
			      const pkgCache::VerIterator &Ver,
			      bool const &follow_recommends,
			      bool const &follow_suggests)
{
   {
      pkgDepCache::StateCache &state = PkgState[Pkg->ID];
      // if we are marked already we are done
      if(state.Marked || unlikely(Ver.end()))
	 return;
      state.Marked=true;
   }

   if (IsPkgInBoringState(Pkg, PkgState))
      return;

   bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove", false);
   if(debug_autoremove)
      std::clog << "Marking: " << Pkg.FullName() << " " << Ver.VerStr() << std::endl;

   for (auto D = Ver.DependsList(); D.end() == false; ++D)
   {
      auto const T = D.TargetPkg();
      if (PkgState[T->ID].Marked)
	 continue;

      if (D->Type != Dep::Depends &&
	    D->Type != Dep::PreDepends &&
	    (follow_recommends == false || D->Type != Dep::Recommends) &&
	    (follow_suggests == false || D->Type != Dep::Suggests))
	 continue;

      // handle the virtual part first
      APT::VersionVector providers;
      for(auto Prv = T.ProvidesList(); Prv.end() == false; ++Prv)
      {
	 auto PP = Prv.OwnerPkg();
	 if (IsPkgInBoringState(PP, PkgState))
	    continue;

	 // we want to ignore provides from uninteresting versions
	 auto const PV = (PkgState[PP->ID].Install()) ?
	    PkgState[PP->ID].InstVerIter(*this) : PP.CurrentVer();
	 if (unlikely(PV.end()) || PV != Prv.OwnerVer() || D.IsSatisfied(Prv) == false)
	    continue;

	 providers.emplace_back(PV);
      }
      if (providers.empty() == false)
      {
	 // sort providers by source version so that only the latest versioned
	 // binary package of a source package is marked instead of all
	 std::sort(providers.begin(), providers.end(),
	    [](pkgCache::VerIterator const &A, pkgCache::VerIterator const &B) {
	       auto const nameret = strcmp(A.SourcePkgName(), B.SourcePkgName());
	       if (nameret != 0)
	          return nameret < 0;
	       auto const verret = A.Cache()->VS->CmpVersion(A.SourceVerStr(), B.SourceVerStr());
	       if (verret != 0)
	          return verret > 0;
	       return strcmp(A.ParentPkg().Name(), B.ParentPkg().Name()) < 0;
	 });
	 auto const prvsize = providers.size();
	 providers.erase(std::unique(providers.begin(), providers.end(),
	    [](pkgCache::VerIterator const &A, pkgCache::VerIterator const &B) {
	       return strcmp(A.SourcePkgName(), B.SourcePkgName()) == 0 &&
	          strcmp(A.SourceVerStr(), B.SourceVerStr()) != 0;
	    }), providers.end());
	 for (auto && PV: providers)
	 {
	    auto const PP = PV.ParentPkg();
	    if (debug_autoremove)
	       std::clog << "Following dep: " << APT::PrettyDep(this, D)
		  << ", provided by " << PP.FullName() << " " << PV.VerStr()
		  << " (" << providers.size() << "/" << prvsize << ")"<< std::endl;
	    MarkPackage(PP, PV, follow_recommends, follow_suggests);
	 }
      }

      // now deal with the real part of the package
      if (IsPkgInBoringState(T, PkgState))
	 continue;

      auto const TV = (PkgState[T->ID].Install()) ?
	 PkgState[T->ID].InstVerIter(*this) : T.CurrentVer();
      if (unlikely(TV.end()) || D.IsSatisfied(TV) == false)
	 continue;

      if (debug_autoremove)
	 std::clog << "Following dep: " << APT::PrettyDep(this, D) << std::endl;
      MarkPackage(T, TV, follow_recommends, follow_suggests);
   }
}
									/*}}}*/
bool pkgDepCache::Sweep()						/*{{{*/
{
   bool debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);

   // do the sweep
   for(PkgIterator p=PkgBegin(); !p.end(); ++p)
   {
     StateCache &state=PkgState[p->ID];

     // skip required packages
     if (!p.CurrentVer().end() && 
	 (p.CurrentVer()->Priority == pkgCache::State::Required))
	continue;

     // if it is not marked and it is installed, it's garbage 
     if(!state.Marked && (!p.CurrentVer().end() || state.Install()))
     {
	state.Garbage=true;
	if(debug_autoremove)
	   std::clog << "Garbage: " << p.FullName() << std::endl;
     }
  }   

   return true;
}
									/*}}}*/
// DepCache::MarkAndSweep						/*{{{*/
bool pkgDepCache::MarkAndSweep(InRootSetFunc &rootFunc)
{
   return MarkRequired(rootFunc) && Sweep();
}
bool pkgDepCache::MarkAndSweep()
{
   std::unique_ptr<InRootSetFunc> f(GetRootSetFunc());
   if(f.get() != NULL)
      return MarkAndSweep(*f.get());
   else
      return false;
}
									/*}}}*/

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.cc,v 1.14 1998/12/22 08:01:04 jgg Exp $
/* ######################################################################

   Dependency Cache - Caches Dependency information.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/depcache.h"
#endif
#include <apt-pkg/depcache.h>

#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
									/*}}}*/

// DepCache::pkgDepCache - Constructors					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::pkgDepCache(MMap &Map,OpProgress &Prog) :
             pkgCache(Map), PkgState(0), DepState(0)
{
   if (_error->PendingError() == false)
      Init(&Prog);
}
pkgDepCache::pkgDepCache(MMap &Map) :
             pkgCache(Map), PkgState(0), DepState(0)
{
   if (_error->PendingError() == false)
      Init(0);
}
									/*}}}*/
// DepCache::~pkgDepCache - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::~pkgDepCache()
{
   delete [] PkgState;
   delete [] DepState;
}
									/*}}}*/
// DepCache::Init - Generate the initial extra structures.		/*{{{*/
// ---------------------------------------------------------------------
/* This allocats the extension buffers and initializes them. */
bool pkgDepCache::Init(OpProgress *Prog)
{
   delete [] PkgState;
   delete [] DepState;
   PkgState = new StateCache[Head().PackageCount];
   DepState = new unsigned char[Head().DependsCount];
   memset(PkgState,0,sizeof(*PkgState)*Head().PackageCount);
   memset(DepState,0,sizeof(*DepState)*Head().DependsCount); 
   
   if (Prog != 0)
   {
      Prog->OverallProgress(0,2*Head().PackageCount,Head().PackageCount,
			    "Building Dependency Tree");
      Prog->SubProgress(Head().PackageCount,"Candidate Versions");
   }
   
   /* Set the current state of everything. In this state all of the
      packages are kept exactly as is. See AllUpgrade */
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; I++,Done++)
   {
      if (Prog != 0)
	 Prog->Progress(Done);
      
      // Find the proper cache slot
      StateCache &State = PkgState[I->ID];
      State.iFlags = 0;
      
      // Figure out the install version
      State.CandidateVer = GetCandidateVer(I);
      State.InstallVer = I.CurrentVer();
      State.Mode = ModeKeep;
      
      State.Update(I,*this);
   }   
   
   if (Prog != 0)
   {
      
      Prog->OverallProgress(Head().PackageCount,2*Head().PackageCount,
			    Head().PackageCount,
			    "Building Dependency Tree");
      Prog->SubProgress(Head().PackageCount,"Dependency Generation");
   }
   
   Update(Prog);
   
   return true;
} 
									/*}}}*/
// DepCache::GetCandidateVer - Returns the Candidate install version	/*{{{*/
// ---------------------------------------------------------------------
/* The default just returns the target version if it exists or the
   highest version. */
pkgDepCache::VerIterator pkgDepCache::GetCandidateVer(PkgIterator Pkg)
{
   // Try to use an explicit target
   if (Pkg->TargetVer == 0)
   {
      /* Not source/not automatic versions cannot be a candidate version 
         unless they are already installed */
      for (VerIterator I = Pkg.VersionList(); I.end() == false; I++)
      {
	 if (Pkg.CurrentVer() == I)
	    return I;
	 for (VerFileIterator J = I.FileList(); J.end() == false; J++)
	    if ((J.File()->Flags & Flag::NotSource) == 0 &&
		(J.File()->Flags & Flag::NotAutomatic) == 0)
		return I;
      }
	 
      return VerIterator(*this,0);
   }
   else
      return Pkg.TargetVer(); 
}
									/*}}}*/
// DepCache::IsImportantDep - True if the dependency is important	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDepCache::IsImportantDep(DepIterator Dep)
{
   return Dep.IsCritical();
}
									/*}}}*/

// DepCache::CheckDep - Checks a single dependency			/*{{{*/
// ---------------------------------------------------------------------
/* This first checks the dependency against the main target package and
   then walks along the package provides list and checks if each provides 
   will be installed then checks the provides against the dep. Res will be 
   set to the package which was used to satisfy the dep. */
bool pkgDepCache::CheckDep(DepIterator Dep,int Type,PkgIterator &Res)
{
   Res = Dep.TargetPkg();

   /* Check simple depends. A depends -should- never self match but 
      we allow it anyhow because dpkg does. Technically it is a packaging
      bug. Conflicts may never self match */
   if (Dep.TargetPkg() != Dep.ParentPkg() || Dep->Type != Dep::Conflicts)
   {
      PkgIterator Pkg = Dep.TargetPkg();
      // Check the base package
      if (Type == NowVersion && Pkg->CurrentVer != 0)
	 if (pkgCheckDep(Dep.TargetVer(),
			 Pkg.CurrentVer().VerStr(),Dep->CompareOp) == true)
	    return true;
      
      if (Type == InstallVersion && PkgState[Pkg->ID].InstallVer != 0)
	 if (pkgCheckDep(Dep.TargetVer(),
			 PkgState[Pkg->ID].InstVerIter(*this).VerStr(),
			 Dep->CompareOp) == true)
	    return true;
      
      if (Type == CandidateVersion && PkgState[Pkg->ID].CandidateVer != 0)
	 if (pkgCheckDep(Dep.TargetVer(),
			 PkgState[Pkg->ID].CandidateVerIter(*this).VerStr(),
			 Dep->CompareOp) == true)
	    return true;
   }
   
   // Check the providing packages
   PrvIterator P = Dep.TargetPkg().ProvidesList();
   PkgIterator Pkg = Dep.ParentPkg();
   for (; P.end() != true; P++)
   {
      /* Provides may never be applied against the same package if it is
         a conflicts. See the comment above. */
      if (P.OwnerPkg() == Pkg && Dep->Type == Dep::Conflicts)
	 continue;
      
      // Check if the provides is a hit
      if (Type == NowVersion)
      {
	 if (P.OwnerPkg().CurrentVer() != P.OwnerVer())
	    continue;
      }
      
      if (Type == InstallVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.InstallVer != (Version *)P.OwnerVer())
	    continue;
      }

      if (Type == CandidateVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.CandidateVer != (Version *)P.OwnerVer())
	    continue;
      }
      
      // Compare the versions.
      if (pkgCheckDep(Dep.TargetVer(),P.ProvideVersion(),Dep->CompareOp) == true)
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
/* Call with Mult = -1 to preform the inverse opration */
void pkgDepCache::AddSizes(const PkgIterator &Pkg,long Mult)
{
   StateCache &P = PkgState[Pkg->ID];
   
   if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure)
   {
      iUsrSize += Mult*P.InstVerIter(*this)->InstalledSize;
      return;
   }
   
   // Compute the size data
   if (P.NewInstall() == true)
   {
      iUsrSize += Mult*P.InstVerIter(*this)->InstalledSize;
      iDownloadSize += Mult*P.InstVerIter(*this)->Size;
      return;
   }
   
   // Upgrading
   if (Pkg->CurrentVer != 0 && P.InstallVer != (Version *)Pkg.CurrentVer() &&
       P.InstallVer != 0)
   {
      iUsrSize += Mult*((signed)P.InstVerIter(*this)->InstalledSize - 
			(signed)Pkg.CurrentVer()->InstalledSize);
      iDownloadSize += Mult*P.InstVerIter(*this)->Size;
      return;
   }
   
   // Reinstall
   if (Pkg.State() == pkgCache::PkgIterator::NeedsUnpack &&
       P.Delete() == false)
   {
      iDownloadSize += Mult*P.InstVerIter(*this)->Size;
      return;
   }
   
   // Removing
   if (Pkg->CurrentVer != 0 && P.InstallVer == 0)
   {
      iUsrSize -= Mult*Pkg.CurrentVer()->InstalledSize;
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
void pkgDepCache::AddStates(const PkgIterator &Pkg,int Add)
{
   StateCache &State = PkgState[Pkg->ID];
   
   // The Package is broken
   if ((State.DepState & DepInstMin) != DepInstMin)
      iBrokenCount += Add;
   
   // Bad state
   if (Pkg.State() != PkgIterator::NeedsNothing)
      iBadCount += Add;
   
   // Not installed
   if (Pkg->CurrentVer == 0)
   {
      if (State.Mode == ModeInstall)
	 iInstCount += Add;
      return;
   }
   
   // Installed, no upgrade
   if (State.Upgradable() == false)
   {
      if (State.Mode == ModeDelete)
	 iDelCount += Add;
      return;
   }
   
   // Alll 3 are possible
   if (State.Mode == ModeDelete)
      iDelCount += Add;   
   if (State.Mode == ModeKeep)
      iKeepCount += Add;
   if (State.Mode == ModeInstall)
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
   
   for (DepIterator D = V.DependsList(); D.end() != true; D++)
   {
      // Build the dependency state.
      unsigned char &State = DepState[D->ID];

      /* Invert for Conflicts. We have to do this twice to get the
         right sense for a conflicts group */
      if (D->Type == Dep::Conflicts)
	 State = ~State;
      
      // Add to the group if we are within an or..
      State &= 0x7;
      Group |= State;
      State |= Group << 3;
      if ((D->CompareOp & Dep::Or) != Dep::Or)
	 Group = 0;
      
      // Invert for Conflicts
      if (D->Type == Dep::Conflicts)
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
unsigned char pkgDepCache::VersionState(DepIterator D,unsigned char Check,
				       unsigned char SetMin,
				       unsigned char SetPolicy)
{
   unsigned char Dep = 0xFF;
   
   while (D.end() != true)
   {
      // Compute a single dependency element (glob or)
      DepIterator Start = D;
      unsigned char State = 0;
      for (bool LastOR = true; D.end() == false && LastOR == true; D++)
      {
	 State |= DepState[D->ID];
	 LastOR = (D->CompareOp & Dep::Or) == Dep::Or;
      }
	
      // Minimum deps that must be satisfied to have a working package
      if (Start.IsCritical() == true)
	 if ((State & Check) != Check)
	    Dep &= ~SetMin;
      
      // Policy deps that must be satisfied to install the package
      if (IsImportantDep(Start) == true && 
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
unsigned char pkgDepCache::DependencyState(DepIterator &D)
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
void pkgDepCache::UpdateVerState(PkgIterator Pkg)
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
void pkgDepCache::Update(OpProgress *Prog)
{   
   iUsrSize = 0;
   iDownloadSize = 0;
   iDelCount = 0;
   iInstCount = 0;
   iKeepCount = 0;
   iBrokenCount = 0;
   iBadCount = 0;
   
   // Perform the depends pass
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; I++,Done++)
   {
      if (Prog != 0 && Done%20 == 0)
	 Prog->Progress(Done);
      for (VerIterator V = I.VersionList(); V.end() != true; V++)
      {
	 unsigned char Group = 0;
	 
	 for (DepIterator D = V.DependsList(); D.end() != true; D++)
	 {
	    // Build the dependency state.
	    unsigned char &State = DepState[D->ID];
	    State = DependencyState(D);;

	    // Add to the group if we are within an or..
	    Group |= State;
	    State |= Group << 3;
	    if ((D->CompareOp & Dep::Or) != Dep::Or)
	       Group = 0;

	    // Invert for Conflicts
	    if (D->Type == Dep::Conflicts)
	       State = ~State;
	 }	 
      }

      // Compute the pacakge dependency state and size additions
      AddSizes(I);
      UpdateVerState(I);
      AddStates(I);
   }

   if (Prog != 0)      
      Prog->Progress(Done);
}
									/*}}}*/
// DepCache::Update - Update the deps list of a package	   		/*{{{*/
// ---------------------------------------------------------------------
/* This is a helper for update that only does the dep portion of the scan. 
   It is mainly ment to scan reverse dependencies. */
void pkgDepCache::Update(DepIterator D)
{
   // Update the reverse deps
   for (;D.end() != true; D++)
   {      
      unsigned char &State = DepState[D->ID];
      State = DependencyState(D);
    
      // Invert for Conflicts
      if (D->Type == Dep::Conflicts)
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
	   P.end() != true; P++)
	 Update(P.ParentPkg().RevDependsList());

   // Update the provides map for the candidate ver
   for (PrvIterator P = PkgState[Pkg->ID].CandidateVerIter(*this).ProvidesList();
	P.end() != true; P++)
      Update(P.ParentPkg().RevDependsList());
}

									/*}}}*/

// DepCache::MarkKeep - Put the package in the keep state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkKeep(PkgIterator const &Pkg,bool Soft)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;
   
   /* We changed the soft state all the time so the UI is a bit nicer
      to use */
   StateCache &P = PkgState[Pkg->ID];
   if (Soft == true)
      P.iFlags |= AutoKept;
   else
      P.iFlags &= ~AutoKept;
   
   // Check that it is not already kept
   if (P.Mode == ModeKeep)
      return;

   // We dont even try to keep virtual packages..
   if (Pkg->VersionList == 0)
      return;
   
   P.Flags &= ~Flag::Auto;
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
}
									/*}}}*/
// DepCache::MarkDelete - Put the package in the delete state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkDelete(PkgIterator const &Pkg)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;

   // Check that it is not already marked for delete
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~AutoKept;
   if (P.Mode == ModeDelete || P.InstallVer == 0)
      return;

   // We dont even try to delete virtual packages..
   if (Pkg->VersionList == 0)
      return;

   RemoveSizes(Pkg);
   RemoveStates(Pkg);
   
   if (Pkg->CurrentVer == 0)
      P.Mode = ModeKeep;
   else
      P.Mode = ModeDelete;
   P.InstallVer = 0;
   P.Flags &= Flag::Auto;

   AddStates(Pkg);   
   Update(Pkg);
   AddSizes(Pkg);
}
									/*}}}*/
// DepCache::MarkInstall - Put the package in the install state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkInstall(PkgIterator const &Pkg,bool AutoInst)
{   
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;
   
   /* Check that it is not already marked for install and that it can be 
      installed */
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~AutoKept;
   if (P.InstBroken() == false && (P.Mode == ModeInstall ||
	P.CandidateVer == (Version *)Pkg.CurrentVer()))
   {
      if (P.CandidateVer == (Version *)Pkg.CurrentVer() && P.InstallVer == 0)
	 MarkKeep(Pkg);
      return;
   }
   
   // We dont even try to install virtual packages..
   if (Pkg->VersionList == 0)
      return;
   
   /* Target the candidate version and remove the autoflag. We reset the
      autoflag below if this was called recursively. Otherwise the user
      should have the ability to de-auto a package by changing its state */
   RemoveSizes(Pkg);
   RemoveStates(Pkg);
   
   P.Mode = ModeInstall;
   P.InstallVer = P.CandidateVer;
   P.Flags &= ~Flag::Auto;
   if (P.CandidateVer == (Version *)Pkg.CurrentVer())
      P.Mode = ModeKeep;
       
   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);
   
   if (AutoInst == false)
      return;

   DepIterator Dep = P.InstVerIter(*this).DependsList();
   for (; Dep.end() != true;)
   {
      // Grok or groups
      DepIterator Start = Dep;
      bool Result = true;
      for (bool LastOR = true; Dep.end() == false && LastOR == true; Dep++)
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
	 it will be installed. Otherwise we only worry about critical deps */
      if (IsImportantDep(Start) == false)
	 continue;
      if (Pkg->CurrentVer != 0 && Start.IsCritical() == false)
	 continue;

      // Now we have to take action...
      PkgIterator P = Start.SmartTargetPkg();
      if ((DepState[Start->ID] & DepCVer) == DepCVer)
      {
	 MarkInstall(P,true);
	 
	 // Set the autoflag, after MarkInstall because MarkInstall unsets it
	 if (P->CurrentVer == 0)
	    PkgState[P->ID].Flags |= Flag::Auto;

	 continue;
      }
      
      // For conflicts we just de-install the package and mark as auto
      if (Start->Type == Dep::Conflicts)
      {
	 Version **List = Start.AllTargets();
	 for (Version **I = List; *I != 0; I++)
	 {
	    VerIterator Ver(*this,*I);
	    PkgIterator Pkg = Ver.ParentPkg();
      
	    MarkDelete(Pkg);
	    PkgState[Pkg->ID].Flags |= Flag::Auto;
	 }
	 delete [] List;
	 continue;
      }      
   }
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
   
   // Strip off the epochs for display
   CurVersion = StripEpoch(CurVersion);
   CandVersion = StripEpoch(CandVersion);
   
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
   for (const char *I = Ver; *I != 0; I++)
      if (*I == ':')
	 return I + 1;
   return Ver;
}
									/*}}}*/

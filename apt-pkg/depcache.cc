// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: depcache.cc,v 1.25 2001/05/27 05:36:04 jgg Exp $
/* ######################################################################

   Dependency Cache - Caches Dependency information.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/depcache.h>
#include <apt-pkg/version.h>
#include <apt-pkg/versionmatch.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/algorithms.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/tagfile.h>

#include <iostream>
#include <sstream>    
#include <set>

#include <sys/stat.h>

#include <apti18n.h>    
									/*}}}*/
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
  cache(cache), released(false)
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

      released = false;
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
pkgDepCache::pkgDepCache(pkgCache *pCache,Policy *Plcy) :
  group_level(0), Cache(pCache), PkgState(0), DepState(0)
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
bool pkgDepCache::Init(OpProgress *Prog)
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
   for (PkgIterator I = PkgBegin(); I.end() != true; I++,Done++)
   {
      if (Prog != 0 && Done%20 == 0)
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
			    _("Building dependency tree"));
      Prog->SubProgress(Head().PackageCount,_("Dependency generation"));
   }
   
   Update(Prog);

   if(Prog != 0)
      Prog->Done();

   return true;
} 
									/*}}}*/
bool pkgDepCache::readStateFile(OpProgress *Prog)			/*{{{*/
{
   FileFd state_file;
   string const state = _config->FindFile("Dir::State::extended_states");
   if(RealFileExists(state)) {
      state_file.Open(state, FileFd::ReadOnly);
      int const file_size = state_file.Size();
      if(Prog != NULL)
	 Prog->OverallProgress(0, file_size, 1, 
			       _("Reading state information"));

      pkgTagFile tagfile(&state_file);
      pkgTagSection section;
      int amt = 0;
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
bool pkgDepCache::writeStateFile(OpProgress *prog, bool InstalledOnly)	/*{{{*/
{
   bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);
   
   if(debug_autoremove)
      std::clog << "pkgDepCache::writeStateFile()" << std::endl;

   FileFd StateFile;
   string const state = _config->FindFile("Dir::State::extended_states");

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

   FILE *OutFile;
   string const outfile = state + ".tmp";
   if((OutFile = fopen(outfile.c_str(),"w")) == NULL)
      return _error->Error(_("Failed to write temporary StateFile %s"),
			   outfile.c_str());

   // first merge with the existing sections
   pkgTagFile tagfile(&StateFile);
   pkgTagSection section;
   std::set<string> pkgs_seen;
   const char *nullreorderlist[] = {0};
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
	 // skip not installed or now-removed ones if requested
	 if (InstalledOnly && (
	     (pkg->CurrentVer == 0 && P.Mode != ModeInstall) ||
	     (pkg->CurrentVer != 0 && P.Mode == ModeDelete)))
	 {
	    // The section is obsolete if it contains no other tag
	    unsigned int const count = section.Count();
	    if (count < 2 ||
		(count == 2 && section.Exists("Auto-Installed")) ||
		(count == 3 && section.Exists("Auto-Installed") && section.Exists("Architecture")))
	       continue;
	    else
	       newAuto = false;
	 }
	 if(_config->FindB("Debug::pkgAutoRemove",false))
	    std::clog << "Update existing AutoInstall info: " 
		      << pkg.FullName() << std::endl;
	 TFRewriteData rewrite[3];
	 rewrite[0].Tag = "Architecture";
	 rewrite[0].Rewrite = pkg.Arch();
	 rewrite[0].NewTag = 0;
	 rewrite[1].Tag = "Auto-Installed";
	 rewrite[1].Rewrite = newAuto ? "1" : "0";
	 rewrite[1].NewTag = 0;
	 rewrite[2].Tag = 0;
	 TFRewrite(OutFile, section, nullreorderlist, rewrite);
	 fprintf(OutFile,"\n");
	 pkgs_seen.insert(pkg.FullName());
   }
   
   // then write the ones we have not seen yet
   std::ostringstream ostr;
   for(pkgCache::PkgIterator pkg=Cache->PkgBegin(); !pkg.end(); pkg++) {
      StateCache const &P = PkgState[pkg->ID];
      if(P.Flags & Flag::Auto) {
	 if (pkgs_seen.find(pkg.FullName()) != pkgs_seen.end()) {
	    if(debug_autoremove)
	       std::clog << "Skipping already written " << pkg.FullName() << std::endl;
	    continue;
	 }
	 // skip not installed ones if requested
	 if (InstalledOnly && (
	     (pkg->CurrentVer == 0 && P.Mode != ModeInstall) ||
	     (pkg->CurrentVer != 0 && P.Mode == ModeDelete)))
	    continue;
	 const char* const pkgarch = pkg.Arch();
	 if (strcmp(pkgarch, "all") == 0)
	    continue;
	 if(debug_autoremove)
	    std::clog << "Writing new AutoInstall: " << pkg.FullName() << std::endl;
	 ostr.str(string(""));
	 ostr << "Package: " << pkg.Name()
	      << "\nArchitecture: " << pkgarch
	      << "\nAuto-Installed: 1\n\n";
	 fprintf(OutFile,"%s",ostr.str().c_str());
      }
   }
   fclose(OutFile);

   // move the outfile over the real file and set permissions
   rename(outfile.c_str(), state.c_str());
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
bool pkgDepCache::CheckDep(DepIterator Dep,int Type,PkgIterator &Res)
{
   Res = Dep.TargetPkg();

   /* Check simple depends. A depends -should- never self match but 
      we allow it anyhow because dpkg does. Technically it is a packaging
      bug. Conflicts may never self match */
   if (Dep.TargetPkg() != Dep.ParentPkg() ||
       (Dep->Type != Dep::Conflicts && Dep->Type != Dep::DpkgBreaks && Dep->Type != Dep::Obsoletes))
   {
      PkgIterator Pkg = Dep.TargetPkg();
      // Check the base package
      if (Type == NowVersion && Pkg->CurrentVer != 0)
	 if (VS().CheckDep(Pkg.CurrentVer().VerStr(),Dep->CompareOp,
				 Dep.TargetVer()) == true)
	    return true;
      
      if (Type == InstallVersion && PkgState[Pkg->ID].InstallVer != 0)
	 if (VS().CheckDep(PkgState[Pkg->ID].InstVerIter(*this).VerStr(),
				 Dep->CompareOp,Dep.TargetVer()) == true)
	    return true;
      
      if (Type == CandidateVersion && PkgState[Pkg->ID].CandidateVer != 0)
	 if (VS().CheckDep(PkgState[Pkg->ID].CandidateVerIter(*this).VerStr(),
				 Dep->CompareOp,Dep.TargetVer()) == true)
	    return true;
   }
   
   if (Dep->Type == Dep::Obsoletes)
      return false;
   
   // Check the providing packages
   PrvIterator P = Dep.TargetPkg().ProvidesList();
   PkgIterator Pkg = Dep.ParentPkg();
   for (; P.end() != true; P++)
   {
      /* Provides may never be applied against the same package (or group)
         if it is a conflicts. See the comment above. */
      if (P.OwnerPkg()->Group == Pkg->Group &&
	  (Dep->Type == Dep::Conflicts || Dep->Type == Dep::DpkgBreaks))
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
      if (VS().CheckDep(P.ProvideVersion(),Dep->CompareOp,Dep.TargetVer()) == true)
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
/* Call with Mult = -1 to preform the inverse opration
   The Mult increases the complexity of the calulations here and is unused -
   or do we really have a usecase for removing the size of a package two
   times? So let us replace it with a simple bool and be done with it… */
__deprecated void pkgDepCache::AddSizes(const PkgIterator &Pkg,signed long Mult)
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
      iUsrSize += (signed long long)(Mult*P.InstVerIter(*this)->InstalledSize);
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }
   
   // Upgrading
   if (Pkg->CurrentVer != 0 && 
       (P.InstallVer != (Version *)Pkg.CurrentVer() || 
	(P.iFlags & ReInstall) == ReInstall) && P.InstallVer != 0)
   {
      iUsrSize += (signed long long)(Mult*((signed long long)P.InstVerIter(*this)->InstalledSize - 
			(signed long long)Pkg.CurrentVer()->InstalledSize));
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }
   
   // Reinstall
   if (Pkg.State() == pkgCache::PkgIterator::NeedsUnpack &&
       P.Delete() == false)
   {
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }
   
   // Removing
   if (Pkg->CurrentVer != 0 && P.InstallVer == 0)
   {
      iUsrSize -= (signed long long)(Mult*Pkg.CurrentVer()->InstalledSize);
      return;
   }   
}
									/*}}}*/
// DepCache::AddSizes - Add the packages sizes to the counters		/*{{{*/
// ---------------------------------------------------------------------
/* Call with Inverse = true to preform the inverse opration */
void pkgDepCache::AddSizes(const PkgIterator &Pkg, bool const &Inverse)
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
void pkgDepCache::AddStates(const PkgIterator &Pkg,int Add)
{
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
	  (State.iFlags | Purge) == Purge && Pkg.Purge() == false)
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
      if (D->Type == Dep::Conflicts ||
	  D->Type == Dep::DpkgBreaks ||
	  D->Type == Dep::Obsoletes)
	 State = ~State;
      
      // Add to the group if we are within an or..
      State &= 0x7;
      Group |= State;
      State |= Group << 3;
      if ((D->CompareOp & Dep::Or) != Dep::Or)
	 Group = 0;
      
      // Invert for Conflicts
      if (D->Type == Dep::Conflicts ||
	  D->Type == Dep::DpkgBreaks ||
	  D->Type == Dep::Obsoletes)
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
	    State = DependencyState(D);

	    // Add to the group if we are within an or..
	    Group |= State;
	    State |= Group << 3;
	    if ((D->CompareOp & Dep::Or) != Dep::Or)
	       Group = 0;

	    // Invert for Conflicts
	    if (D->Type == Dep::Conflicts ||
		D->Type == Dep::DpkgBreaks ||
		D->Type == Dep::Obsoletes)
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
   for (;D.end() != true; D++)
   {      
      unsigned char &State = DepState[D->ID];
      State = DependencyState(D);
    
      // Invert for Conflicts
      if (D->Type == Dep::Conflicts ||
	  D->Type == Dep::DpkgBreaks ||
	  D->Type == Dep::Obsoletes)
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
   if (PkgState[Pkg->ID].CandidateVer != 0)
      for (PrvIterator P = PkgState[Pkg->ID].CandidateVerIter(*this).ProvidesList();
	   P.end() != true; P++)
	 Update(P.ParentPkg().RevDependsList());
}
									/*}}}*/
// DepCache::MarkKeep - Put the package in the keep state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkKeep(PkgIterator const &Pkg, bool Soft, bool FromUser,
                           unsigned long Depth)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;

   /* Reject an attempt to keep a non-source broken installed package, those
      must be upgraded */
   if (Pkg.State() == PkgIterator::NeedsUnpack && 
       Pkg.CurrentVer().Downloadable() == false)
      return;
   
   /** \todo Can this be moved later in the method? */
   ActionGroup group(*this);

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
#if 0 // reseting the autoflag here means we lose the 
      // auto-mark information if a user selects a package for removal
      // but changes  his mind then and sets it for keep again
      // - this makes sense as default when all Garbage dependencies
      //   are automatically marked for removal (as aptitude does).
      //   setting a package for keep then makes it no longer autoinstalled
      //   for all other use-case this action is rather suprising
   if(FromUser && !P.Marked)
     P.Flags &= ~Flag::Auto;
#endif

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << "MarkKeep " << Pkg << " FU=" << FromUser << std::endl;

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
void pkgDepCache::MarkDelete(PkgIterator const &Pkg, bool rPurge,
                             unsigned long Depth, bool FromUser)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;

   ActionGroup group(*this);

   // Check that it is not already marked for delete
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~(AutoKept | Purge);
   if (rPurge == true)
      P.iFlags |= Purge;
   
   if ((P.Mode == ModeDelete || P.InstallVer == 0) && 
       (Pkg.Purge() == true || rPurge == false))
      return;
   
   // We dont even try to delete virtual packages..
   if (Pkg->VersionList == 0)
      return;

   // check if we are allowed to install the package
   if (IsDeleteOk(Pkg,rPurge,Depth,FromUser) == false)
      return;

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << (rPurge ? "MarkPurge " : "MarkDelete ") << Pkg << " FU=" << FromUser << std::endl;

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

}
									/*}}}*/
// DepCache::IsDeleteOk - check if it is ok to remove this package	/*{{{*/
// ---------------------------------------------------------------------
/* The default implementation just honors dpkg hold
   But an application using this library can override this method
   to control the MarkDelete behaviour */
bool pkgDepCache::IsDeleteOk(PkgIterator const &Pkg,bool rPurge,
			      unsigned long Depth, bool FromUser)
{
   if (FromUser == false && Pkg->SelectedState == pkgCache::State::Hold && _config->FindB("APT::Ignore-Hold",false) == false)
   {
      if (DebugMarker == true)
	 std::clog << OutputInDepth(Depth) << "Hold prevents MarkDelete of " << Pkg << " FU=" << FromUser << std::endl;
      return false;
   }
   // if the removal is not explictely requested by the user, protect
   // explicit new-install package from accidental removal by the 
   // problemresolver
   else if (FromUser == false && Pkg->CurrentVer == 0)
   {
      StateCache &P = PkgState[Pkg->ID];
      // Status == 2 means this applies for new installs only
      if (P.InstallVer != 0 && P.Status == 2 && (P.Flags & Flag::Auto) != Flag::Auto)
      {
	 if (DebugMarker == true)
	    std::clog << OutputInDepth(Depth) << "Manual install request prevents MarkDelete of " << Pkg << std::endl;
	 return false;
      }
   }
   return true;
}
									/*}}}*/
// DepCache::MarkInstall - Put the package in the install state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkInstall(PkgIterator const &Pkg,bool AutoInst,
			      unsigned long Depth, bool FromUser,
			      bool ForceImportantDeps)
{
   if (Depth > 100)
      return;
   
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;
   
   ActionGroup group(*this);

   /* Check that it is not already marked for install and that it can be 
      installed */
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~AutoKept;
   if ((P.InstPolicyBroken() == false && P.InstBroken() == false) && 
       (P.Mode == ModeInstall ||
	P.CandidateVer == (Version *)Pkg.CurrentVer()))
   {
      if (P.CandidateVer == (Version *)Pkg.CurrentVer() && P.InstallVer == 0)
	 MarkKeep(Pkg, false, FromUser, Depth+1);
      return;
   }

   // See if there is even any possible instalation candidate
   if (P.CandidateVer == 0)
      return;
   // We dont even try to install virtual packages..
   if (Pkg->VersionList == 0)
      return;

   // check if we are allowed to install the package
   if (IsInstallOk(Pkg,AutoInst,Depth,FromUser) == false)
      return;

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

   if (AutoInst == false)
      return;

   if (DebugMarker == true)
      std::clog << OutputInDepth(Depth) << "MarkInstall " << Pkg << " FU=" << FromUser << std::endl;

   DepIterator Dep = P.InstVerIter(*this).DependsList();
   for (; Dep.end() != true;)
   {
      // Grok or groups
      DepIterator Start = Dep;
      bool Result = true;
      unsigned Ors = 0;
      for (bool LastOR = true; Dep.end() == false && LastOR == true; Dep++,Ors++)
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
         deps that have changed from the installed version
      */
      if (IsImportantDep(Start) == false)
	 continue;
      
      /* Check if any ImportantDep() (but not Critical) were added
       * since we installed the package.  Also check for deps that
       * were satisfied in the past: for instance, if a version
       * restriction in a Recommends was tightened, upgrading the
       * package should follow that Recommends rather than causing the
       * dependency to be removed. (bug #470115)
       */
      bool isNewImportantDep = false;
      bool isPreviouslySatisfiedImportantDep = false;
      if(!ForceImportantDeps && !Start.IsCritical())
      {
	 bool found=false;
	 VerIterator instVer = Pkg.CurrentVer();
	 if(!instVer.end())
	 {
	   for (DepIterator D = instVer.DependsList(); D.end() != true; D++)
	     {
	       //FIXME: deal better with or-groups(?)
	       if(IsImportantDep(D) && !D.IsCritical() &&
		  Start.TargetPkg() == D.TargetPkg())
		 {
		   if(!isPreviouslySatisfiedImportantDep)
		     {
		       DepIterator D2 = D;
		       while((D2->CompareOp & Dep::Or) != 0)
			 ++D2;

		       isPreviouslySatisfiedImportantDep =
			 (((*this)[D2] & DepGNow) != 0);
		     }

		   found=true;
		 }
	     }
	    // this is a new dep if it was not found to be already
	    // a important dep of the installed pacakge
	    isNewImportantDep = !found;
	 }
      }
      if(isNewImportantDep)
	 if(DebugAutoInstall == true)
	    std::clog << OutputInDepth(Depth) << "new important dependency: "
		      << Start.TargetPkg().Name() << std::endl;
      if(isPreviouslySatisfiedImportantDep)
	if(DebugAutoInstall == true)
	  std::clog << OutputInDepth(Depth) << "previously satisfied important dependency on "
		    << Start.TargetPkg().Name() << std::endl;

      // skip important deps if the package is already installed
      if (Pkg->CurrentVer != 0 && Start.IsCritical() == false 
	  && !isNewImportantDep && !isPreviouslySatisfiedImportantDep
	  && !ForceImportantDeps)
	 continue;
      
      /* If we are in an or group locate the first or that can 
         succeed. We have already cached this.. */
      for (; Ors > 1 && (DepState[Start->ID] & DepCVer) != DepCVer; Ors--)
	 Start++;

      /* This bit is for processing the possibilty of an install/upgrade
         fixing the problem */
      SPtrArray<Version *> List = Start.AllTargets();
      if (Start->Type != Dep::DpkgBreaks &&
	  (DepState[Start->ID] & DepCVer) == DepCVer)
      {
	 // Right, find the best version to install..
	 Version **Cur = List;
	 PkgIterator P = Start.TargetPkg();
	 PkgIterator InstPkg(*Cache,0);
	 
	 // See if there are direct matches (at the start of the list)
	 for (; *Cur != 0 && (*Cur)->ParentPkg == P.Index(); Cur++)
	 {
	    PkgIterator Pkg(*Cache,Cache->PkgP + (*Cur)->ParentPkg);
	    if (PkgState[Pkg->ID].CandidateVer != *Cur)
	       continue;
	    InstPkg = Pkg;
	    break;
	 }

	 // Select the highest priority providing package
	 if (InstPkg.end() == true)
	 {
	    pkgPrioSortList(*Cache,Cur);
	    for (; *Cur != 0; Cur++)
	    {
	       PkgIterator Pkg(*Cache,Cache->PkgP + (*Cur)->ParentPkg);
	       if (PkgState[Pkg->ID].CandidateVer != *Cur)
		  continue;
	       InstPkg = Pkg;
	       break;
	    }
	 }
	 
	 if (InstPkg.end() == false)
	 {
	    if(DebugAutoInstall == true)
	       std::clog << OutputInDepth(Depth) << "Installing " << InstPkg.Name()
			 << " as " << Start.DepType() << " of " << Pkg.Name()
			 << std::endl;
 	    // now check if we should consider it a automatic dependency or not
 	    if(Pkg.Section() && ConfigValueInSubTree("APT::Never-MarkAuto-Sections", Pkg.Section()))
 	    {
	       if(DebugAutoInstall == true)
		  std::clog << OutputInDepth(Depth) << "Setting NOT as auto-installed (direct "
                            << Start.DepType() << " of pkg in APT::Never-MarkAuto-Sections)" << std::endl;
 	       MarkInstall(InstPkg,true,Depth + 1, true);
 	    }
 	    else 
 	    {
 	       // mark automatic dependency
 	       MarkInstall(InstPkg,true,Depth + 1, false, ForceImportantDeps);
 	       // Set the autoflag, after MarkInstall because MarkInstall unsets it
 	       if (P->CurrentVer == 0)
 		  PkgState[InstPkg->ID].Flags |= Flag::Auto;
 	    }
	 }
	 continue;
      }

      /* For conflicts we just de-install the package and mark as auto,
         Conflicts may not have or groups.  For dpkg's Breaks we try to
         upgrade the package. */
      if (Start->Type == Dep::Conflicts || Start->Type == Dep::Obsoletes ||
	  Start->Type == Dep::DpkgBreaks)
      {
	 for (Version **I = List; *I != 0; I++)
	 {
	    VerIterator Ver(*this,*I);
	    PkgIterator Pkg = Ver.ParentPkg();

	    /* The List includes all packages providing this dependency,
	       even providers which are not installed, so skip them. */
	    if (PkgState[Pkg->ID].InstallVer == 0)
	       continue;

	    if (PkgState[Pkg->ID].CandidateVer != *I &&
	        Start->Type == Dep::DpkgBreaks)
	       MarkInstall(Pkg,true,Depth + 1, false, ForceImportantDeps);
	    else
	       MarkDelete(Pkg,false,Depth + 1, false);
	 }
	 continue;
      }      
   }
}
									/*}}}*/
// DepCache::IsInstallOk - check if it is ok to install this package	/*{{{*/
// ---------------------------------------------------------------------
/* The default implementation just honors dpkg hold
   But an application using this library can override this method
   to control the MarkInstall behaviour */
bool pkgDepCache::IsInstallOk(PkgIterator const &Pkg,bool AutoInst,
			      unsigned long Depth, bool FromUser)
{
   if (FromUser == false && Pkg->SelectedState == pkgCache::State::Hold && _config->FindB("APT::Ignore-Hold",false) == false)
   {
      if (DebugMarker == true)
	 std::clog << OutputInDepth(Depth) << "Hold prevents MarkInstall of " << Pkg << " FU=" << FromUser << std::endl;
      return false;
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

   ActionGroup group(*this);

   RemoveSizes(Pkg);
   RemoveStates(Pkg);
   
   StateCache &P = PkgState[Pkg->ID];
   if (To == true)
      P.iFlags |= ReInstall;
   else
      P.iFlags &= ~ReInstall;
   
   AddStates(Pkg);
   AddSizes(Pkg);

   if (unlikely(Pkg.CurrentVer().end() == true))
      return;

   SetReInstall(Pkg.Group().FindPkg("all"), To);
}
									/*}}}*/
// DepCache::SetCandidateVersion - Change the candidate version		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::SetCandidateVersion(VerIterator TargetVer, bool const &Pseudo)
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
	 if (VS().CheckDep(P.CandVersion(), Start->CompareOp, Start.TargetVer()) == true &&
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
	 if (V.end() == true || VS().CheckDep(V.VerStr(), D->CompareOp, D.TargetVer()) == false)
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
	    Changed.push_back(make_pair(oldCand, TargetVer));
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
// Policy::GetCandidateVer - Returns the Candidate install version	/*{{{*/
// ---------------------------------------------------------------------
/* The default just returns the highest available version that is not
   a source and automatic. */
pkgCache::VerIterator pkgDepCache::Policy::GetCandidateVer(PkgIterator const &Pkg)
{
   /* Not source/not automatic versions cannot be a candidate version 
      unless they are already installed */
   VerIterator Last(*(pkgCache *)this,0);
   
   for (VerIterator I = Pkg.VersionList(); I.end() == false; I++)
   {
      if (Pkg.CurrentVer() == I)
	 return I;
      
      for (VerFileIterator J = I.FileList(); J.end() == false; J++)
      {
	 if ((J.File()->Flags & Flag::NotSource) != 0)
	    continue;

	 /* Stash the highest version of a not-automatic source, we use it
	    if there is nothing better */
	 if ((J.File()->Flags & Flag::NotAutomatic) != 0 ||
	     (J.File()->Flags & Flag::ButAutomaticUpgrades) != 0)
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
bool pkgDepCache::Policy::IsImportantDep(DepIterator const &Dep)
{
   if(Dep.IsCritical())
      return true;
   else if(Dep->Type == pkgCache::Dep::Recommends) 
   {
      if ( _config->FindB("APT::Install-Recommends", false))
	 return true;
      // we suport a special mode to only install-recommends for certain
      // sections
      // FIXME: this is a meant as a temporarly solution until the 
      //        recommends are cleaned up
      const char *sec = Dep.ParentVer().Section();
      if (sec && ConfigValueInSubTree("APT::Install-Recommends-Sections", sec))
	 return true;
   }
   else if(Dep->Type == pkgCache::Dep::Suggests)
     return _config->FindB("APT::Install-Suggests", false);

   return false;
}
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
  return _config->FindB("APT::AutoRemove::SuggestsImportant", false);
}

// pkgDepCache::MarkRequired - the main mark algorithm			/*{{{*/
bool pkgDepCache::MarkRequired(InRootSetFunc &userFunc)
{
   bool follow_recommends;
   bool follow_suggests;
   bool debug_autoremove = _config->FindB("Debug::pkgAutoRemove",false);

   // init the states
   for(PkgIterator p = PkgBegin(); !p.end(); ++p)
   {
      PkgState[p->ID].Marked  = false;
      PkgState[p->ID].Garbage = false;

      // debug output
      if(debug_autoremove && PkgState[p->ID].Flags & Flag::Auto)
  	 std::clog << "AutoDep: " << p.FullName() << std::endl;
   }

   // init vars
   follow_recommends = MarkFollowsRecommends();
   follow_suggests   = MarkFollowsSuggests();



   // do the mark part, this is the core bit of the algorithm
   for(PkgIterator p = PkgBegin(); !p.end(); ++p)
   {
      if(!(PkgState[p->ID].Flags & Flag::Auto) ||
	  (p->Flags & Flag::Essential) ||
	  userFunc.InRootSet(p) ||
	  // be nice even then a required package violates the policy (#583517)
	  // and do the full mark process also for required packages
	  (p.CurrentVer().end() != true &&
	   p.CurrentVer()->Priority == pkgCache::State::Required))
      {
	 // the package is installed (and set to keep)
	 if(PkgState[p->ID].Keep() && !p.CurrentVer().end())
 	    MarkPackage(p, p.CurrentVer(),
			follow_recommends, follow_suggests);
	 // the package is to be installed 
	 else if(PkgState[p->ID].Install())
	    MarkPackage(p, PkgState[p->ID].InstVerIter(*this),
			follow_recommends, follow_suggests);
      }
   }

   return true;
}
									/*}}}*/
// MarkPackage - mark a single package in Mark-and-Sweep		/*{{{*/
void pkgDepCache::MarkPackage(const pkgCache::PkgIterator &pkg,
			      const pkgCache::VerIterator &ver,
			      bool const &follow_recommends,
			      bool const &follow_suggests)
{
   pkgDepCache::StateCache &state = PkgState[pkg->ID];

   // if we are marked already we are done
   if(state.Marked)
      return;

   VerIterator const currver = pkg.CurrentVer();
   VerIterator const instver = state.InstVerIter(*this);

#if 0
   VerIterator const candver = state.CandidateVerIter(*this);

   // If a package was garbage-collected but is now being marked, we
   // should re-select it 
   // For cases when a pkg is set to upgrade and this trigger the
   // removal of a no-longer used dependency.  if the pkg is set to
   // keep again later it will result in broken deps
   if(state.Delete() && state.RemoveReason = Unused) 
   {
      if(ver==candver)
	 mark_install(pkg, false, false, NULL);
      else if(ver==pkg.CurrentVer())
	 MarkKeep(pkg, false, false);
      
      instver=state.InstVerIter(*this);
   }
#endif

   // For packages that are not going to be removed, ignore versions
   // other than the InstVer.  For packages that are going to be
   // removed, ignore versions other than the current version.
   if(!(ver == instver && !instver.end()) &&
      !(ver == currver && instver.end() && !ver.end()))
      return;

   bool const debug_autoremove = _config->FindB("Debug::pkgAutoRemove", false);

   if(debug_autoremove)
     {
       std::clog << "Marking: " << pkg.FullName();
       if(!ver.end())
	 std::clog << " " << ver.VerStr();
       if(!currver.end())
	 std::clog << ", Curr=" << currver.VerStr();
       if(!instver.end())
	 std::clog << ", Inst=" << instver.VerStr();
       std::clog << std::endl;
     }

   state.Marked=true;

   if(ver.end() == true)
      return;

     for(DepIterator d = ver.DependsList(); !d.end(); ++d)
     {
	if(d->Type == Dep::Depends ||
	   d->Type == Dep::PreDepends ||
	   (follow_recommends &&
	    d->Type == Dep::Recommends) ||
	   (follow_suggests &&
	    d->Type == Dep::Suggests))
        {
	   // Try all versions of this package.
	   for(VerIterator V = d.TargetPkg().VersionList(); 
	       !V.end(); ++V)
	   {
	      if(_system->VS->CheckDep(V.VerStr(), d->CompareOp, d.TargetVer()))
	      {
		if(debug_autoremove)
		  {
		    std::clog << "Following dep: " << d.ParentPkg().FullName()
			      << " " << d.ParentVer().VerStr() << " "
			      << d.DepType() << " " << d.TargetPkg().FullName();
		    if((d->CompareOp & ~pkgCache::Dep::Or) != pkgCache::Dep::NoOp)
		      {
			std::clog << " (" << d.CompType() << " "
				  << d.TargetVer() << ")";
		      }
		    std::clog << std::endl;
		  }
		 MarkPackage(V.ParentPkg(), V,
			     follow_recommends, follow_suggests);
	      }
	   }
	   // Now try virtual packages
	   for(PrvIterator prv=d.TargetPkg().ProvidesList(); 
	       !prv.end(); ++prv)
	   {
	      if(_system->VS->CheckDep(prv.ProvideVersion(), d->CompareOp, 
				       d.TargetVer()))
	      {
		if(debug_autoremove)
		  {
		    std::clog << "Following dep: " << d.ParentPkg().FullName() << " "
			      << d.ParentVer().VerStr() << " "
			      << d.DepType() << " " << d.TargetPkg().FullName() << " ";
		    if((d->CompareOp & ~pkgCache::Dep::Or) != pkgCache::Dep::NoOp)
		      {
			std::clog << " (" << d.CompType() << " "
				  << d.TargetVer() << ")";
		      }
		    std::clog << ", provided by "
			      << prv.OwnerPkg().FullName() << " "
			      << prv.OwnerVer().VerStr()
			      << std::endl;
		  }

		 MarkPackage(prv.OwnerPkg(), prv.OwnerVer(),
			     follow_recommends, follow_suggests);
	      }
	   }
	}
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

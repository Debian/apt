// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Algorithms - A set of misc algorithms

   The pkgProblemResolver class has become insanely complex and
   very sophisticated, it handles every test case I have thrown at it
   to my satisfaction. Understanding exactly why all the steps the class
   does are required is difficult and changing though not very risky
   may result in other cases not working.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/dpkgpm.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/string_view.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#include <apt-pkg/prettyprinters.h>

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <string.h>
#include <sys/utsname.h>

#include <apti18n.h>
									/*}}}*/
using namespace std;

class APT_HIDDEN pkgSimulatePrivate
{
public:
   std::vector<pkgDPkgPM::Item> List;
};
// Simulate::Simulate - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The legacy translations here of input Pkg iterators is obsolete, 
   this is not necessary since the pkgCaches are fully shared now. */
pkgSimulate::pkgSimulate(pkgDepCache *Cache) : pkgPackageManager(Cache),
		            d(new pkgSimulatePrivate()), iPolicy(Cache),
			    Sim(&Cache->GetCache(),&iPolicy),
			    group(Sim)
{
   Sim.Init(0);
   auto PackageCount = Cache->Head().PackageCount;
   Flags = new unsigned char[PackageCount];
   memset(Flags,0,sizeof(*Flags)*PackageCount);

   // Fake a filename so as not to activate the media swapping
   string Jnk = "SIMULATE";
   for (decltype(PackageCount) I = 0; I != PackageCount; ++I)
      FileNames[I] = Jnk;

   Cache->CheckConsistency("simulate");
}
									/*}}}*/
// Simulate::~Simulate - Destructor					/*{{{*/
pkgSimulate::~pkgSimulate()
{
   delete[] Flags;
   delete d;
}
									/*}}}*/
// Simulate::Describe - Describe a package				/*{{{*/
// ---------------------------------------------------------------------
/* Parameter Current == true displays the current package version,
   Parameter Candidate == true displays the candidate package version */
void pkgSimulate::Describe(PkgIterator Pkg,ostream &out,bool Current,bool Candidate)
{
   VerIterator Ver(Sim);
 
   out << Pkg.FullName(true);

   if (Current == true)
   {
      Ver = Pkg.CurrentVer();
      if (Ver.end() == false)
         out << " [" << Ver.VerStr() << ']';
   }

   if (Candidate == true)
   {
      Ver = Sim[Pkg].CandidateVerIter(Sim);
      if (Ver.end() == true)
         return;
   
      out << " (" << Ver.VerStr() << ' ' << Ver.RelStr() << ')';
   }
}
									/*}}}*/
// Simulate::Install - Simulate unpacking of a package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSimulate::Install(PkgIterator iPkg,string File)
{
   if (iPkg.end() || File.empty())
      return false;
   d->List.emplace_back(pkgDPkgPM::Item::Install, iPkg, File);
   return true;
}
bool pkgSimulate::RealInstall(PkgIterator iPkg,string /*File*/)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name(), iPkg.Arch());
   Flags[Pkg->ID] = 1;
   
   cout << "Inst ";
   Describe(Pkg,cout,true,true);
   Sim.MarkInstall(Pkg,false);

   // Look for broken conflicts+predepends.
   for (PkgIterator I = Sim.PkgBegin(); I.end() == false; ++I)
   {
      if (Sim[I].InstallVer == 0)
	 continue;
      
      for (DepIterator D = Sim[I].InstVerIter(Sim).DependsList(); D.end() == false;)
      {
	 DepIterator Start;
	 DepIterator End;
	 D.GlobOr(Start,End);
	 if (Start.IsNegative() == true ||
	     End->Type == pkgCache::Dep::PreDepends)
         {
	    if ((Sim[End] & pkgDepCache::DepGInstall) == 0)
	    {
	       cout << " [" << I.FullName(false) << " on " << Start.TargetPkg().FullName(false) << ']';
	       if (Start->Type == pkgCache::Dep::Conflicts)
		  _error->Error("Fatal, conflicts violated %s",I.FullName(false).c_str());
	    }	    
	 }
      }      
   }

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;
   return true;
}
									/*}}}*/
// Simulate::Configure - Simulate configuration of a Package		/*{{{*/
// ---------------------------------------------------------------------
/* This is not an accurate simulation of relatity, we should really not
   install the package.. For some investigations it may be necessary 
   however. */
bool pkgSimulate::Configure(PkgIterator iPkg)
{
   if (iPkg.end())
      return false;
   d->List.emplace_back(pkgDPkgPM::Item::Configure, iPkg);
   return true;
}
bool pkgSimulate::RealConfigure(PkgIterator iPkg)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name(), iPkg.Arch());
   
   Flags[Pkg->ID] = 2;

   if (Sim[Pkg].InstBroken() == true)
   {
      cout << "Conf " << Pkg.FullName(false) << " broken" << endl;

      Sim.Update();
      
      // Print out each package and the failed dependencies
      for (pkgCache::DepIterator D = Sim[Pkg].InstVerIter(Sim).DependsList(); D.end() == false; ++D)
      {
	 if (Sim.IsImportantDep(D) == false || 
	     (Sim[D] & pkgDepCache::DepInstall) != 0)
	    continue;
	 
	 if (D->Type == pkgCache::Dep::Obsoletes)
	    cout << " Obsoletes:" << D.TargetPkg().FullName(false);
	 else if (D->Type == pkgCache::Dep::Conflicts)
	    cout << " Conflicts:" << D.TargetPkg().FullName(false);
	 else if (D->Type == pkgCache::Dep::DpkgBreaks)
	    cout << " Breaks:" << D.TargetPkg().FullName(false);
	 else
	    cout << " Depends:" << D.TargetPkg().FullName(false);
      }	    
      cout << endl;

      _error->Error("Conf Broken %s",Pkg.FullName(false).c_str());
   }   
   else
   {
      cout << "Conf "; 
      Describe(Pkg,cout,false,true);
   }

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;
   
   return true;
}
									/*}}}*/
// Simulate::Remove - Simulate the removal of a package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSimulate::Remove(PkgIterator iPkg,bool Purge)
{
   if (iPkg.end())
      return false;
   d->List.emplace_back(Purge ? pkgDPkgPM::Item::Purge : pkgDPkgPM::Item::Remove, iPkg);
   return true;
}
bool pkgSimulate::RealRemove(PkgIterator iPkg,bool Purge)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name(), iPkg.Arch());
   if (Pkg.end() == true)
   {
      std::cerr << (Purge ? "Purg" : "Remv") << " invalid package " << iPkg.FullName() << std::endl;
      return false;
   }

   Flags[Pkg->ID] = 3;
   Sim.MarkDelete(Pkg);

   if (Purge == true)
      cout << "Purg ";
   else
      cout << "Remv ";
   Describe(Pkg,cout,true,false);

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;

   return true;
}
									/*}}}*/
// Simulate::ShortBreaks - Print out a short line describing all breaks	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgSimulate::ShortBreaks()
{
   cout << " [";
   for (PkgIterator I = Sim.PkgBegin(); I.end() == false; ++I)
   {
      if (Sim[I].InstBroken() == true)
      {
	 if (Flags[I->ID] == 0)
	    cout << I.FullName(false) << ' ';
/*	 else
	    cout << I.Name() << "! ";*/
      }      
   }
   cout << ']' << endl;
}
									/*}}}*/
bool pkgSimulate::Go(APT::Progress::PackageManager *)			/*{{{*/
{
   if (pkgDPkgPM::ExpandPendingCalls(d->List, Cache) == false)
      return false;
   for (auto && I : d->List)
      switch (I.Op)
      {
	 case pkgDPkgPM::Item::Install:
	    if (RealInstall(I.Pkg, I.File) == false)
	       return false;
	    break;
	 case pkgDPkgPM::Item::Configure:
	    if (RealConfigure(I.Pkg) == false)
	       return false;
	    break;
	 case pkgDPkgPM::Item::Remove:
	    if (RealRemove(I.Pkg, false) == false)
	       return false;
	    break;
	 case pkgDPkgPM::Item::Purge:
	    if (RealRemove(I.Pkg, true) == false)
	       return false;
	    break;
	 case pkgDPkgPM::Item::ConfigurePending:
	 case pkgDPkgPM::Item::TriggersPending:
	 case pkgDPkgPM::Item::RemovePending:
	 case pkgDPkgPM::Item::PurgePending:
	    return _error->Error("Internal error, simulation encountered unexpected pending item");
      }
   return true;
}
									/*}}}*/
// ApplyStatus - Adjust for non-ok packages				/*{{{*/
// ---------------------------------------------------------------------
/* We attempt to change the state of the all packages that have failed
   installation toward their real state. The ordering code will perform 
   the necessary calculations to deal with the problems. */
bool pkgApplyStatus(pkgDepCache &Cache)
{
   pkgDepCache::ActionGroup group(Cache);

   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (I->VersionList == 0)
	 continue;
	 
      // Only choice for a ReInstReq package is to reinstall
      if (I->InstState == pkgCache::State::ReInstReq ||
	  I->InstState == pkgCache::State::HoldReInstReq)
      {
	 if (I->CurrentVer != 0 && I.CurrentVer().Downloadable() == true)
	    Cache.MarkKeep(I, false, false);
	 else
	 {
	    // Is this right? Will dpkg choke on an upgrade?
	    if (Cache[I].CandidateVer != 0 &&
		 Cache[I].CandidateVerIter(Cache).Downloadable() == true)
	       Cache.MarkInstall(I, false, 0, false);
	    else
	       return _error->Error(_("The package %s needs to be reinstalled, "
				    "but I can't find an archive for it."),I.FullName(true).c_str());
	 }
	 
	 continue;
      }
      
      switch (I->CurrentState)
      {
	 /* This means installation failed somehow - it does not need to be
	    re-unpacked (probably) */
	 case pkgCache::State::UnPacked:
	 case pkgCache::State::HalfConfigured:
	 case pkgCache::State::TriggersAwaited:
	 case pkgCache::State::TriggersPending:
	 if ((I->CurrentVer != 0 && I.CurrentVer().Downloadable() == true) ||
	     I.State() != pkgCache::PkgIterator::NeedsUnpack)
	    Cache.MarkKeep(I, false, false);
	 else
	 {
	    if (Cache[I].CandidateVer != 0 &&
		 Cache[I].CandidateVerIter(Cache).Downloadable() == true)
	       Cache.MarkInstall(I, true, 0, false);
	    else
	       Cache.MarkDelete(I, false, 0, false);
	 }
	 break;

	 // This means removal failed
	 case pkgCache::State::HalfInstalled:
	 Cache.MarkDelete(I, false, 0, false);
	 break;
	 
	 default:
	 if (I->InstState != pkgCache::State::Ok)
	    return _error->Error("The package %s is not ok and I "
				 "don't know how to fix it!",I.FullName(false).c_str());
      }
   }
   return true;
}
									/*}}}*/
// FixBroken - Fix broken packages					/*{{{*/
// ---------------------------------------------------------------------
/* This autoinstalls every broken package and then runs the problem resolver
   on the result. */
bool pkgFixBroken(pkgDepCache &Cache)
{
   pkgDepCache::ActionGroup group(Cache);

   // Auto upgrade all broken packages
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      if (Cache[I].NowBroken() == true)
	 Cache.MarkInstall(I, true, 0, false);
   
   /* Fix packages that are in a NeedArchive state but don't have a
      downloadable install version */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (I.State() != pkgCache::PkgIterator::NeedsUnpack ||
	  Cache[I].Delete() == true)
	 continue;
      
      if (Cache[I].InstVerIter(Cache).Downloadable() == false)
	 continue;

      Cache.MarkInstall(I, true, 0, false);
   }
   
   pkgProblemResolver Fix(&Cache);
   return Fix.Resolve(true);
}
									/*}}}*/
// ProblemResolver::pkgProblemResolver - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgProblemResolver::pkgProblemResolver(pkgDepCache *pCache) : d(NULL), Cache(*pCache)
{
   // Allocate memory
   auto const Size = Cache.Head().PackageCount;
   Scores = new int[Size];
   Flags = new unsigned char[Size];
   memset(Flags,0,sizeof(*Flags)*Size);
   
   // Set debug to true to see its decision logic
   Debug = _config->FindB("Debug::pkgProblemResolver",false);
}
									/*}}}*/
// ProblemResolver::~pkgProblemResolver - Destructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgProblemResolver::~pkgProblemResolver()
{
   delete [] Scores;
   delete [] Flags;
}
									/*}}}*/
// ProblemResolver::ScoreSort - Sort the list by score			/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgProblemResolver::ScoreSort(Package const *A,Package const *B)
{
   if (Scores[A->ID] > Scores[B->ID])
      return -1;
   if (Scores[A->ID] < Scores[B->ID])
      return 1;
   return 0;
}
									/*}}}*/
// ProblemResolver::MakeScores - Make the score table			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgProblemResolver::MakeScores()
{
   auto const Size = Cache.Head().PackageCount;
   memset(Scores,0,sizeof(*Scores)*Size);

   // maps to pkgCache::State::VerPriority: 
   //    Required Important Standard Optional Extra
   int PrioMap[] = {
      0,
      _config->FindI("pkgProblemResolver::Scores::Required",3),
      _config->FindI("pkgProblemResolver::Scores::Important",2),
      _config->FindI("pkgProblemResolver::Scores::Standard",1),
      _config->FindI("pkgProblemResolver::Scores::Optional",-1),
      _config->FindI("pkgProblemResolver::Scores::Extra",-2)
   };
   int PrioEssentials = _config->FindI("pkgProblemResolver::Scores::Essentials",100);
   int PrioInstalledAndNotObsolete = _config->FindI("pkgProblemResolver::Scores::NotObsolete",1);
   int DepMap[] = {
      0,
      _config->FindI("pkgProblemResolver::Scores::Depends",1),
      _config->FindI("pkgProblemResolver::Scores::PreDepends",1),
      _config->FindI("pkgProblemResolver::Scores::Suggests",0),
      _config->FindI("pkgProblemResolver::Scores::Recommends",1),
      _config->FindI("pkgProblemResolver::Scores::Conflicts",-1),
      _config->FindI("pkgProblemResolver::Scores::Replaces",0),
      _config->FindI("pkgProblemResolver::Scores::Obsoletes",0),
      _config->FindI("pkgProblemResolver::Scores::Breaks",-1),
      _config->FindI("pkgProblemResolver::Scores::Enhances",0)
   };
   int AddProtected = _config->FindI("pkgProblemResolver::Scores::AddProtected",10000);
   int AddEssential = _config->FindI("pkgProblemResolver::Scores::AddEssential",5000);

   if (_config->FindB("Debug::pkgProblemResolver::ShowScores",false) == true)
      clog << "Settings used to calculate pkgProblemResolver::Scores::" << endl
         << "  Required => " << PrioMap[pkgCache::State::Required] << endl
         << "  Important => " << PrioMap[pkgCache::State::Important] << endl
         << "  Standard => " << PrioMap[pkgCache::State::Standard] << endl
         << "  Optional => " << PrioMap[pkgCache::State::Optional] << endl
         << "  Extra => " << PrioMap[pkgCache::State::Extra] << endl
         << "  Essentials => " << PrioEssentials << endl
         << "  InstalledAndNotObsolete => " << PrioInstalledAndNotObsolete << endl
         << "  Pre-Depends => " << DepMap[pkgCache::Dep::PreDepends] << endl
         << "  Depends => " << DepMap[pkgCache::Dep::Depends] << endl
         << "  Recommends => " << DepMap[pkgCache::Dep::Recommends] << endl
         << "  Suggests => " << DepMap[pkgCache::Dep::Suggests] << endl
         << "  Conflicts => " << DepMap[pkgCache::Dep::Conflicts] << endl
         << "  Breaks => " << DepMap[pkgCache::Dep::DpkgBreaks] << endl
         << "  Replaces => " << DepMap[pkgCache::Dep::Replaces] << endl
         << "  Obsoletes => " << DepMap[pkgCache::Dep::Obsoletes] << endl
         << "  Enhances => " << DepMap[pkgCache::Dep::Enhances] << endl
         << "  AddProtected => " << AddProtected << endl
         << "  AddEssential => " << AddEssential << endl;

   // Generate the base scores for a package based on its properties
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (Cache[I].InstallVer == 0)
	 continue;
      
      int &Score = Scores[I->ID];
      
      /* This is arbitrary, it should be high enough to elevate an
         essantial package above most other packages but low enough
	 to allow an obsolete essential packages to be removed by
	 a conflicts on a powerful normal package (ie libc6) */
      if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential
	  || (I->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	 Score += PrioEssentials;

      pkgCache::VerIterator const InstVer = Cache[I].InstVerIter(Cache);
      // We apply priorities only to downloadable packages, all others are prio:extra
      // as an obsolete prio:standard package can't be that standard anymore…
      if (InstVer->Priority <= pkgCache::State::Extra && InstVer.Downloadable() == true)
	 Score += PrioMap[InstVer->Priority];
      else
	 Score += PrioMap[pkgCache::State::Extra];

      /* This helps to fix oddball problems with conflicting packages
	 on the same level. We enhance the score of installed packages
	 if those are not obsolete */
      if (I->CurrentVer != 0 && Cache[I].CandidateVer != 0 && Cache[I].CandidateVerIter(Cache).Downloadable())
	 Score += PrioInstalledAndNotObsolete;

      // propagate score points along dependencies
      for (pkgCache::DepIterator D = InstVer.DependsList(); not D.end(); ++D)
      {
	 if (DepMap[D->Type] == 0)
	    continue;
	 pkgCache::PkgIterator const T = D.TargetPkg();
	 if (not D.IsIgnorable(T))
	 {
	    if (D->Version != 0)
	    {
	       pkgCache::VerIterator const IV = Cache[T].InstVerIter(Cache);
	       if (IV.end() || not D.IsSatisfied(IV))
		  continue;
	    }
	    Scores[T->ID] += DepMap[D->Type];
	 }

	 std::vector<map_id_t> providers;
	 for (auto Prv = T.ProvidesList(); not Prv.end(); ++Prv)
	 {
	    if (D.IsIgnorable(Prv))
	       continue;
	    auto const PV = Prv.OwnerVer();
	    auto const PP = PV.ParentPkg();
	    if (PV != Cache[PP].InstVerIter(Cache) || not D.IsSatisfied(Prv))
	       continue;
	    providers.push_back(PP->ID);
	 }
	 std::sort(providers.begin(), providers.end());
	 providers.erase(std::unique(providers.begin(), providers.end()), providers.end());
	 for (auto const prv : providers)
	    Scores[prv] += DepMap[D->Type];
      }
   }

   // Copy the scores to advoid additive looping
   std::unique_ptr<int[]> OldScores(new int[Size]);
   memcpy(OldScores.get(),Scores,sizeof(*Scores)*Size);
      
   /* Now we cause 1 level of dependency inheritance, that is we add the 
      score of the packages that depend on the target Package. This 
      fortifies high scoring packages */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (Cache[I].InstallVer == 0)
	 continue;
      
      for (pkgCache::DepIterator D = I.RevDependsList(); D.end() == false; ++D)
      {
	 // Only do it for the install version
	 if ((pkgCache::Version *)D.ParentVer() != Cache[D.ParentPkg()].InstallVer ||
	     (D->Type != pkgCache::Dep::Depends && 
	      D->Type != pkgCache::Dep::PreDepends &&
	      D->Type != pkgCache::Dep::Recommends))
	    continue;	 
	 
	 // Do not propagate negative scores otherwise
	 // an extra (-2) package might score better than an optional (-1)
	 if (OldScores[D.ParentPkg()->ID] > 0)
	     Scores[I->ID] += OldScores[D.ParentPkg()->ID];
      }      
   }

   /* Now we propagate along provides. This makes the packages that
      provide important packages extremely important */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      auto const transfer = abs(Scores[I->ID] - OldScores[I->ID]);
      if (transfer == 0)
	 continue;

      std::vector<map_id_t> providers;
      for (auto Prv = I.ProvidesList(); not Prv.end(); ++Prv)
      {
	 if (Prv.IsMultiArchImplicit())
	    continue;
	 auto const PV = Prv.OwnerVer();
	 auto const PP = PV.ParentPkg();
	 if (PV != Cache[PP].InstVerIter(Cache))
	    continue;
	 providers.push_back(PP->ID);
      }
      std::sort(providers.begin(), providers.end());
      providers.erase(std::unique(providers.begin(), providers.end()), providers.end());
      for (auto const prv : providers)
	 Scores[prv] += transfer;
   }

   /* Protected things are pushed really high up. This number should put them
      ahead of everything */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if ((Flags[I->ID] & Protected) != 0)
	 Scores[I->ID] += AddProtected;
      if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential ||
          (I->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	 Scores[I->ID] += AddEssential;
   }
}
									/*}}}*/
// ProblemResolver::DoUpgrade - Attempt to upgrade this package		/*{{{*/
// ---------------------------------------------------------------------
/* This goes through and tries to reinstall packages to make this package
   installable */
bool pkgProblemResolver::DoUpgrade(pkgCache::PkgIterator Pkg)
{
   pkgDepCache::ActionGroup group(Cache);

   if ((Flags[Pkg->ID] & Upgradable) == 0 || Cache[Pkg].Upgradable() == false)
      return false;
   if ((Flags[Pkg->ID] & Protected) == Protected)
      return false;
   
   Flags[Pkg->ID] &= ~Upgradable;
   
   bool WasKept = Cache[Pkg].Keep();
   if (not Cache.MarkInstall(Pkg, false, 0, false))
     return false;

   // This must be a virtual package or something like that.
   if (Cache[Pkg].InstVerIter(Cache).end() == true)
      return false;
   
   // Isolate the problem dependency
   bool Fail = false;
   for (pkgCache::DepIterator D = Cache[Pkg].InstVerIter(Cache).DependsList(); D.end() == false;)
   {
      // Compute a single dependency element (glob or)
      pkgCache::DepIterator Start = D;
      pkgCache::DepIterator End = D;
      for (bool LastOR = true; D.end() == false && LastOR == true;)
      {
	 LastOR = (D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
	 ++D;
	 if (LastOR == true)
	    End = D;
      }
      
      // We only worry about critical deps.
      if (End.IsCritical() != true)
	 continue;
            
      // Iterate over all the members in the or group
      while (1)
      {
	 // Dep is ok now
	 if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    break;
	 
	 // Do not change protected packages
	 PkgIterator P = Start.SmartTargetPkg();
	 if (Cache[P].Protect())
	 {
	    if (Debug == true)
	       clog << "    Reinst Failed because of protected " << P.FullName(false) << endl;
	    Fail = true;
	 }      
	 else
	 {
	    // Upgrade the package if the candidate version will fix the problem.
	    if ((Cache[Start] & pkgDepCache::DepCVer) == pkgDepCache::DepCVer)
	    {
	       if (DoUpgrade(P) == false)
	       {
		  if (Debug == true)
		     clog << "    Reinst Failed because of " << P.FullName(false) << endl;
		  Fail = true;
	       }
	       else
	       {
		  Fail = false;
		  break;
	       }	    
	    }
	    else
	    {
	       /* We let the algorithm deal with conflicts on its next iteration,
		it is much smarter than us */
	       if (Start.IsNegative() == true)
		   break;
	       
	       if (Debug == true)
		  clog << "    Reinst Failed early because of " << Start.TargetPkg().FullName(false) << endl;
	       Fail = true;
	    }     
	 }
	 
	 if (Start == End)
	    break;
	 ++Start;
      }
      if (Fail == true)
	 break;
   }
   
   // Undo our operations - it might be smart to undo everything this did..
   if (Fail == true)
   {
      if (WasKept == true)
	 Cache.MarkKeep(Pkg, false, false);
      else
	 Cache.MarkDelete(Pkg, false, 0, false);
      return false;
   }	 
   
   if (Debug == true)
      clog << "  Re-Instated " << Pkg.FullName(false) << endl;
   return true;
}
									/*}}}*/
// ProblemResolver::Resolve - calls a resolver to fix the situation	/*{{{*/
bool pkgProblemResolver::Resolve(bool BrokenFix, OpProgress * const Progress)
{
   std::string const solver = _config->Find("APT::Solver", "internal");
   auto const ret = EDSP::ResolveExternal(solver.c_str(), Cache, 0, Progress);
   if (solver != "internal")
      return ret;
   return ResolveInternal(BrokenFix);
}
									/*}}}*/
// ProblemResolver::ResolveInternal - Run the resolution pass		/*{{{*/
// ---------------------------------------------------------------------
/* This routines works by calculating a score for each package. The score
   is derived by considering the package's priority and all reverse 
   dependents giving an integer that reflects the amount of breakage that
   adjusting the package will inflict. 
      
   It goes from highest score to lowest and corrects all of the breaks by 
   keeping or removing the dependent packages. If that fails then it removes
   the package itself and goes on. The routine should be able to intelligently
   go from any broken state to a fixed state. 
 
   The BrokenFix flag enables a mode where the algorithm tries to 
   upgrade packages to advoid problems. */
bool pkgProblemResolver::ResolveInternal(bool const BrokenFix)
{
   pkgDepCache::ActionGroup group(Cache);

   if (Debug)
      Cache.CheckConsistency("resolve start");

   // Record which packages are marked for install
   bool Again = false;
   do
   {
      Again = false;
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      {
	 if (Cache[I].Install() == true)
	    Flags[I->ID] |= PreInstalled;
	 else
	 {
	    if (Cache[I].InstBroken() == true && BrokenFix == true)
	    {
	       Cache.MarkInstall(I, false, 0, false);
	       if (Cache[I].Install() == true)
		  Again = true;
	    }
	    
	    Flags[I->ID] &= ~PreInstalled;
	 }
	 Flags[I->ID] |= Upgradable;
      }
   }
   while (Again == true);

   if (Debug == true) {
      clog << "Starting pkgProblemResolver with broken count: " 
           << Cache.BrokenCount() << endl;
   }
   
   MakeScores();

   auto const Size = Cache.Head().PackageCount;

   /* We have to order the packages so that the broken fixing pass 
      operates from highest score to lowest. This prevents problems when
      high score packages cause the removal of lower score packages that
      would cause the removal of even lower score packages. */
   std::unique_ptr<pkgCache::Package *[]> PList(new pkgCache::Package *[Size]);
   pkgCache::Package **PEnd = PList.get();
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      *PEnd++ = I;

   std::sort(PList.get(), PEnd, [this](Package *a, Package *b) { return ScoreSort(a, b) < 0; });

   if (_config->FindB("Debug::pkgProblemResolver::ShowScores",false) == true)
   {
      clog << "Show Scores" << endl;
      for (pkgCache::Package **K = PList.get(); K != PEnd; K++)
         if (Scores[(*K)->ID] != 0)
         {
           pkgCache::PkgIterator Pkg(Cache,*K);
           clog << Scores[(*K)->ID] << ' ' << APT::PrettyPkg(&Cache, Pkg) << std::endl;
         }
   }

   if (Debug == true) {
      clog << "Starting 2 pkgProblemResolver with broken count: " 
           << Cache.BrokenCount() << endl;
   }

   /* Now consider all broken packages. For each broken package we either
      remove the package or fix it's problem. We do this once, it should
      not be possible for a loop to form (that is a < b < c and fixing b by
      changing a breaks c) */
   bool Change = true;
   bool const TryFixByInstall = _config->FindB("pkgProblemResolver::FixByInstall", true);
   int const MaxCounter = _config->FindI("pkgProblemResolver::MaxCounter", 20);
   std::vector<PackageKill> KillList;
   for (int Counter = 0; Counter < MaxCounter && Change; ++Counter)
   {
      Change = false;
      for (pkgCache::Package **K = PList.get(); K != PEnd; K++)
      {
	 pkgCache::PkgIterator I(Cache,*K);

	 /* We attempt to install this and see if any breaks result,
	    this takes care of some strange cases */
	 if (Cache[I].CandidateVer != Cache[I].InstallVer &&
	     I->CurrentVer != 0 && Cache[I].InstallVer != 0 &&
	     (Flags[I->ID] & PreInstalled) != 0 &&
	     not Cache[I].Protect() &&
	     (Flags[I->ID] & ReInstateTried) == 0)
	 {
	    if (Debug == true)
	       clog << " Try to Re-Instate (" << Counter << ") " << I.FullName(false) << endl;
	    auto const OldBreaks = Cache.BrokenCount();
	    pkgCache::Version *OldVer = Cache[I].InstallVer;
	    Flags[I->ID] &= ReInstateTried;
	    
	    Cache.MarkInstall(I, false, 0, false);
	    if (Cache[I].InstBroken() == true || 
		OldBreaks < Cache.BrokenCount())
	    {
	       if (OldVer == 0)
		  Cache.MarkDelete(I, false, 0, false);
	       else
		  Cache.MarkKeep(I, false, false);
	    }	    
	    else
	       if (Debug == true)
		  clog << "Re-Instated " << I.FullName(false) << " (" << OldBreaks << " vs " << Cache.BrokenCount() << ')' << endl;
	 }
	    
	 if (Cache[I].InstallVer == 0 || Cache[I].InstBroken() == false)
	    continue;
	 
	 if (Debug == true)
	    clog << "Investigating (" << Counter << ") " << APT::PrettyPkg(&Cache, I) << endl;
	 
	 // Isolate the problem dependency
	 bool InOr = false;
	 pkgCache::DepIterator Start;
	 pkgCache::DepIterator End;
	 size_t OldSize = 0;

	 KillList.clear();
	 
	 enum {OrRemove,OrKeep} OrOp = OrRemove;
	 for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList();
	      D.end() == false || InOr == true;)
	 {
	    // Compute a single dependency element (glob or)
	    if (Start == End)
	    {
	       // Decide what to do
	       if (InOr == true && OldSize == KillList.size())
	       {
		  if (OrOp == OrRemove)
		  {
		     if (not Cache[I].Protect())
		     {
			if (Debug == true)
			   clog << "  Or group remove for " << I.FullName(false) << endl;
			Cache.MarkDelete(I, false, 0, false);
			Change = true;
		     }
		  }
		  else if (OrOp == OrKeep)
		  {
		     if (Debug == true)
			clog << "  Or group keep for " << I.FullName(false) << endl;
		     Cache.MarkKeep(I, false, false);
		     Change = true;
		  }
	       }
	       
	       /* We do an extra loop (as above) to finalize the or group
		  processing */
	       InOr = false;
	       OrOp = OrRemove;
	       D.GlobOr(Start,End);
	       if (Start.end() == true)
		  break;

	       // We only worry about critical deps.
	       if (End.IsCritical() != true)
		  continue;

	       InOr = Start != End;
	       OldSize = KillList.size();
	    }
	    else
            {
	       ++Start;
	       // We only worry about critical deps.
	       if (Start.IsCritical() != true)
                  continue;
            }

	    // Dep is ok
	    if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    {
	       InOr = false;
	       continue;
	    }
	    
	    if (Debug == true)
	       clog << "Broken " << APT::PrettyDep(&Cache, Start) << endl;

	    /* Look across the version list. If there are no possible
	       targets then we keep the package and bail. This is necessary
	       if a package has a dep on another package that can't be found */
	    std::unique_ptr<pkgCache::Version *[]> VList(Start.AllTargets());
	    if (VList[0] == 0 && not Cache[I].Protect() &&
		Start.IsNegative() == false &&
		Cache[I].NowBroken() == false)
	    {	       
	       if (InOr == true)
	       {
		  /* No keep choice because the keep being OK could be the
		     result of another element in the OR group! */
		  continue;
	       }
	       
	       Change = true;
	       Cache.MarkKeep(I, false, false);
	       break;
	    }
	    
	    bool Done = false;
	    for (pkgCache::Version **V = VList.get(); *V != 0; V++)
	    {
	       pkgCache::VerIterator Ver(Cache,*V);
	       pkgCache::PkgIterator Pkg = Ver.ParentPkg();

               /* This is a conflicts, and the version we are looking
                  at is not the currently selected version of the 
                  package, which means it is not necessary to 
                  remove/keep */
               if (Cache[Pkg].InstallVer != Ver && Start.IsNegative() == true)
               {
                  if (Debug) 
                     clog << "  Conflicts//Breaks against version " 
                          << Ver.VerStr() << " for " << Pkg.Name() 
                          << " but that is not InstVer, ignoring"
                          << endl;
                  continue;
               }

	       if (Debug == true)
		  clog << "  Considering " << Pkg.FullName(false) << ' ' << Scores[Pkg->ID] <<
		  " as a solution to " << I.FullName(false) << ' ' << Scores[I->ID] << endl;

	       /* Try to fix the package under consideration rather than
	          fiddle with the VList package */
	       if (Scores[I->ID] <= Scores[Pkg->ID] ||
		   ((Cache[Start] & pkgDepCache::DepNow) == 0 &&
		    End.IsNegative() == false))
	       {
		  // Try a little harder to fix protected packages..
		  if (Cache[I].Protect())
		  {
		     if (DoUpgrade(Pkg) == true)
		     {
			if (Scores[Pkg->ID] > Scores[I->ID])
			   Scores[Pkg->ID] = Scores[I->ID];
			break;
		     }
		     
		     continue;
		  }
		  
		  /* See if a keep will do, unless the package is protected,
		     then installing it will be necessary */
		  bool Installed = Cache[I].Install();
		  Cache.MarkKeep(I, false, false);
		  if (Cache[I].InstBroken() == false)
		  {
		     // Unwind operation will be keep now
		     if (OrOp == OrRemove)
			OrOp = OrKeep;
		     
		     // Restore
		     if (InOr == true && Installed == true)
			Cache.MarkInstall(I, false, 0, false);
		     
		     if (Debug == true)
			clog << "  Holding Back " << I.FullName(false) << " rather than change " << Start.TargetPkg().FullName(false) << endl;
		  }		  
		  else
		  {		     
		     if (BrokenFix == false || DoUpgrade(I) == false)
		     {
			// Consider other options
			if (InOr == false || Cache[I].Garbage == true)
			{
			   if (Debug == true)
			      clog << "  Removing " << I.FullName(false) << " rather than change " << Start.TargetPkg().FullName(false) << endl;
			   Cache.MarkDelete(I, false, 0, false);
			   if (Counter > 1 && Scores[Pkg->ID] > Scores[I->ID])
			      Scores[I->ID] = Scores[Pkg->ID];
			}
			else if (TryFixByInstall == true &&
				 Start.TargetPkg()->CurrentVer == 0 &&
				 Cache[Start.TargetPkg()].Delete() == false &&
				 (Flags[Start.TargetPkg()->ID] & ToRemove) != ToRemove &&
				 Cache.GetCandidateVersion(Start.TargetPkg()).end() == false)
			{
			   /* Before removing or keeping the package with the broken dependency
			      try instead to install the first not previously installed package
			      solving this dependency. This helps every time a previous solver
			      is removed by the resolver because of a conflict or alike but it is
			      dangerous as it could trigger new breaks/conflicts… */
			   if (Debug == true)
			      clog << "  Try Installing " << APT::PrettyPkg(&Cache, Start.TargetPkg()) << " before changing " << I.FullName(false) << std::endl;
			   auto const OldBroken = Cache.BrokenCount();
			   Cache.MarkInstall(Start.TargetPkg(), true, 1, false);
			   // FIXME: we should undo the complete MarkInstall process here
			   if (Cache[Start.TargetPkg()].InstBroken() == true || Cache.BrokenCount() > OldBroken)
			      Cache.MarkDelete(Start.TargetPkg(), false, 1, false);
			}
		     }
		  }
		  		  
		  Change = true;
		  Done = true;
		  break;
	       }
	       else
	       {
		  if (Start->Type == pkgCache::Dep::DpkgBreaks)
		  {
		     // first, try upgradring the package, if that
		     // does not help, the breaks goes onto the
		     // kill list
                     //
		     // FIXME: use DoUpgrade(Pkg) instead?
		     if (Cache[End] & pkgDepCache::DepGCVer)
		     {
			if (Debug)
			   clog << "  Upgrading " << Pkg.FullName(false) << " due to Breaks field in " << I.FullName(false) << endl;
			Cache.MarkInstall(Pkg, false, 0, false);
			continue;
		     }
		  }

		  // Skip adding to the kill list if it is protected
		  if (Cache[Pkg].Protect() && Cache[Pkg].Mode != pkgDepCache::ModeDelete)
		     continue;
		
		  if (Debug == true)
		     clog << "  Added " << Pkg.FullName(false) << " to the remove list" << endl;

		  KillList.push_back({Pkg, End});
		  
		  if (Start.IsNegative() == false)
		     break;
	       }
	    }

	    // Hm, nothing can possibly satisfy this dep. Nuke it.
	    if (VList[0] == 0 &&
		Start.IsNegative() == false &&
		not Cache[I].Protect())
	    {
	       bool Installed = Cache[I].Install();
	       Cache.MarkKeep(I);
	       if (Cache[I].InstBroken() == false)
	       {
		  // Unwind operation will be keep now
		  if (OrOp == OrRemove)
		     OrOp = OrKeep;
		  
		  // Restore
		  if (InOr == true && Installed == true)
		     Cache.MarkInstall(I, false, 0, false);
		  
		  if (Debug == true)
		     clog << "  Holding Back " << I.FullName(false) << " because I can't find " << Start.TargetPkg().FullName(false) << endl;
	       }	       
	       else
	       {
		  if (Debug == true)
		     clog << "  Removing " << I.FullName(false) << " because I can't find " << Start.TargetPkg().FullName(false) << endl;
		  if (InOr == false)
		     Cache.MarkDelete(I, false, 0, false);
	       }

	       Change = true;
	       Done = true;
	    }
	    
	    // Try some more
	    if (InOr == true)
	       continue;
	    
	    if (Done == true)
	       break;
	 }
	 
	 // Apply the kill list now
	 if (Cache[I].InstallVer != 0)
	 {
	    for (auto const &J : KillList)
	    {
	       bool foundSomething = false;
	       if ((Cache[J.Dep] & pkgDepCache::DepGNow) == 0)
	       {
		  if (J.Dep.IsNegative() && Cache.MarkDelete(J.Pkg, false, 0, false))
		  {
		     if (Debug)
			std::clog << "  Fixing " << I.FullName(false) << " via remove of " << J.Pkg.FullName(false) << '\n';
		     foundSomething = true;
		  }
	       }
	       else if (Cache.MarkKeep(J.Pkg, false, false))
	       {
		  if (Debug)
		     std::clog << "  Fixing " << I.FullName(false) << " via keep of " << J.Pkg.FullName(false) << '\n';
		  foundSomething = true;
	       }

	       if (not foundSomething || Counter > 1)
	       {
		  if (Scores[I->ID] > Scores[J.Pkg->ID])
		  {
		     Scores[J.Pkg->ID] = Scores[I->ID];
		     Change = true;
		  }
	       }
	       if (foundSomething)
		  Change = true;
	    }
	 }
      }
   }

   if (Debug == true)
      clog << "Done" << endl;
      
   if (Cache.BrokenCount() != 0)
   {
      // See if this is the result of a hold
      pkgCache::PkgIterator I = Cache.PkgBegin();
      for (;I.end() != true; ++I)
      {
	 if (Cache[I].InstBroken() == false)
	    continue;
	 if (not Cache[I].Protect())
	    return _error->Error(_("Error, pkgProblemResolver::Resolve generated breaks, this may be caused by held packages."));
      }
      return _error->Error(_("Unable to correct problems, you have held broken packages."));
   }
   
   // set the auto-flags (mvo: I'm not sure if we _really_ need this)
   pkgCache::PkgIterator I = Cache.PkgBegin();
   for (;I.end() != true; ++I) {
      if (Cache[I].NewInstall() && !(Flags[I->ID] & PreInstalled)) {
	 if(_config->FindB("Debug::pkgAutoRemove",false)) {
	    std::clog << "Resolve installed new pkg: " << I.FullName(false) 
		      << " (now marking it as auto)" << std::endl;
	 }
	 Cache[I].Flags |= pkgCache::Flag::Auto;
      }
   }

   if (Debug)
      Cache.CheckConsistency("resolve done");

   return true;
}
									/*}}}*/
// ProblemResolver::BreaksInstOrPolicy - Check if the given pkg is broken/*{{{*/
// ---------------------------------------------------------------------
/* This checks if the given package is broken either by a hard dependency
   (InstBroken()) or by introducing a new policy breakage e.g. new
   unsatisfied recommends for a package that was in "policy-good" state

   Note that this is not perfect as it will ignore further breakage
   for already broken policy (recommends)
*/
bool pkgProblemResolver::InstOrNewPolicyBroken(pkgCache::PkgIterator I)
{
   // a broken install is always a problem
   if (Cache[I].InstBroken() == true)
   {
      if (Debug == true)
	 std::clog << "  Dependencies are not satisfied for " << APT::PrettyPkg(&Cache, I) << std::endl;
      return true;
   }

   // a newly broken policy (recommends/suggests) is a problem
   if ((Flags[I->ID] & BrokenPolicyAllowed) == 0 &&
       Cache[I].NowPolicyBroken() == false &&
       Cache[I].InstPolicyBroken() == true)
   {
      if (Debug == true)
	 std::clog << "  Policy breaks with upgrade of " << APT::PrettyPkg(&Cache, I) << std::endl;
      return true;
   }

   return false;
}
									/*}}}*/
// ProblemResolver::ResolveByKeep - Resolve problems using keep		/*{{{*/
// ---------------------------------------------------------------------
/* This is the work horse of the soft upgrade routine. It is very gentle
   in that it does not install or remove any packages. It is assumed that the
   system was non-broken previously. */
bool pkgProblemResolver::ResolveByKeep(OpProgress * const Progress)
{
   std::string const solver = _config->Find("APT::Solver", "internal");
   constexpr auto flags = EDSP::Request::UPGRADE_ALL | EDSP::Request::FORBID_NEW_INSTALL | EDSP::Request::FORBID_REMOVE;
   auto const ret = EDSP::ResolveExternal(solver.c_str(), Cache, flags, Progress);
   if (solver != "internal")
      return ret;
   return ResolveByKeepInternal();
}
									/*}}}*/
// ProblemResolver::ResolveByKeepInternal - Resolve problems using keep	/*{{{*/
// ---------------------------------------------------------------------
/* This is the work horse of the soft upgrade routine. It is very gentle
   in that it does not install or remove any packages. It is assumed that the
   system was non-broken previously. */
bool pkgProblemResolver::ResolveByKeepInternal()
{
   pkgDepCache::ActionGroup group(Cache);

   if (Debug)
      Cache.CheckConsistency("keep start");

   MakeScores();

   /* We have to order the packages so that the broken fixing pass 
      operates from highest score to lowest. This prevents problems when
      high score packages cause the removal of lower score packages that
      would cause the removal of even lower score packages. */
   auto Size = Cache.Head().PackageCount;
   pkgCache::Package **PList = new pkgCache::Package *[Size];
   pkgCache::Package **PEnd = PList;
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      *PEnd++ = I;

   std::sort(PList,PEnd,[this](Package *a, Package *b) { return ScoreSort(a, b) < 0; });


   if (_config->FindB("Debug::pkgProblemResolver::ShowScores",false) == true)
   {
      clog << "Show Scores" << endl;
      for (pkgCache::Package **K = PList; K != PEnd; K++)
         if (Scores[(*K)->ID] != 0)
         {
           pkgCache::PkgIterator Pkg(Cache,*K);
           clog << Scores[(*K)->ID] << ' ' << APT::PrettyPkg(&Cache, Pkg) << std::endl;
         }
   }

   if (Debug == true)
      clog << "Entering ResolveByKeep" << endl;

   // Consider each broken package 
   pkgCache::Package **LastStop = 0;
   for (pkgCache::Package **K = PList; K != PEnd; K++)
   {
      pkgCache::PkgIterator I(Cache,*K);

      if (Cache[I].InstallVer == 0)
	 continue;

      if (InstOrNewPolicyBroken(I) == false)
         continue;

      /* Keep the package. If this works then great, otherwise we have
	 to be significantly more aggressive and manipulate its dependencies */
      if (not Cache[I].Protect())
      {
	 if (Debug == true)
	    clog << "Keeping package " << I.FullName(false) << endl;
	 Cache.MarkKeep(I, false, false);
	 if (InstOrNewPolicyBroken(I) == false)
	 {
	    K = PList - 1;
	    continue;
	 }
      }
      
      // Isolate the problem dependencies
      for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList(); D.end() == false;)
      {
	 DepIterator Start;
	 DepIterator End;
	 D.GlobOr(Start,End);

	 // We only worry about critical deps.
	 if (End.IsCritical() != true)
	    continue;
	 
	 // Dep is ok
	 if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    continue;

	 /* Hm, the group is broken.. I suppose the best thing to do is to
	    is to try every combination of keep/not-keep for the set, but that's
	    slow, and this never happens, just be conservative and assume the
	    list of ors is in preference and keep till it starts to work. */
	 while (true)
	 {
	    if (Debug == true)
	       clog << "Package " << I.FullName(false) << " " << APT::PrettyDep(&Cache, Start) << endl;

	    // Look at all the possible provides on this package
	    std::unique_ptr<pkgCache::Version *[]> VList(Start.AllTargets());
	    for (pkgCache::Version **V = VList.get(); *V != 0; V++)
	    {
	       pkgCache::VerIterator Ver(Cache,*V);
	       pkgCache::PkgIterator Pkg = Ver.ParentPkg();
	       
	       // It is not keepable
	       if (Cache[Pkg].InstallVer == 0 ||
		   Pkg->CurrentVer == 0)
		  continue;

	       if (not Cache[Pkg].Protect())
	       {
		  if (Debug == true)
		     clog << "  Keeping Package " << Pkg.FullName(false) << " due to " << Start.DepType() << endl;
		  Cache.MarkKeep(Pkg, false, false);
	       }
	       
	       if (InstOrNewPolicyBroken(I) == false)
		  break;
	    }
	    
	    if (InstOrNewPolicyBroken(I) == false)
	       break;

	    if (Start == End)
	       break;
	    ++Start;
	 }
	      
	 if (InstOrNewPolicyBroken(I) == false)
	    break;
      }

      if (InstOrNewPolicyBroken(I) == true)
	 continue;
      
      // Restart again.
      if (K == LastStop) {
          // I is an iterator based off our temporary package list,
          // so copy the name we need before deleting the temporary list
          std::string const LoopingPackage = I.FullName(false);
          delete[] PList;
          return _error->Error("Internal Error, pkgProblemResolver::ResolveByKeep is looping on package %s.", LoopingPackage.c_str());
      }
      LastStop = K;
      K = PList - 1;
   }

   delete[] PList;

   if (Debug)
      Cache.CheckConsistency("keep done");

   return true;
}
									/*}}}*/
// PrioSortList - Sort a list of versions by priority			/*{{{*/
// ---------------------------------------------------------------------
/* This is meant to be used in conjunction with AllTargets to get a list 
   of versions ordered by preference. */

struct PrioComp {
   pkgCache &PrioCache;

   explicit PrioComp(pkgCache &PrioCache) : PrioCache(PrioCache) {
   }

   bool operator() (pkgCache::Version * const &A, pkgCache::Version * const &B) {
      return compare(A, B) < 0;
   }

   int compare(pkgCache::Version * const &A, pkgCache::Version * const &B) {
      pkgCache::VerIterator L(PrioCache,A);
      pkgCache::VerIterator R(PrioCache,B);

      if ((L.ParentPkg()->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential &&
	  (R.ParentPkg()->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential)
	return 1;
      if ((L.ParentPkg()->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential &&
	  (R.ParentPkg()->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	return -1;

      if ((L.ParentPkg()->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important &&
	  (R.ParentPkg()->Flags & pkgCache::Flag::Important) != pkgCache::Flag::Important)
	return 1;
      if ((L.ParentPkg()->Flags & pkgCache::Flag::Important) != pkgCache::Flag::Important &&
	  (R.ParentPkg()->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important)
	return -1;

      if (L->Priority != R->Priority)
	 return R->Priority - L->Priority;
      return strcmp(L.ParentPkg().Name(),R.ParentPkg().Name());
   }
};

void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List)
{
   unsigned long Count = 0;
   for (pkgCache::Version **I = List; *I != 0; I++)
      Count++;
   std::sort(List,List+Count,PrioComp(Cache));
}
									/*}}}*/

namespace APT
{

namespace KernelAutoRemoveHelper
{

// \brief Returns the uname from a kernel package name, or "" for non-kernel packages.
std::string getUname(std::string const &packageName)
{

   static const constexpr char *const prefixes[] = {
      "linux-image-",
      "kfreebsd-image-",
      "gnumach-image-",
   };

   for (auto prefix : prefixes)
   {
      if (likely(not APT::String::Startswith(packageName, prefix)))
	 continue;
      if (unlikely(APT::String::Endswith(packageName, "-dbgsym")))
	 continue;
      if (unlikely(APT::String::Endswith(packageName, "-dbg")))
	 continue;

      auto aUname = packageName.substr(strlen(prefix));

      // aUname must start with [0-9]+\.
      if (aUname.length() < 2)
	 continue;
      if (strchr("0123456789", aUname[0]) == nullptr)
	 continue;
      auto dot = aUname.find_first_not_of("0123456789");
      if (dot == aUname.npos || aUname[dot] != '.')
	 continue;

      return aUname;
   }

   return "";
}
std::string GetProtectedKernelsRegex(pkgCache *cache, bool ReturnRemove)
{
   if (_config->FindB("APT::Protect-Kernels", true) == false)
      return "";

   struct CompareKernel
   {
      pkgCache *cache;
      bool operator()(const std::string &a, const std::string &b) const
      {
	 return cache->VS->CmpVersion(a, b) < 0;
      }
   };
   bool Debug = _config->FindB("Debug::pkgAutoRemove", false);
   // kernel version -> list of unames
   std::map<std::string, std::vector<std::string>, CompareKernel> version2unames(CompareKernel{cache});
   // needs to be initialized to 0s, might not be set up.
   utsname uts{};
   std::string bootedVersion;

   // Get currently booted version, but only when not on reproducible build.
   if (getenv("SOURCE_DATE_EPOCH") == 0)
   {
      if (uname(&uts) != 0)
	 abort();
   }

   auto VirtualKernelPkg = cache->FindPkg("$kernel", "any");
   if (VirtualKernelPkg.end())
      return "";

   for (pkgCache::PrvIterator Prv = VirtualKernelPkg.ProvidesList(); Prv.end() == false; ++Prv)
   {
      auto Pkg = Prv.OwnerPkg();
      if (likely(Pkg->CurrentVer == 0))
	 continue;

      auto pkgUname = APT::KernelAutoRemoveHelper::getUname(Pkg.Name());
      auto pkgVersion = Pkg.CurrentVer().VerStr();

      if (pkgUname.empty())
	 continue;

      if (Debug)
	 std::clog << "Found kernel " << pkgUname << "(" << pkgVersion << ")" << std::endl;

      version2unames[pkgVersion].push_back(pkgUname);

      if (pkgUname == uts.release)
	 bootedVersion = pkgVersion;
   }

   if (version2unames.size() == 0)
      return "";

   auto latest = version2unames.rbegin();
   auto previous = latest;
   ++previous;

   std::set<std::string> keep;

   if (not bootedVersion.empty())
   {
      if (Debug)
	 std::clog << "Keeping booted kernel " << bootedVersion << std::endl;
      keep.insert(bootedVersion);
   }
   if (latest != version2unames.rend())
   {
      if (Debug)
	 std::clog << "Keeping latest kernel " << latest->first << std::endl;
      keep.insert(latest->first);
   }
   if (keep.size() < 2 && previous != version2unames.rend())
   {
      if (Debug)
	 std::clog << "Keeping previous kernel " << previous->first << std::endl;
      keep.insert(previous->first);
   }
   // Escape special characters '.' and '+' in version strings so we can build a regular expression
   auto escapeSpecial = [](std::string input) -> std::string {
      for (size_t pos = 0; (pos = input.find_first_of(".+", pos)) != input.npos; pos += 2) {
	 input.insert(pos, 1, '\\');
      }
      return input;
   };
   std::ostringstream ss;
   for (auto &pattern : _config->FindVector("APT::VersionedKernelPackages"))
   {
      // Legacy compatibility: Always protected the booted uname and last installed uname
      if (*uts.release)
	 ss << "|^" << pattern << "-" << escapeSpecial(uts.release) << "$";
      for (auto const &kernel : version2unames)
      {
	 if (ReturnRemove ? keep.find(kernel.first) == keep.end() : keep.find(kernel.first) != keep.end())
	 {
	    for (auto const &uname : kernel.second)
	       ss << "|^" << pattern << "-" << escapeSpecial(uname) << "$";
	 }
      }
   }

   auto re_with_leading_or = ss.str();

   if (re_with_leading_or.empty())
      return "";

   auto re = re_with_leading_or.substr(1);
   if (Debug)
      std::clog << "Kernel protection regex: " << re << "\n";

   return re;
}

std::unique_ptr<APT::CacheFilter::Matcher> GetProtectedKernelsFilter(pkgCache *cache, bool returnRemove)
{
   auto regex = GetProtectedKernelsRegex(cache, returnRemove);

   if (regex.empty())
      return std::make_unique<APT::CacheFilter::FalseMatcher>();

   return std::make_unique<APT::CacheFilter::PackageNameMatchesRegEx>(regex);
}

} // namespace KernelAutoRemoveHelper
} // namespace APT

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Package Manager - Abstracts the package manager

   More work is needed in the area of transitioning provides, ie exim
   replacing smail. This can cause interesting side effects.

   Other cases involving conflicts+replaces should be tested. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/edsp.h>
#include <apt-pkg/error.h>
#include <apt-pkg/install-progress.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/orderlist.h>
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/prettyprinters.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#include <iostream>
#include <list>
#include <string>
#include <stddef.h>

#include <apti18n.h>
									/*}}}*/
using namespace std;

bool pkgPackageManager::SigINTStop = false;

// PM::PackageManager - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgPackageManager::pkgPackageManager(pkgDepCache *pCache) : Cache(*pCache),
							    List(NULL), Res(Incomplete), d(NULL)
{
   FileNames = new string[Cache.Head().PackageCount];
   Debug = _config->FindB("Debug::pkgPackageManager",false);
   NoImmConfigure = !_config->FindB("APT::Immediate-Configure",true);
   ImmConfigureAll = _config->FindB("APT::Immediate-Configure-All",false);
}
									/*}}}*/
// PM::PackageManager - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgPackageManager::~pkgPackageManager()
{
   delete List;
   delete [] FileNames;
}
									/*}}}*/
// PM::GetArchives - Queue the archives for download			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgPackageManager::GetArchives(pkgAcquire *Owner,pkgSourceList *Sources,
				    pkgRecords *Recs)
{
   if (CreateOrderList() == false)
      return false;
   
   bool const ordering =
	_config->FindB("PackageManager::UnpackAll",true) ?
		List->OrderUnpack() : List->OrderCritical();
   if (ordering == false)
      return _error->Error("Internal ordering error");

   for (pkgOrderList::iterator I = List->begin(); I != List->end(); ++I)
   {
      PkgIterator Pkg(Cache,*I);
      FileNames[Pkg->ID] = string();
      
      // Skip packages to erase
      if (Cache[Pkg].Delete() == true)
	 continue;

      // Skip Packages that need configure only.
      if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure && 
	  Cache[Pkg].Keep() == true)
	 continue;

      // Skip already processed packages
      if (List->IsNow(Pkg) == false)
	 continue;

      new pkgAcqArchive(Owner,Sources,Recs,Cache[Pkg].InstVerIter(Cache),
			FileNames[Pkg->ID]);
   }

   return true;
}
									/*}}}*/
// PM::FixMissing - Keep all missing packages				/*{{{*/
// ---------------------------------------------------------------------
/* This is called to correct the installation when packages could not
   be downloaded. */
bool pkgPackageManager::FixMissing()
{   
   pkgDepCache::ActionGroup group(Cache);
   pkgProblemResolver Resolve(&Cache);
   List->SetFileList(FileNames);

   bool Bad = false;
   for (PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      if (List->IsMissing(I) == false)
	 continue;
   
      // Okay, this file is missing and we need it. Mark it for keep 
      Bad = true;
      Cache.MarkKeep(I, false, false);
   }
 
   // We have to empty the list otherwise it will not have the new changes
   delete List;
   List = 0;
   
   if (Bad == false)
      return true;
   
   // Now downgrade everything that is broken
   return Resolve.ResolveByKeep() == true && Cache.BrokenCount() == 0;   
}
									/*}}}*/
// PM::ImmediateAdd - Add the immediate flag recursively		/*{{{*/
// ---------------------------------------------------------------------
/* This adds the immediate flag to the pkg and recursively to the
   dependencies
 */
void pkgPackageManager::ImmediateAdd(PkgIterator I, bool UseInstallVer, unsigned const int &Depth)
{
   DepIterator D;
   
   if(UseInstallVer)
   {
      if(Cache[I].InstallVer == 0)
	 return;
      D = Cache[I].InstVerIter(Cache).DependsList(); 
   } else {
      if (I->CurrentVer == 0)
	 return;
      D = I.CurrentVer().DependsList(); 
   }

   for ( /* nothing */  ; D.end() == false; ++D)
      if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends)
      {
	 if(!List->IsFlag(D.TargetPkg(), pkgOrderList::Immediate))
	 {
	    if(Debug)
	       clog << OutputInDepth(Depth) << "ImmediateAdd(): Adding Immediate flag to " << APT::PrettyPkg(&Cache, D.TargetPkg()) << " cause of " << D.DepType() << " " << I.FullName() << endl;
	    List->Flag(D.TargetPkg(),pkgOrderList::Immediate);
	    ImmediateAdd(D.TargetPkg(), UseInstallVer, Depth + 1);
	 }
      }
   return;
}
									/*}}}*/
// PM::CreateOrderList - Create the ordering class			/*{{{*/
// ---------------------------------------------------------------------
/* This populates the ordering list with all the packages that are
   going to change. */
bool pkgPackageManager::CreateOrderList()
{
   if (List != 0)
      return true;
   
   delete List;
   List = new pkgOrderList(&Cache);

   if (Debug && ImmConfigureAll) 
      clog << "CreateOrderList(): Adding Immediate flag for all packages because of APT::Immediate-Configure-All" << endl;
   
   // Generate the list of affected packages and sort it
   for (PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
   {
      // Ignore no-version packages
      if (I->VersionList == 0)
	 continue;
      
      // Mark the package and its dependents for immediate configuration
      if ((((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential) &&
	  NoImmConfigure == false) || ImmConfigureAll)
      {
	 if(Debug && !ImmConfigureAll)
	    clog << "CreateOrderList(): Adding Immediate flag for " << I.FullName() << endl;
	 List->Flag(I,pkgOrderList::Immediate);
	 
	 if (!ImmConfigureAll) {
	    // Look for other install packages to make immediate configurea
	    ImmediateAdd(I, true);
	  
	    // And again with the current version.
	    ImmediateAdd(I, false);
	 }
      }
      
      // Not interesting
      if ((Cache[I].Keep() == true || 
	  Cache[I].InstVerIter(Cache) == I.CurrentVer()) && 
	  I.State() == pkgCache::PkgIterator::NeedsNothing &&
	  (Cache[I].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall &&
	  (I.Purge() != false || Cache[I].Mode != pkgDepCache::ModeDelete ||
	   (Cache[I].iFlags & pkgDepCache::Purge) != pkgDepCache::Purge))
	 continue;
      
      // Append it to the list
      List->push_back(I);      
   }
   
   return true;
}
									/*}}}*/
// PM::DepAlwaysTrue - Returns true if this dep is irrelevant		/*{{{*/
// ---------------------------------------------------------------------
/* The restriction on provides is to eliminate the case when provides
   are transitioning between valid states [ie exim to smail] */
bool pkgPackageManager::DepAlwaysTrue(DepIterator D)
{
   if (D.TargetPkg()->ProvidesList != 0)
      return false;
   
   if ((Cache[D] & pkgDepCache::DepInstall) != 0 &&
       (Cache[D] & pkgDepCache::DepNow) != 0)
      return true;
   return false;
}
									/*}}}*/
// PM::CheckRConflicts - Look for reverse conflicts			/*{{{*/
// ---------------------------------------------------------------------
/* This looks over the reverses for a conflicts line that needs early
   removal. */
bool pkgPackageManager::CheckRConflicts(PkgIterator Pkg,DepIterator D,
					const char *Ver)
{
   for (;D.end() == false; ++D)
   {
      if (D->Type != pkgCache::Dep::Conflicts &&
	  D->Type != pkgCache::Dep::Obsoletes)
	 continue;

      // The package hasn't been changed
      if (List->IsNow(Pkg) == false)
	 continue;
      
      // Ignore self conflicts, ignore conflicts from irrelevant versions
      if (D.IsIgnorable(Pkg) || D.ParentVer() != D.ParentPkg().CurrentVer())
	 continue;
      
      if (Cache.VS().CheckDep(Ver,D->CompareOp,D.TargetVer()) == false)
	 continue;

      if (EarlyRemove(D.ParentPkg(), &D) == false)
	 return _error->Error("Reverse conflicts early remove for package '%s' failed",
			      Pkg.FullName().c_str());
   }
   return true;
}
									/*}}}*/
// PM::CheckRBreaks - Look for reverse breaks				/*{{{*/
bool pkgPackageManager::CheckRBreaks(PkgIterator const &Pkg, DepIterator D,
				     const char * const Ver)
{
   for (;D.end() == false; ++D)
   {
      if (D->Type != pkgCache::Dep::DpkgBreaks)
	 continue;

      PkgIterator const DP = D.ParentPkg();
      if (Cache[DP].Delete() == false)
	 continue;

      // Ignore self conflicts, ignore conflicts from irrelevant versions
      if (D.IsIgnorable(Pkg) || D.ParentVer() != DP.CurrentVer())
	 continue;

      if (Cache.VS().CheckDep(Ver, D->CompareOp, D.TargetVer()) == false)
	 continue;

      // no earlyremove() here as user has already agreed to the permanent removal
      if (SmartRemove(DP) == false)
	 return _error->Error("Internal Error, Could not early remove %s (%d)",DP.FullName().c_str(), 4);
   }
   return true;
}
									/*}}}*/
// PM::ConfigureAll - Run the all out configuration			/*{{{*/
// ---------------------------------------------------------------------
/* This configures every package. It is assumed they are all unpacked and
   that the final configuration is valid. This is also used to catch packages
   that have not been configured when using ImmConfigureAll */
bool pkgPackageManager::ConfigureAll()
{
   pkgOrderList OList(&Cache);
   
   // Populate the order list
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); ++I)
      if (List->IsFlag(pkgCache::PkgIterator(Cache,*I),
		       pkgOrderList::UnPacked) == true)
	 OList.push_back(*I);
   
   if (OList.OrderConfigure() == false)
      return false;

   std::string const conf = _config->Find("PackageManager::Configure", "smart");
   bool const ConfigurePkgs = (ImmConfigureAll || conf == "all");

   // Perform the configuring
   for (pkgOrderList::iterator I = OList.begin(); I != OList.end(); ++I)
   {
      PkgIterator Pkg(Cache,*I);
      
      /* Check if the package has been configured, this can happen if SmartConfigure
         calls its self */ 
      if (List->IsFlag(Pkg,pkgOrderList::Configured)) continue;

      if (ConfigurePkgs == true && SmartConfigure(Pkg, 0) == false) {
         if (ImmConfigureAll)
            _error->Error(_("Could not perform immediate configuration on '%s'. "
			"Please see man 5 apt.conf under APT::Immediate-Configure for details. (%d)"),Pkg.FullName().c_str(),1);
         else
            _error->Error("Internal error, packages left unconfigured. %s",Pkg.FullName().c_str());
	 return false;
      }
      
      List->Flag(Pkg,pkgOrderList::Configured,pkgOrderList::States);
   }
   
   return true;
}
									/*}}}*/
// PM::NonLoopingSmart - helper to avoid loops while calling Smart methods /*{{{*/
// -----------------------------------------------------------------------
/* ensures that a loop of the form A depends B, B depends A (and similar)
   is not leading us down into infinite recursion segfault land */
bool pkgPackageManager::NonLoopingSmart(SmartAction const action, pkgCache::PkgIterator &Pkg,
      pkgCache::PkgIterator DepPkg, int const Depth, bool const PkgLoop,
      bool * const Bad, bool * const Changed)
{
   if (PkgLoop == false)
      List->Flag(Pkg,pkgOrderList::Loop);
   bool success = false;
   switch(action)
   {
      case UNPACK_IMMEDIATE: success = SmartUnPack(DepPkg, true, Depth + 1); break;
      case UNPACK: success = SmartUnPack(DepPkg, false, Depth + 1); break;
      case CONFIGURE: success = SmartConfigure(DepPkg, Depth + 1); break;
   }
   if (PkgLoop == false)
      List->RmFlag(Pkg,pkgOrderList::Loop);

   if (success == false)
      return false;

   if (Bad != NULL)
      *Bad = false;
   if (Changed != NULL && List->IsFlag(DepPkg,pkgOrderList::Loop) == false)
      *Changed = true;
   return true;
}
									/*}}}*/
// PM::SmartConfigure - Perform immediate configuration of the pkg	/*{{{*/
// ---------------------------------------------------------------------
/* This function tries to put the system in a state where Pkg can be configured.
   This involves checking each of Pkg's dependencies and unpacking and
   configuring packages where needed. */
bool pkgPackageManager::SmartConfigure(PkgIterator Pkg, int const Depth)
{
   // If this is true, only check and correct and dependencies without the Loop flag
   bool const PkgLoop = List->IsFlag(Pkg,pkgOrderList::Loop);

   if (Debug) {
      VerIterator InstallVer = VerIterator(Cache,Cache[Pkg].InstallVer);
      clog << OutputInDepth(Depth) << "SmartConfigure " << Pkg.FullName() << " (" << InstallVer.VerStr() << ")";
      if (PkgLoop)
        clog << " (Only Correct Dependencies)";
      clog << endl;
   }

   VerIterator const instVer = Cache[Pkg].InstVerIter(Cache);

   /* Because of the ordered list, most dependencies should be unpacked,
      however if there is a loop (A depends on B, B depends on A) this will not
      be the case, so check for dependencies before configuring. */
   bool Bad = false, Changed = false;
   const unsigned int max_loops = _config->FindI("APT::pkgPackageManager::MaxLoopCount", 5000);
   unsigned int i=0;
   std::list<DepIterator> needConfigure;
   do
   {
      // Check each dependency and see if anything needs to be done
      // so that it can be configured
      Changed = false;
      for (DepIterator D = instVer.DependsList(); D.end() == false; )
      {
	 // Compute a single dependency element (glob or)
	 pkgCache::DepIterator Start, End;
	 D.GlobOr(Start,End);

	 if (End->Type != pkgCache::Dep::Depends && End->Type != pkgCache::Dep::PreDepends)
	    continue;
	 Bad = true;

         // the first pass checks if we its all good, i.e. if we have
         // to do anything at all
	 for (DepIterator Cur = Start; true; ++Cur)
	 {
	    std::unique_ptr<Version *[]> VList(Cur.AllTargets());

	    for (Version **I = VList.get(); *I != 0; ++I)
	    {
	       VerIterator Ver(Cache,*I);
	       PkgIterator DepPkg = Ver.ParentPkg();

	       // Check if the current version of the package is available and will satisfy this dependency
	       if (DepPkg.CurrentVer() == Ver && List->IsNow(DepPkg) == true &&
		   List->IsFlag(DepPkg,pkgOrderList::Removed) == false &&
		   DepPkg.State() == PkgIterator::NeedsNothing &&
		   (Cache[DepPkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall)
	       {
		  Bad = false;
		  break;
	       }

	       // Check if the version that is going to be installed will satisfy the dependency
	       if (Cache[DepPkg].InstallVer != *I || List->IsNow(DepPkg) == false)
		  continue;

	       if (PkgLoop == true)
	       {
		  if (Debug)
		     std::clog << OutputInDepth(Depth) << "Package " << APT::PrettyPkg(&Cache, Pkg) << " loops in SmartConfigure";
		  if (List->IsFlag(DepPkg,pkgOrderList::UnPacked))
		     Bad = false;
		  else if (Debug)
		     std::clog << ", but it isn't unpacked yet";
		  if (Debug)
		     std::clog << std::endl;
	       }
	    }

            if (Cur == End || Bad == false)
	       break;
         }

         // this dependency is in a good state, so we can stop
         if (Bad == false)
         {
            if (Debug)
               std::clog << OutputInDepth(Depth) << "Found ok dep " << APT::PrettyPkg(&Cache, Start.TargetPkg()) << std::endl;
            continue;
         }

	 // Check for dependencies that have not been unpacked, 
         // probably due to loops.
	 for (DepIterator Cur = Start; true; ++Cur)
	 {
	    std::unique_ptr<Version *[]> VList(Cur.AllTargets());

	    for (Version **I = VList.get(); *I != 0; ++I)
	    {
	       VerIterator Ver(Cache,*I);
	       PkgIterator DepPkg = Ver.ParentPkg();

	       // Check if the current version of the package is available and will satisfy this dependency
	       if (DepPkg.CurrentVer() == Ver && List->IsNow(DepPkg) == true &&
		   List->IsFlag(DepPkg,pkgOrderList::Removed) == false &&
		   DepPkg.State() == PkgIterator::NeedsNothing &&
		   (Cache[DepPkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall)
                  continue;

	       // Check if the version that is going to be installed will satisfy the dependency
	       if (Cache[DepPkg].InstallVer != *I || List->IsNow(DepPkg) == false)
		  continue;

	       if (PkgLoop == true)
	       {
		  if (Debug)
		     std::clog << OutputInDepth(Depth) << "Package " << APT::PrettyPkg(&Cache, Pkg) << " loops in SmartConfigure";
		  if (List->IsFlag(DepPkg,pkgOrderList::UnPacked))
		     Bad = false;
		  else if (Debug)
		     std::clog << ", but it isn't unpacked yet";
		  if (Debug)
		     std::clog << std::endl;
	       }
	       else
	       {
		  if (Debug)
		     clog << OutputInDepth(Depth) << "Unpacking " << DepPkg.FullName() << " to avoid loop " << APT::PrettyDep(&Cache, Cur) << endl;
		  if (NonLoopingSmart(UNPACK_IMMEDIATE, Pkg, DepPkg, Depth, PkgLoop, &Bad, &Changed) == false)
		     return false;
	       }
               // at this point we either unpacked a Dep or we are in a loop,
               // no need to unpack a second one
	       break;
	    }

	    if (Cur == End || Bad == false)
	       break;
	 }

	 if (Bad == false)
	    continue;

	 needConfigure.push_back(Start);
      }
      if (i++ > max_loops)
         return _error->Error("Internal error: MaxLoopCount reached in SmartUnPack (1) for %s, aborting", Pkg.FullName().c_str());
   } while (Changed == true);

   // now go over anything that needs configuring
   Bad = false, Changed = false, i = 0;
   do
   {
      Changed = false;
      for (std::list<DepIterator>::const_iterator D = needConfigure.begin(); D != needConfigure.end(); ++D)
      {
	 // Compute a single dependency element (glob or) without modifying D
	 pkgCache::DepIterator Start, End;
	 {
	    pkgCache::DepIterator Discard = *D;
	    Discard.GlobOr(Start,End);
	 }

	 if (End->Type != pkgCache::Dep::Depends && End->Type != pkgCache::Dep::PreDepends)
	    continue;
	 Bad = true;

	 // Search for dependencies which are unpacked but aren't configured yet (maybe loops)
	 for (DepIterator Cur = Start; true; ++Cur)
	 {
	    std::unique_ptr<Version *[]> VList(Cur.AllTargets());

	    for (Version **I = VList.get(); *I != 0; ++I)
	    {
	       VerIterator Ver(Cache,*I);
	       PkgIterator DepPkg = Ver.ParentPkg();

	       // Check if the version that is going to be installed will satisfy the dependency
	       if (Cache[DepPkg].InstallVer != *I)
		  continue;

	       if (List->IsFlag(DepPkg,pkgOrderList::UnPacked))
	       {
		  if (List->IsFlag(DepPkg,pkgOrderList::Loop) && PkgLoop)
		  {
		    // This dependency has already been dealt with by another SmartConfigure on Pkg
		    Bad = false;
		    break;
		  }
		  if (Debug)
		     std::clog << OutputInDepth(Depth) << "Configure already unpacked " << APT::PrettyPkg(&Cache, DepPkg) << std::endl;
		  if (NonLoopingSmart(CONFIGURE, Pkg, DepPkg, Depth, PkgLoop, &Bad, &Changed) == false)
		     return false;
		  break;

	       }
	       else if (List->IsFlag(DepPkg,pkgOrderList::Configured))
	       {
		  Bad = false;
		  break;
	       }
	    }
	    if (Cur == End || Bad == false)
	       break;
         }


	 if (Bad == true && Changed == false && Debug == true)
	    std::clog << OutputInDepth(Depth) << "Could not satisfy " << APT::PrettyDep(&Cache, *D) << std::endl;
      }
      if (i++ > max_loops)
         return _error->Error("Internal error: MaxLoopCount reached in SmartUnPack (2) for %s, aborting", Pkg.FullName().c_str());
   } while (Changed == true);

   if (Bad == true)
      return _error->Error(_("Could not configure '%s'. "),Pkg.FullName().c_str());

   // Check for reverse conflicts.
   if (CheckRBreaks(Pkg,Pkg.RevDependsList(), instVer.VerStr()) == false)
      return false;

   for (PrvIterator P = instVer.ProvidesList(); P.end() == false; ++P)
      if (Pkg->Group != P.OwnerPkg()->Group)
	 CheckRBreaks(Pkg,P.ParentPkg().RevDependsList(),P.ProvideVersion());

   if (PkgLoop) return true;

   static std::string const conf = _config->Find("PackageManager::Configure", "smart");
   static bool const ConfigurePkgs = (conf == "all" || conf == "smart");

   if (List->IsFlag(Pkg,pkgOrderList::Configured))
      return _error->Error("Internal configure error on '%s'.", Pkg.FullName().c_str());

   if (ConfigurePkgs == true && Configure(Pkg) == false)
      return false;

   List->Flag(Pkg,pkgOrderList::Configured,pkgOrderList::States);

   if ((Cache[Pkg].InstVerIter(Cache)->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same &&
       not List->IsFlag(Pkg, pkgOrderList::Immediate))
      for (PkgIterator P = Pkg.Group().PackageList();
	   P.end() == false; P = Pkg.Group().NextPkg(P))
      {
	 if (Pkg == P || List->IsFlag(P,pkgOrderList::Configured) == true ||
	     List->IsFlag(P,pkgOrderList::UnPacked) == false ||
	     Cache[P].InstallVer == 0 || (P.CurrentVer() == Cache[P].InstallVer &&
	      (Cache[Pkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall))
	    continue;
	 if (SmartConfigure(P, (Depth +1)) == false)
	    return false;
      }

   // Sanity Check
   if (List->IsFlag(Pkg,pkgOrderList::Configured) == false)
      return _error->Error(_("Could not configure '%s'. "),Pkg.FullName().c_str());

   return true;
}
									/*}}}*/
// PM::EarlyRemove - Perform removal of packages before their time	/*{{{*/
// ---------------------------------------------------------------------
/* This is called to deal with conflicts arising from unpacking */
bool pkgPackageManager::EarlyRemove(PkgIterator Pkg, DepIterator const * const Dep)
{
   if (List->IsNow(Pkg) == false)
      return true;

   // Already removed it
   if (List->IsFlag(Pkg,pkgOrderList::Removed) == true)
      return true;

   // Woops, it will not be re-installed!
   if (List->IsFlag(Pkg,pkgOrderList::InList) == false)
      return false;

   // these breaks on M-A:same packages can be dealt with. They 'loop' by design
   if (Dep != NULL && (*Dep)->Type == pkgCache::Dep::DpkgBreaks && Dep->IsMultiArchImplicit() == true)
      return true;

   // Essential packages get special treatment
   bool IsEssential = false;
   if ((Pkg->Flags & pkgCache::Flag::Essential) != 0)
      IsEssential = true;
   bool IsProtected = false;
   if ((Pkg->Flags & pkgCache::Flag::Important) != 0)
      IsProtected = true;

   /* Check for packages that are the dependents of essential packages and
      promote them too */
   if (Pkg->CurrentVer != 0)
   {
      for (pkgCache::DepIterator D = Pkg.RevDependsList(); D.end() == false &&
	   IsEssential == false; ++D)
	 if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends) {
	    if ((D.ParentPkg()->Flags & pkgCache::Flag::Essential) != 0)
	       IsEssential = true;
	    if ((D.ParentPkg()->Flags & pkgCache::Flag::Important) != 0)
	       IsProtected = true;
	 }
   }

   // dpkg will auto-deconfigure it, no need for the big remove hammer
   if (Dep != NULL && (*Dep)->Type == pkgCache::Dep::DpkgBreaks)
      return true;
   else if (IsEssential == true)
   {
      // FIXME: Unify messaging with Protected below.
      if (_config->FindB("APT::Force-LoopBreak",false) == false)
	 return _error->Error(_("This installation run will require temporarily "
				"removing the essential package %s due to a "
				"Conflicts/Pre-Depends loop. This is often bad, "
				"but if you really want to do it, activate the "
				"APT::Force-LoopBreak option."),Pkg.FullName().c_str());
   }
   else if (IsProtected == true)
   {
      // FIXME: Message should talk about Protected, not Essential, and unified.
      if (_config->FindB("APT::Force-LoopBreak",false) == false)
	 return _error->Error(_("This installation run will require temporarily "
				"removing the essential package %s due to a "
				"Conflicts/Pre-Depends loop. This is often bad, "
				"but if you really want to do it, activate the "
				"APT::Force-LoopBreak option."),Pkg.FullName().c_str());
   }

   bool Res = SmartRemove(Pkg);
   if (Cache[Pkg].Delete() == false)
      List->Flag(Pkg,pkgOrderList::Removed,pkgOrderList::States);

   return Res;
}
									/*}}}*/
// PM::SmartRemove - Removal Helper					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgPackageManager::SmartRemove(PkgIterator Pkg)
{
   if (List->IsNow(Pkg) == false)
      return true;

   List->Flag(Pkg,pkgOrderList::Configured,pkgOrderList::States);

   return Remove(Pkg,(Cache[Pkg].iFlags & pkgDepCache::Purge) == pkgDepCache::Purge);
}
									/*}}}*/
// PM::SmartUnPack - Install helper					/*{{{*/
// ---------------------------------------------------------------------
/* This puts the system in a state where it can Unpack Pkg, if Pkg is already
   unpacked, or when it has been unpacked, if Immediate==true it configures it. */
bool pkgPackageManager::SmartUnPack(PkgIterator Pkg, bool const Immediate, int const Depth)
{
   bool PkgLoop = List->IsFlag(Pkg,pkgOrderList::Loop);

   if (Debug) {
      clog << OutputInDepth(Depth) << "SmartUnPack " << Pkg.FullName();
      VerIterator InstallVer = VerIterator(Cache,Cache[Pkg].InstallVer);
      if (Pkg.CurrentVer() == 0)
        clog << " (install version " << InstallVer.VerStr() << ")";
      else
        clog << " (replace version " << Pkg.CurrentVer().VerStr() << " with " << InstallVer.VerStr() << ")";
      if (PkgLoop)
        clog << " (Only Perform PreUnpack Checks)";
      if (Immediate)
	 clog << " immediately";
      clog << endl;
   }

   VerIterator const instVer = Cache[Pkg].InstVerIter(Cache);

   /* PreUnpack Checks: This loop checks and attempts to rectify any problems that would prevent the package being unpacked.
      It addresses: PreDepends, Conflicts, Obsoletes and Breaks (DpkgBreaks). Any resolutions that do not require it should
      avoid configuration (calling SmartUnpack with Immediate=true), this is because when unpacking some packages with
      complex dependency structures, trying to configure some packages while breaking the loops can complicate things.
      This will be either dealt with if the package is configured as a dependency of Pkg (if and when Pkg is configured),
      or by the ConfigureAll call at the end of the for loop in OrderInstall. */
   bool SomethingBad = false, Changed = false;
   bool couldBeTemporaryRemoved = Depth != 0 && List->IsFlag(Pkg,pkgOrderList::Removed) == false;
   const unsigned int max_loops = _config->FindI("APT::pkgPackageManager::MaxLoopCount", 5000);
   unsigned int i = 0;
   do 
   {
      Changed = false;
      for (DepIterator D = instVer.DependsList(); D.end() == false; )
      {
	 // Compute a single dependency element (glob or)
	 pkgCache::DepIterator Start, End;
	 D.GlobOr(Start,End);

	 if (End->Type == pkgCache::Dep::PreDepends)
         {
	    bool Bad = true;
	    if (Debug)
	       clog << OutputInDepth(Depth) << "PreDepends order for " << Pkg.FullName() << std::endl;

	    // Look for easy targets: packages that are already okay
	    for (DepIterator Cur = Start; Bad == true; ++Cur)
	    {
	       std::unique_ptr<Version *[]> VList(Cur.AllTargets());
	       for (Version **I = VList.get(); *I != 0; ++I)
	       {
		  VerIterator Ver(Cache,*I);
		  PkgIterator Pkg = Ver.ParentPkg();

		  // See if the current version is ok
		  if (Pkg.CurrentVer() == Ver && List->IsNow(Pkg) == true &&
		      Pkg.State() == PkgIterator::NeedsNothing &&
		      (Cache[Pkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall)
		  {
		     Bad = false;
		     if (Debug)
			clog << OutputInDepth(Depth) << "Found ok package " << Pkg.FullName() << endl;
		     break;
		  }
	       }
	       if (Cur == End)
		  break;
	    }

	    // Look for something that could be configured.
	    for (DepIterator Cur = Start; Bad == true && Cur.end() == false; ++Cur)
	    {
	       std::unique_ptr<Version *[]> VList(Cur.AllTargets());
	       for (Version **I = VList.get(); *I != 0; ++I)
	       {
		  VerIterator Ver(Cache,*I);
		  PkgIterator DepPkg = Ver.ParentPkg();

		  // Not the install version
		  if (Cache[DepPkg].InstallVer != *I)
		     continue;

		  if (Cache[DepPkg].Keep() == true && DepPkg.State() == PkgIterator::NeedsNothing &&
			(Cache[DepPkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall)
		     continue;

		  if (List->IsFlag(DepPkg,pkgOrderList::Configured))
		  {
		     Bad = false;
		     break;
		  }

		  // check if it needs unpack or if configure is enough
		  if (List->IsFlag(DepPkg,pkgOrderList::UnPacked) == false)
		  {
		     // two packages pre-depending on each other can't be handled sanely
		     if (List->IsFlag(DepPkg,pkgOrderList::Loop) && PkgLoop)
		     {
			// this isn't an error as there is potential for something else to satisfy it
			// (like a provides or an or-group member)
			if (Debug)
			   clog << OutputInDepth(Depth) << "Unpack loop detected between " << DepPkg.FullName() << " and " << Pkg.FullName() << endl;
			continue;
		     }

		     if (Debug)
			clog << OutputInDepth(Depth) << "Trying to SmartUnpack " << DepPkg.FullName() << endl;
		     if (NonLoopingSmart(UNPACK_IMMEDIATE, Pkg, DepPkg, Depth, PkgLoop, &Bad, &Changed) == false)
			return false;
		  }
		  else
		  {
		     if (Debug)
			clog << OutputInDepth(Depth) << "Trying to SmartConfigure " << DepPkg.FullName() << endl;
		     if (NonLoopingSmart(CONFIGURE, Pkg, DepPkg, Depth, PkgLoop, &Bad, &Changed) == false)
			return false;
		  }
		  break;
	       }
	    }

	    if (Bad == true)
	       SomethingBad = true;
	 }
	 else if (End->Type == pkgCache::Dep::Conflicts ||
		  End->Type == pkgCache::Dep::Obsoletes ||
		  End->Type == pkgCache::Dep::DpkgBreaks)
	 {
	    std::unique_ptr<Version *[]> VList(End.AllTargets());
	    for (Version **I = VList.get(); *I != 0; ++I)
	    {
	       VerIterator Ver(Cache,*I);
	       PkgIterator ConflictPkg = Ver.ParentPkg();
	       if (ConflictPkg.CurrentVer() != Ver)
	       {
		  if (Debug)
		     std::clog << OutputInDepth(Depth) << "Ignore not-installed version " << Ver.VerStr() << " of " << ConflictPkg.FullName() << " for " << APT::PrettyDep(&Cache, End) << std::endl;
		  continue;
	       }

	       if (List->IsNow(ConflictPkg) == false)
	       {
		  if (Debug)
		     std::clog << OutputInDepth(Depth) << "Ignore already dealt-with version " << Ver.VerStr() << " of " << ConflictPkg.FullName() << " for " << APT::PrettyDep(&Cache, End) << std::endl;
		  continue;
	       }

	       if (List->IsFlag(ConflictPkg,pkgOrderList::Removed) == true)
	       {
		  if (Debug)
		     clog << OutputInDepth(Depth) << "Ignoring " << APT::PrettyDep(&Cache, End) << " as " << ConflictPkg.FullName() << "was temporarily removed" << endl;
		  continue;
	       }

	       if (List->IsFlag(ConflictPkg,pkgOrderList::Loop) && PkgLoop)
	       {
		  if (End->Type == pkgCache::Dep::DpkgBreaks && End.IsMultiArchImplicit() == true)
		  {
		     if (Debug)
			clog << OutputInDepth(Depth) << "Because dependency is MultiArchImplicit we ignored looping on: " << APT::PrettyPkg(&Cache, ConflictPkg) << endl;
		     continue;
		  }
		  if (Debug)
		  {
		     if (End->Type == pkgCache::Dep::DpkgBreaks)
			clog << OutputInDepth(Depth) << "Because of breaks knot, deconfigure " << ConflictPkg.FullName() << " temporarily" << endl;
		     else
			clog << OutputInDepth(Depth) << "Because of conflict knot, removing " << ConflictPkg.FullName() << " temporarily" << endl;
		  }
		  if (EarlyRemove(ConflictPkg, &End) == false)
		     return _error->Error("Internal Error, Could not early remove %s (%d)",ConflictPkg.FullName().c_str(), 3);
		  SomethingBad = true;
		  continue;
	       }

	       if (Cache[ConflictPkg].Delete() == false)
	       {
		  if (Debug)
		  {
		     clog << OutputInDepth(Depth) << "Unpacking " << ConflictPkg.FullName() << " to avoid " << APT::PrettyDep(&Cache, End);
		     if (PkgLoop == true)
			clog << " (Looping)";
		     clog << std::endl;
		  }
		  // we would like to avoid temporary removals and all that at best via a simple unpack
		  _error->PushToStack();
		  if (NonLoopingSmart(UNPACK, Pkg, ConflictPkg, Depth, PkgLoop, NULL, &Changed) == false)
		  {
		     // but if it fails ignore this failure and look for alternative ways of solving
		     if (Debug)
		     {
			clog << OutputInDepth(Depth) << "Avoidance unpack of " << ConflictPkg.FullName() << " failed for " << APT::PrettyDep(&Cache, End) << " ignoring:" << std::endl;
			_error->DumpErrors(std::clog, GlobalError::DEBUG, false);
		     }
		     _error->RevertToStack();
		     // ignorance can only happen if a) one of the offenders is already gone
		     if (List->IsFlag(ConflictPkg,pkgOrderList::Removed) == true)
		     {
			if (Debug)
			   clog << OutputInDepth(Depth) << "But " << ConflictPkg.FullName() << " was temporarily removed in the meantime to satisfy " << APT::PrettyDep(&Cache, End) << endl;
		     }
		     else if (List->IsFlag(Pkg,pkgOrderList::Removed) == true)
		     {
			if (Debug)
			   clog << OutputInDepth(Depth) << "But " << Pkg.FullName() << " was temporarily removed in the meantime to satisfy " <<  APT::PrettyDep(&Cache, End) << endl;
		     }
		     // or b) we can make one go (removal or dpkg auto-deconfigure)
		     else
		     {
			if (Debug)
			   clog << OutputInDepth(Depth) << "So temporary remove/deconfigure " << ConflictPkg.FullName() << " to satisfy " <<  APT::PrettyDep(&Cache, End) << endl;
			if (EarlyRemove(ConflictPkg, &End) == false)
			   return _error->Error("Internal Error, Could not early remove %s (%d)",ConflictPkg.FullName().c_str(), 2);
		     }
		  }
		  else
		     _error->MergeWithStack();
	       }
	       else
	       {
		  if (Debug)
		     clog << OutputInDepth(Depth) << "Removing " << ConflictPkg.FullName() << " now to avoid " << APT::PrettyDep(&Cache, End) << endl;
		  // no earlyremove() here as user has already agreed to the permanent removal
		  if (SmartRemove(ConflictPkg) == false)
		     return _error->Error("Internal Error, Could not early remove %s (%d)",ConflictPkg.FullName().c_str(), 1);
	       }
	    }
	 }
      }
      if (i++ > max_loops)
         return _error->Error("Internal error: APT::pkgPackageManager::MaxLoopCount reached in SmartConfigure for %s, aborting", Pkg.FullName().c_str());
   } while (Changed == true);

   if (SomethingBad == true)
      return _error->Error("Couldn't configure %s, probably a dependency cycle.", Pkg.FullName().c_str());

   if (couldBeTemporaryRemoved == true && List->IsFlag(Pkg,pkgOrderList::Removed) == true)
   {
      if (Debug)
	 std::clog << OutputInDepth(Depth) << "Prevent unpack as " << APT::PrettyPkg(&Cache, Pkg) << " is currently temporarily removed" << std::endl;
      return true;
   }

   // Check for reverse conflicts.
   if (CheckRConflicts(Pkg,Pkg.RevDependsList(),
		   instVer.VerStr()) == false)
		          return false;
   
   for (PrvIterator P = instVer.ProvidesList();
	P.end() == false; ++P)
      if (Pkg->Group != P.OwnerPkg()->Group)
	 CheckRConflicts(Pkg,P.ParentPkg().RevDependsList(),P.ProvideVersion());

   if (PkgLoop)
      return true;

   List->Flag(Pkg,pkgOrderList::UnPacked,pkgOrderList::States);

   if (Immediate == true && (instVer->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
   {
      /* Do lockstep M-A:same unpacking in two phases:
	 First unpack all installed architectures, then the not installed.
	 This way we avoid that M-A: enabled packages are installed before
	 their older non-M-A enabled packages are replaced by newer versions */
      bool const installed = Pkg->CurrentVer != 0;
      if (installed == true &&
	  (instVer != Pkg.CurrentVer() ||
	   ((Cache[Pkg].iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall)) &&
	  Install(Pkg,FileNames[Pkg->ID]) == false)
	 return false;
      for (PkgIterator P = Pkg.Group().PackageList();
	   P.end() == false; P = Pkg.Group().NextPkg(P))
      {
	 if (P->CurrentVer == 0 || P == Pkg || List->IsFlag(P,pkgOrderList::UnPacked) == true ||
	     Cache[P].InstallVer == 0 || (P.CurrentVer() == Cache[P].InstallVer &&
	      (Cache[Pkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall))
	    continue;
	 if (SmartUnPack(P, false, Depth + 1) == false)
	    return false;
      }
      if (installed == false && Install(Pkg,FileNames[Pkg->ID]) == false)
	 return false;
      for (PkgIterator P = Pkg.Group().PackageList();
	   P.end() == false; P = Pkg.Group().NextPkg(P))
      {
	 if (P->CurrentVer != 0 || P == Pkg || List->IsFlag(P,pkgOrderList::UnPacked) == true ||
	     List->IsFlag(P,pkgOrderList::Configured) == true ||
	     Cache[P].InstallVer == 0 || (P.CurrentVer() == Cache[P].InstallVer &&
	      (Cache[Pkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall))
	    continue;
	 if (SmartUnPack(P, false, Depth + 1) == false)
	    return false;
      }
   }
   // packages which are already unpacked don't need to be unpacked again
   else if ((instVer != Pkg.CurrentVer() ||
	     ((Cache[Pkg].iFlags & pkgDepCache::ReInstall) == pkgDepCache::ReInstall)) &&
	    Install(Pkg,FileNames[Pkg->ID]) == false)
      return false;

   if (Immediate == true) {
      // Perform immediate configuration of the package.
      _error->PushToStack();
      bool configured = SmartConfigure(Pkg, Depth + 1);
      _error->RevertToStack();

      if (not configured && Debug) {
	 clog << OutputInDepth(Depth);
	 ioprintf(clog, _("Could not perform immediate configuration on '%s'. "
			   "Please see man 5 apt.conf under APT::Immediate-Configure for details. (%d)"),
			 Pkg.FullName().c_str(), 2);
	 clog << endl;
      }
   }
   
   return true;
}
									/*}}}*/
// PM::OrderInstall - Installation ordering routine			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgPackageManager::OrderResult pkgPackageManager::OrderInstall()
{
   if (CreateOrderList() == false)
      return Failed;

   Reset();

   if (Debug == true)
      clog << "Beginning to order" << endl;

   std::string const planner = _config->Find("APT::Planner", "internal");
   unsigned int flags = 0;
   if (_config->FindB("APT::Immediate-Configure", true) == false)
      flags |= EIPP::Request::NO_IMMEDIATE_CONFIGURATION;
   else if (_config->FindB("APT::Immediate-Configure-All", false))
      flags |= EIPP::Request::IMMEDIATE_CONFIGURATION_ALL;
   else if (_config->FindB("APT::Force-LoopBreak", false))
      flags |= EIPP::Request::ALLOW_TEMPORARY_REMOVE_OF_ESSENTIALS;
   auto const ret = EIPP::OrderInstall(planner.c_str(), this, flags, nullptr);
   if (planner != "internal")
      return ret ? Completed : Failed;

   bool const ordering =
	_config->FindB("PackageManager::UnpackAll",true) ?
		List->OrderUnpack(FileNames) : List->OrderCritical();
   if (ordering == false)
   {
      _error->Error("Internal ordering error");
      return Failed;
   }

   if (Debug == true)
      clog << "Done ordering" << endl;

   bool DoneSomething = false;
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); ++I)
   {
      PkgIterator Pkg(Cache,*I);

      if (List->IsNow(Pkg) == false)
      {
	 if (Debug == true)
	    clog << "Skipping already done " << Pkg.FullName() << endl;
	 continue;
      }

      if (List->IsMissing(Pkg) == true)
      {
	 if (Debug == true)
	    clog << "Sequence completed at " << Pkg.FullName() << endl;
	 if (DoneSomething == false)
	 {
	    _error->Error("Internal Error, ordering was unable to handle the media swap");
	    return Failed;
	 }	 
	 return Incomplete;
      }
      
      // Sanity check
      if (Cache[Pkg].Keep() == true && 
	  Pkg.State() == pkgCache::PkgIterator::NeedsNothing &&
	  (Cache[Pkg].iFlags & pkgDepCache::ReInstall) != pkgDepCache::ReInstall)
      {
	 _error->Error("Internal Error, trying to manipulate a kept package (%s)",Pkg.FullName().c_str());
	 return Failed;
      }
      
      // Perform a delete or an install
      if (Cache[Pkg].Delete() == true)
      {
	 if (SmartRemove(Pkg) == false)
	    return Failed;
      }
      else
	 if (SmartUnPack(Pkg,List->IsFlag(Pkg,pkgOrderList::Immediate),0) == false)
	    return Failed;
      DoneSomething = true;
      
      if (ImmConfigureAll) {
         /* ConfigureAll here to pick up and packages left unconfigured because they were unpacked in the
            "PreUnpack Checks" section */
         if (!ConfigureAll())
            return Failed; 
      }
   }

   // Final run through the configure phase
   if (ConfigureAll() == false)
      return Failed;

   // Sanity check
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); ++I)
   {
      if (List->IsFlag(*I,pkgOrderList::Configured) == false)
      {
	 _error->Error("Internal error, packages left unconfigured. %s",
		       PkgIterator(Cache,*I).FullName().c_str());
	 return Failed;
      }
   }
	 
   return Completed;
}
// PM::DoInstallPostFork - Does install part that happens after the fork /*{{{*/
// ---------------------------------------------------------------------
pkgPackageManager::OrderResult 
pkgPackageManager::DoInstallPostFork(APT::Progress::PackageManager *progress)
{
   bool goResult;
   goResult = Go(progress);
   if(goResult == false) 
      return Failed;
   
   return Res;
}
									/*}}}*/	
// PM::DoInstall - Does the installation				/*{{{*/
// ---------------------------------------------------------------------
/* This uses the filenames in FileNames and the information in the
   DepCache to perform the installation of packages.*/
pkgPackageManager::OrderResult 
pkgPackageManager::DoInstall(APT::Progress::PackageManager *progress)
{
   if(DoInstallPreFork() == Failed)
      return Failed;
   
   return DoInstallPostFork(progress);
}
									/*}}}*/	      

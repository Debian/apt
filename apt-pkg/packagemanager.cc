// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: packagemanager.cc,v 1.30 2003/04/27 03:04:15 doogie Exp $
/* ######################################################################

   Package Manager - Abstacts the package manager

   More work is needed in the area of transitioning provides, ie exim
   replacing smail. This can cause interesing side effects.

   Other cases involving conflicts+replaces should be tested. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/packagemanager.h>
#include <apt-pkg/orderlist.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/sptr.h>
    
#include <apti18n.h>    
#include <iostream>
#include <fcntl.h> 

using namespace std;

// PM::PackageManager - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgPackageManager::pkgPackageManager(pkgDepCache *pCache) : Cache(*pCache)
{
   FileNames = new string[Cache.Head().PackageCount];
   List = 0;
   Debug = _config->FindB("Debug::pkgPackageManager",false);
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
   
   if (List->OrderUnpack() == false)
      return _error->Error("Internal ordering error");

   for (pkgOrderList::iterator I = List->begin(); I != List->end(); I++)
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
   for (PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
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

// PM::ImmediateAdd - Add the immediate flag recursivly			/*{{{*/
// ---------------------------------------------------------------------
/* This adds the immediate flag to the pkg and recursively to the
   dependendies 
 */
void pkgPackageManager::ImmediateAdd(PkgIterator I, bool UseInstallVer)
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

   for ( /* nothing */  ; D.end() == false; D++)
      if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends)
      {
	 if(!List->IsFlag(D.TargetPkg(), pkgOrderList::Immediate))
	 {
	    if(Debug)
	       clog << "ImmediateAdd(): Adding Immediate flag to " << I.Name() << endl;
	    List->Flag(D.TargetPkg(),pkgOrderList::Immediate);
	    ImmediateAdd(D.TargetPkg(), UseInstallVer);
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
   
   bool NoImmConfigure = !_config->FindB("APT::Immediate-Configure",true);
   
   // Generate the list of affected packages and sort it
   for (PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      // Ignore no-version packages
      if (I->VersionList == 0)
	 continue;
      
      // Mark the package and its dependends for immediate configuration
      if (((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential ||
	   (I->Flags & pkgCache::Flag::Important) == pkgCache::Flag::Important) &&
	  NoImmConfigure == false)
      {
	 if(Debug)
	    clog << "CreateOrderList(): Adding Immediate flag for " << I.Name() << endl;
	 List->Flag(I,pkgOrderList::Immediate);

	 // Look for other install packages to make immediate configurea
	 ImmediateAdd(I, true);
	 
	 // And again with the current version.
	 ImmediateAdd(I, false);
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
// PM::DepAlwaysTrue - Returns true if this dep is irrelevent		/*{{{*/
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
   for (;D.end() == false; D++)
   {
      if (D->Type != pkgCache::Dep::Conflicts &&
	  D->Type != pkgCache::Dep::Obsoletes)
	 continue;

      // The package hasnt been changed
      if (List->IsNow(Pkg) == false)
	 continue;
      
      // Ignore self conflicts, ignore conflicts from irrelevent versions
      if (D.ParentPkg() == Pkg || D.ParentVer() != D.ParentPkg().CurrentVer())
	 continue;
      
      if (Cache.VS().CheckDep(Ver,D->CompareOp,D.TargetVer()) == false)
	 continue;

      if (EarlyRemove(D.ParentPkg()) == false)
	 return _error->Error("Reverse conflicts early remove for package '%s' failed",
			      Pkg.Name());
   }
   return true;
}
									/*}}}*/
// PM::ConfigureAll - Run the all out configuration			/*{{{*/
// ---------------------------------------------------------------------
/* This configures every package. It is assumed they are all unpacked and
   that the final configuration is valid. */
bool pkgPackageManager::ConfigureAll()
{
   pkgOrderList OList(&Cache);
   
   // Populate the order list
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); I++)
      if (List->IsFlag(pkgCache::PkgIterator(Cache,*I),
		       pkgOrderList::UnPacked) == true)
	 OList.push_back(*I);
   
   if (OList.OrderConfigure() == false)
      return false;
   
   // Perform the configuring
   for (pkgOrderList::iterator I = OList.begin(); I != OList.end(); I++)
   {
      PkgIterator Pkg(Cache,*I);
      
      if (Configure(Pkg) == false)
	 return false;
      
      List->Flag(Pkg,pkgOrderList::Configured,pkgOrderList::States);
   }
   
   return true;
}
									/*}}}*/
// PM::SmartConfigure - Perform immediate configuration of the pkg	/*{{{*/
// ---------------------------------------------------------------------
/* This routine scheduals the configuration of the given package and all
   of it's dependents. */
bool pkgPackageManager::SmartConfigure(PkgIterator Pkg)
{
   pkgOrderList OList(&Cache);

   if (DepAdd(OList,Pkg) == false)
      return false;
   
   if (OList.OrderConfigure() == false)
      return false;
   
   // Perform the configuring
   for (pkgOrderList::iterator I = OList.begin(); I != OList.end(); I++)
   {
      PkgIterator Pkg(Cache,*I);
      
      if (Configure(Pkg) == false)
	 return false;
      
      List->Flag(Pkg,pkgOrderList::Configured,pkgOrderList::States);
   }

   // Sanity Check
   if (List->IsFlag(Pkg,pkgOrderList::Configured) == false)
      return _error->Error("Internal error, could not immediate configure %s",Pkg.Name());
   
   return true;
}
									/*}}}*/
// PM::DepAdd - Add all dependents to the oder list			/*{{{*/
// ---------------------------------------------------------------------
/* This recursively adds all dependents to the order list */
bool pkgPackageManager::DepAdd(pkgOrderList &OList,PkgIterator Pkg,int Depth)
{
   if (OList.IsFlag(Pkg,pkgOrderList::Added) == true)
      return true;
   if (List->IsFlag(Pkg,pkgOrderList::Configured) == true)
      return true;
   if (List->IsFlag(Pkg,pkgOrderList::UnPacked) == false)
      return false;
      
   // Put the package on the list
   OList.push_back(Pkg);
   OList.Flag(Pkg,pkgOrderList::Added);
   Depth++;

   // Check the dependencies to see if they are all satisfied.
   bool Bad = false;
   for (DepIterator D = Cache[Pkg].InstVerIter(Cache).DependsList(); D.end() == false;)
   {
      if (D->Type != pkgCache::Dep::Depends && D->Type != pkgCache::Dep::PreDepends)
      {
	 D++;
	 continue;
      }
      
      // Grok or groups
      Bad = true;
      for (bool LastOR = true; D.end() == false && LastOR == true; D++)
      {
	 LastOR = (D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
	 
	 if (Bad == false)
	    continue;

	 SPtrArray<Version *> VList = D.AllTargets();
	 for (Version **I = VList; *I != 0 && Bad == true; I++)
	 {
	    VerIterator Ver(Cache,*I);
	    PkgIterator Pkg = Ver.ParentPkg();

	    // See if the current version is ok
	    if (Pkg.CurrentVer() == Ver && List->IsNow(Pkg) == true && 
		Pkg.State() == PkgIterator::NeedsNothing)
	    {
	       Bad = false;
	       continue;
	    }
	    
	    // Not the install version 
	    if (Cache[Pkg].InstallVer != *I || 
		(Cache[Pkg].Keep() == true && Pkg.State() == PkgIterator::NeedsNothing))
	       continue;
	    
	    if (List->IsFlag(Pkg,pkgOrderList::UnPacked) == true)
	       Bad = !DepAdd(OList,Pkg,Depth);
	    if (List->IsFlag(Pkg,pkgOrderList::Configured) == true)
	       Bad = false;
	 }
      }
      
      if (Bad == true)
      {
	 OList.Flag(Pkg,0,pkgOrderList::Added);
	 OList.pop_back();
	 Depth--;
	 return false;
      }
   }
   
   Depth--;
   return true;
}
									/*}}}*/
// PM::EarlyRemove - Perform removal of packages before their time	/*{{{*/
// ---------------------------------------------------------------------
/* This is called to deal with conflicts arising from unpacking */
bool pkgPackageManager::EarlyRemove(PkgIterator Pkg)
{
   if (List->IsNow(Pkg) == false)
      return true;
	 
   // Already removed it
   if (List->IsFlag(Pkg,pkgOrderList::Removed) == true)
      return true;
   
   // Woops, it will not be re-installed!
   if (List->IsFlag(Pkg,pkgOrderList::InList) == false)
      return false;

   // Essential packages get special treatment
   bool IsEssential = false;
   if ((Pkg->Flags & pkgCache::Flag::Essential) != 0)
      IsEssential = true;

   /* Check for packages that are the dependents of essential packages and 
      promote them too */
   if (Pkg->CurrentVer != 0)
   {
      for (DepIterator D = Pkg.RevDependsList(); D.end() == false &&
	   IsEssential == false; D++)
	 if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends)
	    if ((D.ParentPkg()->Flags & pkgCache::Flag::Essential) != 0)
	       IsEssential = true;
   }

   if (IsEssential == true)
   {
      if (_config->FindB("APT::Force-LoopBreak",false) == false)
	 return _error->Error(_("This installation run will require temporarily "
				"removing the essential package %s due to a "
				"Conflicts/Pre-Depends loop. This is often bad, "
				"but if you really want to do it, activate the "
				"APT::Force-LoopBreak option."),Pkg.Name());
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
/* This performs the task of handling pre-depends. */
bool pkgPackageManager::SmartUnPack(PkgIterator Pkg)
{
   // Check if it is already unpacked
   if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure &&
       Cache[Pkg].Keep() == true)
   {
      List->Flag(Pkg,pkgOrderList::UnPacked,pkgOrderList::States);
      if (List->IsFlag(Pkg,pkgOrderList::Immediate) == true)
	 if (SmartConfigure(Pkg) == false)
	    return _error->Error("Internal Error, Could not perform immediate configuration (1) on %s",Pkg.Name());
      return true;
   }

   /* See if this packages install version has any predependencies
      that are not met by 'now' packages. */
   for (DepIterator D = Cache[Pkg].InstVerIter(Cache).DependsList(); 
	D.end() == false; )
   {
      // Compute a single dependency element (glob or)
      pkgCache::DepIterator Start;
      pkgCache::DepIterator End;
      D.GlobOr(Start,End);
      
      while (End->Type == pkgCache::Dep::PreDepends)
      {
	 // Look for possible ok targets.
	 SPtrArray<Version *> VList = Start.AllTargets();
	 bool Bad = true;
	 for (Version **I = VList; *I != 0 && Bad == true; I++)
	 {
	    VerIterator Ver(Cache,*I);
	    PkgIterator Pkg = Ver.ParentPkg();
	    
	    // See if the current version is ok
	    if (Pkg.CurrentVer() == Ver && List->IsNow(Pkg) == true && 
		Pkg.State() == PkgIterator::NeedsNothing)
	    {
	       Bad = false;
	       continue;
	    }
	 }
	 
	 // Look for something that could be configured.
	 for (Version **I = VList; *I != 0 && Bad == true; I++)
	 {
	    VerIterator Ver(Cache,*I);
	    PkgIterator Pkg = Ver.ParentPkg();
	    
	    // Not the install version 
	    if (Cache[Pkg].InstallVer != *I || 
		(Cache[Pkg].Keep() == true && Pkg.State() == PkgIterator::NeedsNothing))
	       continue;

	    Bad = !SmartConfigure(Pkg);
	 }

	 /* If this or element did not match then continue on to the
	    next or element until a matching element is found */
	 if (Bad == true)
	 {
	    // This triggers if someone make a pre-depends/depend loop.
	    if (Start == End)
	       return _error->Error("Couldn't configure pre-depend %s for %s, "
				    "probably a dependency cycle.",
				    End.TargetPkg().Name(),Pkg.Name());
	    Start++;
	 }
	 else
	    break;
      }
      
      if (End->Type == pkgCache::Dep::Conflicts || 
	  End->Type == pkgCache::Dep::Obsoletes)
      {
	 /* Look for conflicts. Two packages that are both in the install
	    state cannot conflict so we don't check.. */
	 SPtrArray<Version *> VList = End.AllTargets();
	 for (Version **I = VList; *I != 0; I++)
	 {
	    VerIterator Ver(Cache,*I);
	    PkgIterator Pkg = Ver.ParentPkg();
	    
	    // See if the current version is conflicting
	    if (Pkg.CurrentVer() == Ver && List->IsNow(Pkg) == true)
	    {
	       if (EarlyRemove(Pkg) == false)
		  return _error->Error("Internal Error, Could not early remove %s",Pkg.Name());
	    }
	 }
      }
   }

   // Check for reverse conflicts.
   if (CheckRConflicts(Pkg,Pkg.RevDependsList(),
		   Cache[Pkg].InstVerIter(Cache).VerStr()) == false)
      return false;
   
   for (PrvIterator P = Cache[Pkg].InstVerIter(Cache).ProvidesList(); 
	P.end() == false; P++)
      CheckRConflicts(Pkg,P.ParentPkg().RevDependsList(),P.ProvideVersion());
   
   if (Install(Pkg,FileNames[Pkg->ID]) == false)
      return false;
   
   List->Flag(Pkg,pkgOrderList::UnPacked,pkgOrderList::States);
   
   // Perform immedate configuration of the package.
   if (List->IsFlag(Pkg,pkgOrderList::Immediate) == true)
      if (SmartConfigure(Pkg) == false)
	 return _error->Error("Internal Error, Could not perform immediate configuration (2) on %s",Pkg.Name());
   
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
      clog << "Begining to order" << endl;

   if (List->OrderUnpack(FileNames) == false)
   {
      _error->Error("Internal ordering error");
      return Failed;
   }
   
   if (Debug == true)
      clog << "Done ordering" << endl;

   bool DoneSomething = false;
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); I++)
   {
      PkgIterator Pkg(Cache,*I);

      if (List->IsNow(Pkg) == false)
      {
	 if (Debug == true)
	    clog << "Skipping already done " << Pkg.Name() << endl;
	 continue;
      }
      
      if (List->IsMissing(Pkg) == true)
      {
	 if (Debug == true)
	    clog << "Sequence completed at " << Pkg.Name() << endl;
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
	 _error->Error("Internal Error, trying to manipulate a kept package (%s)",Pkg.Name());
	 return Failed;
      }
      
      // Perform a delete or an install
      if (Cache[Pkg].Delete() == true)
      {
	 if (SmartRemove(Pkg) == false)
	    return Failed;
      }
      else
	 if (SmartUnPack(Pkg) == false)
	    return Failed;
      DoneSomething = true;
   }
   
   // Final run through the configure phase
   if (ConfigureAll() == false)
      return Failed;

   // Sanity check
   for (pkgOrderList::iterator I = List->begin(); I != List->end(); I++)
   {
      if (List->IsFlag(*I,pkgOrderList::Configured) == false)
      {
	 _error->Error("Internal error, packages left unconfigured. %s",
		       PkgIterator(Cache,*I).Name());
	 return Failed;
      }
   }   
	 
   return Completed;
}
									/*}}}*/
// PM::DoInstallPostFork - Does install part that happens after the fork /*{{{*/
// ---------------------------------------------------------------------
pkgPackageManager::OrderResult 
pkgPackageManager::DoInstallPostFork(int statusFd)
{
      if(statusFd > 0)
         // FIXME: use SetCloseExec here once it taught about throwing
	 //        exceptions instead of doing _exit(100) on failure
	 fcntl(statusFd,F_SETFD,FD_CLOEXEC); 
      bool goResult = Go(statusFd);
      if(goResult == false) 
	 return Failed;

      return Res;
};

// PM::DoInstall - Does the installation				/*{{{*/
// ---------------------------------------------------------------------
/* This uses the filenames in FileNames and the information in the
   DepCache to perform the installation of packages.*/
pkgPackageManager::OrderResult pkgPackageManager::DoInstall(int statusFd)
{
   if(DoInstallPreFork() == Failed)
      return Failed;
   
   return DoInstallPostFork(statusFd);
}
									/*}}}*/

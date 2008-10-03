// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: orderlist.cc,v 1.14 2001/05/07 05:49:43 jgg Exp $
/* ######################################################################

   Order List - Represents and Manipulates an ordered list of packages.
   
   A list of packages can be ordered by a number of conflicting criteria
   each given a specific priority. Each package also has a set of flags
   indicating some useful things about it that are derived in the 
   course of sorting. The pkgPackageManager class uses this class for
   all of it's installation ordering needs.

   This is a modified version of Manoj's Routine B. It consists of four
   independent ordering algorithms that can be applied at for different
   points in the ordering. By appling progressivly fewer ordering
   operations it is possible to give each consideration it's own
   priority and create an order that satisfies the lowest applicable
   consideration.
   
   The rules for unpacking ordering are:
    1) Unpacking ignores Depends: on all packages
    2) Unpacking requires Conflicts: on -ALL- packages to be satisfied
    3) Unpacking requires PreDepends: on this package only to be satisfied
    4) Removing requires that no packages depend on the package to be
       removed.
   
   And the rule for configuration ordering is:
    1) Configuring requires that the Depends: of the package be satisfied
       Conflicts+PreDepends are ignored because unpacking says they are 
       already correct [exageration, it does check but we need not be 
       concerned]

   And some features that are valuable for unpacking ordering.
     f1) Unpacking a new package should advoid breaking dependencies of
         configured packages
     f2) Removal should not require a force, corrolory of f1
     f3) Unpacking should order by depends rather than fall back to random
         ordering.  
   
   Each of the features can be enabled in the sorting routine at an 
   arbitrary priority to give quite abit of control over the final unpacking
   order.

   The rules listed above may never be violated and are called Critical.
   When a critical rule is violated then a loop condition is recorded
   and will have to be delt with in the caller.

   The ordering keeps two lists, the main list and the 'After List'. The
   purpose of the after list is to allow packages to be delayed. This is done
   by setting the after flag on the package. Any package which requires this
   package to be ordered before will inherit the after flag and so on. This
   is used for CD swap ordering where all packages on a second CD have the 
   after flag set. This forces them and all their dependents to be ordered
   toward the end.
   
   There are complications in this algorithm when presented with cycles.
   For all known practical cases it works, all cases where it doesn't work
   is fixable by tweaking the package descriptions. However, it should be
   possible to impove this further to make some better choices when 
   presented with cycles. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/orderlist.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/configuration.h>

#include <iostream>
									/*}}}*/

using namespace std;

pkgOrderList *pkgOrderList::Me = 0;

// OrderList::pkgOrderList - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgOrderList::pkgOrderList(pkgDepCache *pCache) : Cache(*pCache)
{
   FileList = 0;
   Primary = 0;
   Secondary = 0;
   RevDepends = 0;
   Remove = 0;
   LoopCount = -1;
   Debug = _config->FindB("Debug::pkgOrderList",false);
   
   /* Construct the arrays, egcs 1.0.1 bug requires the package count
      hack */
   unsigned long Size = Cache.Head().PackageCount;
   Flags = new unsigned short[Size];
   End = List = new Package *[Size];
   memset(Flags,0,sizeof(*Flags)*Size);
}
									/*}}}*/
// OrderList::~pkgOrderList - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgOrderList::~pkgOrderList()
{
   delete [] List;
   delete [] Flags;
}
									/*}}}*/
// OrderList::IsMissing - Check if a file is missing			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgOrderList::IsMissing(PkgIterator Pkg) 
{
   // Skip packages to erase
   if (Cache[Pkg].Delete() == true)
      return false;

   // Skip Packages that need configure only.
   if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure && 
       Cache[Pkg].Keep() == true)
      return false;

   if (FileList == 0)
      return false;
   
   if (FileList[Pkg->ID].empty() == false)
      return false;
   return true;
}
									/*}}}*/

// OrderList::DoRun - Does an order run					/*{{{*/
// ---------------------------------------------------------------------
/* The caller is expeted to have setup the desired probe state */
bool pkgOrderList::DoRun()
{   
   // Temp list
   unsigned long Size = Cache.Head().PackageCount;
   SPtrArray<Package *> NList = new Package *[Size];
   SPtrArray<Package *> AfterList = new Package *[Size];
   AfterEnd = AfterList;
   
   Depth = 0;
   WipeFlags(Added | AddPending | Loop | InList);

   for (iterator I = List; I != End; I++)
      Flag(*I,InList);

   // Rebuild the main list into the temp list.
   iterator OldEnd = End;
   End = NList;
   for (iterator I = List; I != OldEnd; I++)
      if (VisitNode(PkgIterator(Cache,*I)) == false)
      {
	 End = OldEnd;
	 return false;
      }
   
   // Copy the after list to the end of the main list
   for (Package **I = AfterList; I != AfterEnd; I++)
      *End++ = *I;
   
   // Swap the main list to the new list
   delete [] List;
   List = NList.UnGuard();
   return true;
}
									/*}}}*/
// OrderList::OrderCritical - Perform critical unpacking ordering	/*{{{*/
// ---------------------------------------------------------------------
/* This performs predepends and immediate configuration ordering only. 
   This is termed critical unpacking ordering. Any loops that form are
   fatal and indicate that the packages cannot be installed. */
bool pkgOrderList::OrderCritical()
{
   FileList = 0;
   
   Primary = &pkgOrderList::DepUnPackPre;
   Secondary = 0;
   RevDepends = 0;
   Remove = 0;
   LoopCount = 0;
   
   // Sort
   Me = this;
   qsort(List,End - List,sizeof(*List),&OrderCompareB);   
   
   if (DoRun() == false)
      return false;
   
   if (LoopCount != 0)
      return _error->Error("Fatal, predepends looping detected");
   return true;
}
									/*}}}*/
// OrderList::OrderUnpack - Perform complete unpacking ordering		/*{{{*/
// ---------------------------------------------------------------------
/* This performs complete unpacking ordering and creates an order that is
   suitable for unpacking */
bool pkgOrderList::OrderUnpack(string *FileList)
{
   this->FileList = FileList;

   // Setup the after flags
   if (FileList != 0)
   {
      WipeFlags(After);
      
      // Set the inlist flag
      for (iterator I = List; I != End; I++)
      {
	 PkgIterator P(Cache,*I);
	 if (IsMissing(P) == true && IsNow(P) == true)
	     Flag(*I,After);
      }
   }
   
   Primary = &pkgOrderList::DepUnPackCrit;
   Secondary = &pkgOrderList::DepConfigure;
   RevDepends = &pkgOrderList::DepUnPackDep;
   Remove = &pkgOrderList::DepRemove;
   LoopCount = -1;

   // Sort
   Me = this;
   qsort(List,End - List,sizeof(*List),&OrderCompareA);

   if (Debug == true)
      clog << "** Pass A" << endl;
   if (DoRun() == false)
      return false;
   
   if (Debug == true)
      clog << "** Pass B" << endl;
   Secondary = 0;
   if (DoRun() == false)
      return false;

   if (Debug == true)
      clog << "** Pass C" << endl;
   LoopCount = 0;
   RevDepends = 0;
   Remove = 0;             // Otherwise the libreadline remove problem occures
   if (DoRun() == false)
      return false;
      
   if (Debug == true)
      clog << "** Pass D" << endl;
   LoopCount = 0;
   Primary = &pkgOrderList::DepUnPackPre;
   if (DoRun() == false)
      return false;

   if (Debug == true)
   {
      clog << "** Unpack ordering done" << endl;

      for (iterator I = List; I != End; I++)
      {
	 PkgIterator P(Cache,*I);
	 if (IsNow(P) == true)
	    clog << P.Name() << ' ' << IsMissing(P) << ',' << IsFlag(P,After) << endl;
      }
   }   

   return true;
}
									/*}}}*/
// OrderList::OrderConfigure - Perform configuration ordering		/*{{{*/
// ---------------------------------------------------------------------
/* This orders by depends only and produces an order which is suitable
   for configuration */
bool pkgOrderList::OrderConfigure()
{
   FileList = 0;
   Primary = &pkgOrderList::DepConfigure;
   Secondary = 0;
   RevDepends = 0;
   Remove = 0;
   LoopCount = -1;
   return DoRun();
}
									/*}}}*/

// OrderList::Score - Score the package for sorting			/*{{{*/
// ---------------------------------------------------------------------
/* Higher scores order earlier */
int pkgOrderList::Score(PkgIterator Pkg)
{
   // Removal is always done first
   if (Cache[Pkg].Delete() == true)
      return 200;
   
   // This should never happen..
   if (Cache[Pkg].InstVerIter(Cache).end() == true)
      return -1;
   
   int Score = 0;
   if ((Pkg->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
      Score += 100;

   if (IsFlag(Pkg,Immediate) == true)
      Score += 10;
   
   for (DepIterator D = Cache[Pkg].InstVerIter(Cache).DependsList(); 
	D.end() == false; D++)
      if (D->Type == pkgCache::Dep::PreDepends)
      {
	 Score += 50;
	 break;
      }
      
   // Important Required Standard Optional Extra
   signed short PrioMap[] = {0,5,4,3,1,0};
   if (Cache[Pkg].InstVerIter(Cache)->Priority <= 5)
      Score += PrioMap[Cache[Pkg].InstVerIter(Cache)->Priority];
   return Score;
}
									/*}}}*/
// OrderList::FileCmp - Compare by package file				/*{{{*/
// ---------------------------------------------------------------------
/* This compares by the package file that the install version is in. */
int pkgOrderList::FileCmp(PkgIterator A,PkgIterator B)
{
   if (Cache[A].Delete() == true && Cache[B].Delete() == true)
      return 0;
   if (Cache[A].Delete() == true)
      return -1;
   if (Cache[B].Delete() == true)
      return 1;
   
   if (Cache[A].InstVerIter(Cache).FileList().end() == true)
      return -1;
   if (Cache[B].InstVerIter(Cache).FileList().end() == true)
      return 1;
   
   pkgCache::PackageFile *FA = Cache[A].InstVerIter(Cache).FileList().File();
   pkgCache::PackageFile *FB = Cache[B].InstVerIter(Cache).FileList().File();
   if (FA < FB)
      return -1;
   if (FA > FB)
      return 1;
   return 0;
}
									/*}}}*/
// BoolCompare - Comparison function for two booleans			/*{{{*/
// ---------------------------------------------------------------------
/* */
static int BoolCompare(bool A,bool B)
{
   if (A == B)
      return 0;
   if (A == false)
      return -1;
   return 1;
}
									/*}}}*/
// OrderList::OrderCompareA - Order the installation by op		/*{{{*/
// ---------------------------------------------------------------------
/* This provides a first-pass sort of the list and gives a decent starting
    point for further complete ordering. It is used by OrderUnpack only */
int pkgOrderList::OrderCompareA(const void *a, const void *b)
{
   PkgIterator A(Me->Cache,*(Package **)a);
   PkgIterator B(Me->Cache,*(Package **)b);

   // We order packages with a set state toward the front
   int Res;
   if ((Res = BoolCompare(Me->IsNow(A),Me->IsNow(B))) != 0)
      return -1*Res;
   
   // We order missing files to toward the end
/*   if (Me->FileList != 0)
   {
      if ((Res = BoolCompare(Me->IsMissing(A),
			     Me->IsMissing(B))) != 0)
	 return Res;
   }*/
   
   if (A.State() != pkgCache::PkgIterator::NeedsNothing && 
       B.State() == pkgCache::PkgIterator::NeedsNothing)
      return -1;

   if (A.State() == pkgCache::PkgIterator::NeedsNothing && 
       B.State() != pkgCache::PkgIterator::NeedsNothing)
      return 1;
   
   int ScoreA = Me->Score(A);
   int ScoreB = Me->Score(B);
   if (ScoreA > ScoreB)
      return -1;
   
   if (ScoreA < ScoreB)
      return 1;

   return strcmp(A.Name(),B.Name());
}
									/*}}}*/
// OrderList::OrderCompareB - Order the installation by source		/*{{{*/
// ---------------------------------------------------------------------
/* This orders by installation source. This is useful to handle
   inter-source breaks */
int pkgOrderList::OrderCompareB(const void *a, const void *b)
{
   PkgIterator A(Me->Cache,*(Package **)a);
   PkgIterator B(Me->Cache,*(Package **)b);

   if (A.State() != pkgCache::PkgIterator::NeedsNothing && 
       B.State() == pkgCache::PkgIterator::NeedsNothing)
      return -1;

   if (A.State() == pkgCache::PkgIterator::NeedsNothing && 
       B.State() != pkgCache::PkgIterator::NeedsNothing)
      return 1;
   
   int F = Me->FileCmp(A,B);
   if (F != 0)
   {
      if (F > 0)
	 return -1;
      return 1;
   }
   
   int ScoreA = Me->Score(A);
   int ScoreB = Me->Score(B);
   if (ScoreA > ScoreB)
      return -1;
   
   if (ScoreA < ScoreB)
      return 1;

   return strcmp(A.Name(),B.Name());
}
									/*}}}*/

// OrderList::VisitDeps - Visit forward install dependencies		/*{{{*/
// ---------------------------------------------------------------------
/* This calls the dependency function for the normal forwards dependencies
   of the package */
bool pkgOrderList::VisitDeps(DepFunc F,PkgIterator Pkg)
{
   if (F == 0 || Pkg.end() == true || Cache[Pkg].InstallVer == 0)
      return true;
   
   return (this->*F)(Cache[Pkg].InstVerIter(Cache).DependsList());
}
									/*}}}*/
// OrderList::VisitRDeps - Visit reverse dependencies			/*{{{*/
// ---------------------------------------------------------------------
/* This calls the dependency function for all of the normal reverse depends
   of the package */
bool pkgOrderList::VisitRDeps(DepFunc F,PkgIterator Pkg)
{
   if (F == 0 || Pkg.end() == true)
      return true;
   
   return (this->*F)(Pkg.RevDependsList());
}
									/*}}}*/
// OrderList::VisitRProvides - Visit provides reverse dependencies	/*{{{*/
// ---------------------------------------------------------------------
/* This calls the dependency function for all reverse dependencies
   generated by the provides line on the package. */
bool pkgOrderList::VisitRProvides(DepFunc F,VerIterator Ver)
{
   if (F == 0 || Ver.end() == true)
      return true;
   
   bool Res = true;
   for (PrvIterator P = Ver.ProvidesList(); P.end() == false; P++)
      Res &= (this->*F)(P.ParentPkg().RevDependsList());
   return true;
}
									/*}}}*/
// OrderList::VisitProvides - Visit all of the providing packages	/*{{{*/
// ---------------------------------------------------------------------
/* This routine calls visit on all providing packages. */
bool pkgOrderList::VisitProvides(DepIterator D,bool Critical)
{   
   SPtrArray<Version *> List = D.AllTargets();
   for (Version **I = List; *I != 0; I++)
   {
      VerIterator Ver(Cache,*I);
      PkgIterator Pkg = Ver.ParentPkg();

      if (Cache[Pkg].Keep() == true && Pkg.State() == PkgIterator::NeedsNothing)
	 continue;
      
      if (D->Type != pkgCache::Dep::Conflicts &&
	  D->Type != pkgCache::Dep::DpkgBreaks &&
	  D->Type != pkgCache::Dep::Obsoletes &&
	  Cache[Pkg].InstallVer != *I)
	 continue;
      
      if ((D->Type == pkgCache::Dep::Conflicts ||
	   D->Type == pkgCache::Dep::DpkgBreaks ||
	   D->Type == pkgCache::Dep::Obsoletes) &&
	  (Version *)Pkg.CurrentVer() != *I)
	 continue;
      
      // Skip over missing files
      if (Critical == false && IsMissing(D.ParentPkg()) == true)
	 continue;

      if (VisitNode(Pkg) == false)
	 return false;
   }
   return true;
}
									/*}}}*/
// OrderList::VisitNode - Recursive ordering director			/*{{{*/
// ---------------------------------------------------------------------
/* This is the core ordering routine. It calls the set dependency
   consideration functions which then potentialy call this again. Finite
   depth is achived through the colouring mechinism. */
bool pkgOrderList::VisitNode(PkgIterator Pkg)
{
   // Looping or irrelevent.
   // This should probably trancend not installed packages
   if (Pkg.end() == true || IsFlag(Pkg,Added) == true || 
       IsFlag(Pkg,AddPending) == true || IsFlag(Pkg,InList) == false)
      return true;

   if (Debug == true)
   {
      for (int j = 0; j != Depth; j++) clog << ' ';
      clog << "Visit " << Pkg.Name() << endl;
   }
   
   Depth++;
   
   // Color grey
   Flag(Pkg,AddPending);

   DepFunc Old = Primary;
   
   // Perform immedate configuration of the package if so flagged.
   if (IsFlag(Pkg,Immediate) == true && Primary != &pkgOrderList::DepUnPackPre)
      Primary = &pkgOrderList::DepUnPackPreD;

   if (IsNow(Pkg) == true)
   {
      bool Res = true;
      if (Cache[Pkg].Delete() == false)
      {
	 // Primary
	 Res &= Res && VisitDeps(Primary,Pkg);
	 Res &= Res && VisitRDeps(Primary,Pkg);
	 Res &= Res && VisitRProvides(Primary,Pkg.CurrentVer());
	 Res &= Res && VisitRProvides(Primary,Cache[Pkg].InstVerIter(Cache));
	 
	 // RevDep
	 Res &= Res && VisitRDeps(RevDepends,Pkg);
	 Res &= Res && VisitRProvides(RevDepends,Pkg.CurrentVer());
	 Res &= Res && VisitRProvides(RevDepends,Cache[Pkg].InstVerIter(Cache));
	 
	 // Secondary
	 Res &= Res && VisitDeps(Secondary,Pkg);
	 Res &= Res && VisitRDeps(Secondary,Pkg);
	 Res &= Res && VisitRProvides(Secondary,Pkg.CurrentVer());
	 Res &= Res && VisitRProvides(Secondary,Cache[Pkg].InstVerIter(Cache));
      }
      else
      { 
	 // RevDep
	 Res &= Res && VisitRDeps(Remove,Pkg);
	 Res &= Res && VisitRProvides(Remove,Pkg.CurrentVer());
      }
   }
   
   if (IsFlag(Pkg,Added) == false)
   {
      Flag(Pkg,Added,Added | AddPending);
      if (IsFlag(Pkg,After) == true)
	 *AfterEnd++ = Pkg;
      else
	 *End++ = Pkg;
   }
   
   Primary = Old;
   Depth--;

   if (Debug == true)
   {
      for (int j = 0; j != Depth; j++) clog << ' ';
      clog << "Leave " << Pkg.Name() << ' ' << IsFlag(Pkg,Added) << ',' << IsFlag(Pkg,AddPending) << endl;
   }
   
   return true;
}
									/*}}}*/

// OrderList::DepUnPackCrit - Critical UnPacking ordering		/*{{{*/
// ---------------------------------------------------------------------
/* Critical unpacking ordering strives to satisfy Conflicts: and 
   PreDepends: only. When a prdepends is encountered the Primary 
   DepFunc is changed to be DepUnPackPreD. 

   Loops are preprocessed and logged. */
bool pkgOrderList::DepUnPackCrit(DepIterator D)
{
   for (; D.end() == false; D++)
   {
      if (D.Reverse() == true)
      {
	 /* Reverse depenanices are only interested in conflicts,
	    predepend breakage is ignored here */
	 if (D->Type != pkgCache::Dep::Conflicts && 
	     D->Type != pkgCache::Dep::Obsoletes)
	    continue;

	 // Duplication elimination, consider only the current version
	 if (D.ParentPkg().CurrentVer() != D.ParentVer())
	    continue;
	 
	 /* For reverse dependencies we wish to check if the
	    dependency is satisifed in the install state. The
	    target package (caller) is going to be in the installed
	    state. */
	 if (CheckDep(D) == true)
	    continue;

	 if (VisitNode(D.ParentPkg()) == false)
	    return false;
      }
      else
      {
	 /* Forward critical dependencies MUST be correct before the 
	    package can be unpacked. */
	 if (D->Type != pkgCache::Dep::Conflicts &&
	     D->Type != pkgCache::Dep::DpkgBreaks &&
	     D->Type != pkgCache::Dep::Obsoletes &&
	     D->Type != pkgCache::Dep::PreDepends)
	    continue;
	 	 	 	 
	 /* We wish to check if the dep is okay in the now state of the
	    target package against the install state of this package. */
	 if (CheckDep(D) == true)
	 {
	    /* We want to catch predepends loops with the code below.
	       Conflicts loops that are Dep OK are ignored */
	    if (IsFlag(D.TargetPkg(),AddPending) == false ||
		D->Type != pkgCache::Dep::PreDepends)
	       continue;
	 }

	 // This is the loop detection
	 if (IsFlag(D.TargetPkg(),Added) == true || 
	     IsFlag(D.TargetPkg(),AddPending) == true)
	 {
	    if (IsFlag(D.TargetPkg(),AddPending) == true)
	       AddLoop(D);
	    continue;
	 }

	 /* Predepends require a special ordering stage, they must have
	    all dependents installed as well */
	 DepFunc Old = Primary;
	 bool Res = false;
	 if (D->Type == pkgCache::Dep::PreDepends)
	    Primary = &pkgOrderList::DepUnPackPreD;
	 Res = VisitProvides(D,true);
	 Primary = Old;
	 if (Res == false)
	    return false;
      }	 
   }   
   return true;
}

// OrderList::DepUnPackPreD - Critical UnPacking ordering with depends	/*{{{*/
// ---------------------------------------------------------------------
/* Critical PreDepends (also configure immediate and essential) strives to
   ensure not only that all conflicts+predepends are met but that this
   package will be immediately configurable when it is unpacked. 

   Loops are preprocessed and logged. */
bool pkgOrderList::DepUnPackPreD(DepIterator D)
{
   if (D.Reverse() == true)
      return DepUnPackCrit(D);
   
   for (; D.end() == false; D++)
   {
      if (D.IsCritical() == false)
	 continue;

      /* We wish to check if the dep is okay in the now state of the
         target package against the install state of this package. */
      if (CheckDep(D) == true)
      {
	 /* We want to catch predepends loops with the code below.
	    Conflicts loops that are Dep OK are ignored */
	 if (IsFlag(D.TargetPkg(),AddPending) == false ||
	     D->Type != pkgCache::Dep::PreDepends)
	    continue;
      }
      
      // This is the loop detection
      if (IsFlag(D.TargetPkg(),Added) == true || 
	  IsFlag(D.TargetPkg(),AddPending) == true)
      {
	 if (IsFlag(D.TargetPkg(),AddPending) == true)
	    AddLoop(D);
	 continue;
      }
      
      if (VisitProvides(D,true) == false)
	 return false;
   }   
   return true;
}
									/*}}}*/
// OrderList::DepUnPackPre - Critical Predepends ordering		/*{{{*/
// ---------------------------------------------------------------------
/* Critical PreDepends (also configure immediate and essential) strives to
   ensure not only that all conflicts+predepends are met but that this
   package will be immediately configurable when it is unpacked. 

   Loops are preprocessed and logged. All loops will be fatal. */
bool pkgOrderList::DepUnPackPre(DepIterator D)
{
   if (D.Reverse() == true)
      return true;
   
   for (; D.end() == false; D++)
   {
      /* Only consider the PreDepends or Depends. Depends are only
       	 considered at the lowest depth or in the case of immediate
       	 configure */
      if (D->Type != pkgCache::Dep::PreDepends)
      {
	 if (D->Type == pkgCache::Dep::Depends)
	 {
	    if (Depth == 1 && IsFlag(D.ParentPkg(),Immediate) == false)
	       continue;
	 }
	 else
	    continue;
      }
      
      /* We wish to check if the dep is okay in the now state of the
         target package against the install state of this package. */
      if (CheckDep(D) == true)
      {
	 /* We want to catch predepends loops with the code below.
	    Conflicts loops that are Dep OK are ignored */
	 if (IsFlag(D.TargetPkg(),AddPending) == false)
	    continue;
      }

      // This is the loop detection
      if (IsFlag(D.TargetPkg(),Added) == true || 
	  IsFlag(D.TargetPkg(),AddPending) == true)
      {
	 if (IsFlag(D.TargetPkg(),AddPending) == true)
	    AddLoop(D);
	 continue;
      }
      
      if (VisitProvides(D,true) == false)
	 return false;
   }   
   return true;
}
									/*}}}*/
// OrderList::DepUnPackDep - Reverse dependency considerations		/*{{{*/
// ---------------------------------------------------------------------
/* Reverse dependencies are considered to determine if unpacking this
   package will break any existing dependencies. If so then those
   packages are ordered before this one so that they are in the
   UnPacked state. 
 
   The forwards depends loop is designed to bring the packages dependents
   close to the package. This helps reduce deconfigure time. 
   
   Loops are irrelevent to this. */
bool pkgOrderList::DepUnPackDep(DepIterator D)
{
   
   for (; D.end() == false; D++)
      if (D.IsCritical() == true)
      {
	 if (D.Reverse() == true)
	 {
	    /* Duplication prevention. We consider rev deps only on
	       the current version, a not installed package
	       cannot break */
	    if (D.ParentPkg()->CurrentVer == 0 ||
		D.ParentPkg().CurrentVer() != D.ParentVer())
	       continue;

	    // The dep will not break so it is irrelevent.
	    if (CheckDep(D) == true)
	       continue;
	    
	    // Skip over missing files
	    if (IsMissing(D.ParentPkg()) == true)
	       continue;
	    
	    if (VisitNode(D.ParentPkg()) == false)
	       return false;
	 }
	 else
	 {
	    if (D->Type == pkgCache::Dep::Depends)
	       if (VisitProvides(D,false) == false)
		  return false;

	    if (D->Type == pkgCache::Dep::DpkgBreaks)
	    {
	       if (CheckDep(D) == true)
		 continue;

	       if (VisitNode(D.TargetPkg()) == false)
		 return false;
	    }
	 }
      }
   return true;
}
									/*}}}*/
// OrderList::DepConfigure - Configuration ordering			/*{{{*/
// ---------------------------------------------------------------------
/* Configuration only ordering orders by the Depends: line only. It
   orders configuration so that when a package comes to be configured it's
   dependents are configured. 
 
   Loops are ingored. Depends loop entry points are chaotic. */
bool pkgOrderList::DepConfigure(DepIterator D)
{
   // Never consider reverse configuration dependencies.
   if (D.Reverse() == true)
      return true;
   
   for (; D.end() == false; D++)
      if (D->Type == pkgCache::Dep::Depends)
	 if (VisitProvides(D,false) == false)
	    return false;
   return true;
}
									/*}}}*/
// OrderList::DepRemove - Removal ordering				/*{{{*/
// ---------------------------------------------------------------------
/* Removal visits all reverse depends. It considers if the dependency
   of the Now state version to see if it is okay with removing this
   package. This check should always fail, but is provided for symetery
   with the other critical handlers.
 
   Loops are preprocessed and logged. Removal loops can also be
   detected in the critical handler. They are characterized by an
   old version of A depending on B but the new version of A conflicting
   with B, thus either A or B must break to install. */
bool pkgOrderList::DepRemove(DepIterator D)
{
   if (D.Reverse() == false)
      return true;
   for (; D.end() == false; D++)
      if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends)
      {
	 // Duplication elimination, consider the current version only
	 if (D.ParentPkg().CurrentVer() != D.ParentVer())
	    continue;

	 /* We wish to see if the dep on the parent package is okay
	    in the removed (install) state of the target pkg. */	 
	 if (CheckDep(D) == true)
	 {
	    // We want to catch loops with the code below.
	    if (IsFlag(D.ParentPkg(),AddPending) == false)
	       continue;
	 }

	 // This is the loop detection
	 if (IsFlag(D.ParentPkg(),Added) == true || 
	     IsFlag(D.ParentPkg(),AddPending) == true)
	 {
	    if (IsFlag(D.ParentPkg(),AddPending) == true)
	       AddLoop(D);
	    continue;
	 }

	 // Skip over missing files
	 if (IsMissing(D.ParentPkg()) == true)
	    continue;
	 
	 if (VisitNode(D.ParentPkg()) == false)
	    return false;
      }
   
   return true;
}
									/*}}}*/

// OrderList::AddLoop - Add a loop to the loop list			/*{{{*/
// ---------------------------------------------------------------------
/* We record the loops. This is a relic since loop breaking is done 
   genericaly as part of the safety routines. */
bool pkgOrderList::AddLoop(DepIterator D)
{
   if (LoopCount < 0 || LoopCount >= 20)
      return false;  
   
   // Skip dups
   if (LoopCount != 0)
   {
      if (Loops[LoopCount - 1].ParentPkg() == D.ParentPkg() ||
	  Loops[LoopCount - 1].TargetPkg() == D.ParentPkg())
	 return true;
   }
   
   Loops[LoopCount++] = D;
   
   // Mark the packages as being part of a loop.
   Flag(D.TargetPkg(),Loop);
   Flag(D.ParentPkg(),Loop);
   return true;
}
									/*}}}*/
// OrderList::WipeFlags - Unset the given flags from all packages	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgOrderList::WipeFlags(unsigned long F)
{
   unsigned long Size = Cache.Head().PackageCount;
   for (unsigned long I = 0; I != Size; I++)
      Flags[I] &= ~F;
}
									/*}}}*/
// OrderList::CheckDep - Check a dependency for truth			/*{{{*/
// ---------------------------------------------------------------------
/* This performs a complete analysis of the dependency wrt to the
   current add list. It returns true if after all events are
   performed it is still true. This sort of routine can be approximated
   by examining the DepCache, however in convoluted cases of provides
   this fails to produce a suitable result. */
bool pkgOrderList::CheckDep(DepIterator D)
{
   SPtrArray<Version *> List = D.AllTargets();
   bool Hit = false;
   for (Version **I = List; *I != 0; I++)
   {
      VerIterator Ver(Cache,*I);
      PkgIterator Pkg = Ver.ParentPkg();
      
      /* The meaning of Added and AddPending is subtle. AddPending is
       	 an indication that the package is looping. Because of the
       	 way ordering works Added means the package will be unpacked
       	 before this one and AddPending means after. It is therefore
       	 correct to ignore AddPending in all cases, but that exposes
       	 reverse-ordering loops which should be ignored. */
      if (IsFlag(Pkg,Added) == true ||
	  (IsFlag(Pkg,AddPending) == true && D.Reverse() == true))
      {
	 if (Cache[Pkg].InstallVer != *I)
	    continue;
      }
      else
	 if ((Version *)Pkg.CurrentVer() != *I || 
	     Pkg.State() != PkgIterator::NeedsNothing)
	    continue;
      
      /* Conflicts requires that all versions are not present, depends
         just needs one */
      if (D->Type != pkgCache::Dep::Conflicts && 
	  D->Type != pkgCache::Dep::DpkgBreaks && 
	  D->Type != pkgCache::Dep::Obsoletes)
      {
	 /* Try to find something that does not have the after flag set
	    if at all possible */
	 if (IsFlag(Pkg,After) == true)
	 {
	    Hit = true;
	    continue;
	 }
      
	 return true;
      }
      else
      {
	 if (IsFlag(Pkg,After) == true)
	    Flag(D.ParentPkg(),After);
	 
	 return false;
      }      
   }

   // We found a hit, but it had the after flag set
   if (Hit == true && D->Type == pkgCache::Dep::PreDepends)
   {
      Flag(D.ParentPkg(),After);
      return true;
   }
   
   /* Conflicts requires that all versions are not present, depends
      just needs one */
   if (D->Type == pkgCache::Dep::Conflicts ||
       D->Type == pkgCache::Dep::Obsoletes)
      return true;
   return false;
}
									/*}}}*/

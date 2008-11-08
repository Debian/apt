// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: apt-cache.cc,v 1.72 2004/04/30 04:34:03 mdz Exp $
/* ######################################################################
   
   apt-cache - Manages the cache files
   
   apt-cache provides some functions fo manipulating the cache files.
   It uses the command line interface common to all the APT tools. 
   
   Returns 100 on failure, 0 on success.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/sptr.h>

#include <config.h>
#include <apti18n.h>

#include <locale.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>

#include <iomanip>
									/*}}}*/

using namespace std;

pkgCache *GCache = 0;
pkgSourceList *SrcList = 0;

// LocalitySort - Sort a version list by package file locality		/*{{{*/
// ---------------------------------------------------------------------
/* */
int LocalityCompare(const void *a, const void *b)
{
   pkgCache::VerFile *A = *(pkgCache::VerFile **)a;
   pkgCache::VerFile *B = *(pkgCache::VerFile **)b;
   
   if (A == 0 && B == 0)
      return 0;
   if (A == 0)
      return 1;
   if (B == 0)
      return -1;
   
   if (A->File == B->File)
      return A->Offset - B->Offset;
   return A->File - B->File;
}

void LocalitySort(pkgCache::VerFile **begin,
		  unsigned long Count,size_t Size)
{   
   qsort(begin,Count,Size,LocalityCompare);
}

void LocalitySort(pkgCache::DescFile **begin,
		  unsigned long Count,size_t Size)
{   
   qsort(begin,Count,Size,LocalityCompare);
}
									/*}}}*/
// UnMet - Show unmet dependencies					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool UnMet(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   bool Important = _config->FindB("APT::Cache::Important",false);
   
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
      {
	 bool Header = false;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false;)
	 {
	    // Collect or groups
	    pkgCache::DepIterator Start;
	    pkgCache::DepIterator End;
	    D.GlobOr(Start,End);
	    
	    // Skip conflicts and replaces
	    if (End->Type != pkgCache::Dep::PreDepends &&
		End->Type != pkgCache::Dep::Depends && 
		End->Type != pkgCache::Dep::Suggests &&
		End->Type != pkgCache::Dep::Recommends &&
		End->Type != pkgCache::Dep::DpkgBreaks)
	       continue;

	    // Important deps only
	    if (Important == true)
	       if (End->Type != pkgCache::Dep::PreDepends &&
		   End->Type != pkgCache::Dep::Depends &&
		   End->Type != pkgCache::Dep::DpkgBreaks)
		  continue;
	    
	    // Verify the or group
	    bool OK = false;
	    pkgCache::DepIterator RealStart = Start;
	    do
	    {
	       // See if this dep is Ok
	       pkgCache::Version **VList = Start.AllTargets();
	       if (*VList != 0)
	       {
		  OK = true;
		  delete [] VList;
		  break;
	       }
	       delete [] VList;
	       
	       if (Start == End)
		  break;
	       Start++;
	    }
	    while (1);

	    // The group is OK
	    if (OK == true)
	       continue;
	    
	    // Oops, it failed..
	    if (Header == false)
	       ioprintf(cout,_("Package %s version %s has an unmet dep:\n"),
			P.Name(),V.VerStr());
	    Header = true;
	    
	    // Print out the dep type
	    cout << " " << End.DepType() << ": ";

	    // Show the group
	    Start = RealStart;
	    do
	    {
	       cout << Start.TargetPkg().Name();
	       if (Start.TargetVer() != 0)
		  cout << " (" << Start.CompType() << " " << Start.TargetVer() <<
		  ")";
	       if (Start == End)
		  break;
	       cout << " | ";
	       Start++;
	    }
	    while (1);
	    
	    cout << endl;
	 }	 
      }
   }   
   return true;
}
									/*}}}*/
// DumpPackage - Show a dump of a package record			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DumpPackage(CommandLine &CmdL)
{   
   pkgCache &Cache = *GCache;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }

      cout << "Package: " << Pkg.Name() << endl;
      cout << "Versions: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr();
	 for (pkgCache::VerFileIterator Vf = Cur.FileList(); Vf.end() == false; Vf++)
	    cout << " (" << Vf.File().FileName() << ")";
	 cout << endl;
	 for (pkgCache::DescIterator D = Cur.DescriptionList(); D.end() == false; D++)
	 {
	    cout << " Description Language: " << D.LanguageCode() << endl
		 << "                 File: " << D.FileList().File().FileName() << endl
		 << "                  MD5: " << D.md5() << endl;
	 }
	 cout << endl;
      }
      
      cout << endl;
      
      cout << "Reverse Depends: " << endl;
      for (pkgCache::DepIterator D = Pkg.RevDependsList(); D.end() != true; D++)
      {
	 cout << "  " << D.ParentPkg().Name() << ',' << D.TargetPkg().Name();
	 if (D->Version != 0)
	    cout << ' ' << DeNull(D.TargetVer()) << endl;
	 else
	    cout << endl;
      }
      
      cout << "Dependencies: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::DepIterator Dep = Cur.DependsList(); Dep.end() != true; Dep++)
	    cout << Dep.TargetPkg().Name() << " (" << (int)Dep->CompareOp << " " << DeNull(Dep.TargetVer()) << ") ";
	 cout << endl;
      }      

      cout << "Provides: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; Cur++)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::PrvIterator Prv = Cur.ProvidesList(); Prv.end() != true; Prv++)
	    cout << Prv.ParentPkg().Name() << " ";
	 cout << endl;
      }
      cout << "Reverse Provides: " << endl;
      for (pkgCache::PrvIterator Prv = Pkg.ProvidesList(); Prv.end() != true; Prv++)
	 cout << Prv.OwnerPkg().Name() << " " << Prv.OwnerVer().VerStr() << endl;            
   }

   return true;
}
									/*}}}*/
// Stats - Dump some nice statistics					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Stats(CommandLine &Cmd)
{
   pkgCache &Cache = *GCache;
   cout << _("Total package names: ") << Cache.Head().PackageCount << " (" <<
      SizeToStr(Cache.Head().PackageCount*Cache.Head().PackageSz) << ')' << endl;

   int Normal = 0;
   int Virtual = 0;
   int NVirt = 0;
   int DVirt = 0;
   int Missing = 0;
   pkgCache::PkgIterator I = Cache.PkgBegin();
   for (;I.end() != true; I++)
   {
      if (I->VersionList != 0 && I->ProvidesList == 0)
      {
	 Normal++;
	 continue;
      }

      if (I->VersionList != 0 && I->ProvidesList != 0)
      {
	 NVirt++;
	 continue;
      }
      
      if (I->VersionList == 0 && I->ProvidesList != 0)
      {
	 // Only 1 provides
	 if (I.ProvidesList()->NextProvides == 0)
	 {
	    DVirt++;
	 }
	 else
	    Virtual++;
	 continue;
      }
      if (I->VersionList == 0 && I->ProvidesList == 0)
      {
	 Missing++;
	 continue;
      }
   }
   cout << _("  Normal packages: ") << Normal << endl;
   cout << _("  Pure virtual packages: ") << Virtual << endl;
   cout << _("  Single virtual packages: ") << DVirt << endl;
   cout << _("  Mixed virtual packages: ") << NVirt << endl;
   cout << _("  Missing: ") << Missing << endl;
   
   cout << _("Total distinct versions: ") << Cache.Head().VersionCount << " (" <<
      SizeToStr(Cache.Head().VersionCount*Cache.Head().VersionSz) << ')' << endl;
   cout << _("Total distinct descriptions: ") << Cache.Head().DescriptionCount << " (" <<
      SizeToStr(Cache.Head().DescriptionCount*Cache.Head().DescriptionSz) << ')' << endl;
   cout << _("Total dependencies: ") << Cache.Head().DependsCount << " (" << 
      SizeToStr(Cache.Head().DependsCount*Cache.Head().DependencySz) << ')' << endl;
   
   cout << _("Total ver/file relations: ") << Cache.Head().VerFileCount << " (" <<
      SizeToStr(Cache.Head().VerFileCount*Cache.Head().VerFileSz) << ')' << endl;
   cout << _("Total Desc/File relations: ") << Cache.Head().DescFileCount << " (" <<
      SizeToStr(Cache.Head().DescFileCount*Cache.Head().DescFileSz) << ')' << endl;
   cout << _("Total Provides mappings: ") << Cache.Head().ProvidesCount << " (" <<
      SizeToStr(Cache.Head().ProvidesCount*Cache.Head().ProvidesSz) << ')' << endl;
   
   // String list stats
   unsigned long Size = 0;
   unsigned long Count = 0;
   for (pkgCache::StringItem *I = Cache.StringItemP + Cache.Head().StringList;
        I!= Cache.StringItemP; I = Cache.StringItemP + I->NextItem)
   {
      Count++;
      Size += strlen(Cache.StrP + I->String) + 1;
   }
   cout << _("Total globbed strings: ") << Count << " (" << SizeToStr(Size) << ')' << endl;

   unsigned long DepVerSize = 0;
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
      {
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false; D++)
	 {
	    if (D->Version != 0)
	       DepVerSize += strlen(D.TargetVer()) + 1;
	 }
      }
   }
   cout << _("Total dependency version space: ") << SizeToStr(DepVerSize) << endl;
   
   unsigned long Slack = 0;
   for (int I = 0; I != 7; I++)
      Slack += Cache.Head().Pools[I].ItemSize*Cache.Head().Pools[I].Count;
   cout << _("Total slack space: ") << SizeToStr(Slack) << endl;
   
   unsigned long Total = 0;
   Total = Slack + Size + Cache.Head().DependsCount*Cache.Head().DependencySz + 
           Cache.Head().VersionCount*Cache.Head().VersionSz +
           Cache.Head().PackageCount*Cache.Head().PackageSz + 
           Cache.Head().VerFileCount*Cache.Head().VerFileSz +
           Cache.Head().ProvidesCount*Cache.Head().ProvidesSz;
   cout << _("Total space accounted for: ") << SizeToStr(Total) << endl;
   
   return true;
}
									/*}}}*/
// Dump - show everything						/*{{{*/
// ---------------------------------------------------------------------
/* This is worthless except fer debugging things */
bool Dump(CommandLine &Cmd)
{
   pkgCache &Cache = *GCache;
   cout << "Using Versioning System: " << Cache.VS->Label << endl;
   
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      cout << "Package: " << P.Name() << endl;
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; V++)
      {
	 cout << " Version: " << V.VerStr() << endl;
	 cout << "     File: " << V.FileList().File().FileName() << endl;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false; D++)
	    cout << "  Depends: " << D.TargetPkg().Name() << ' ' << 
	                     DeNull(D.TargetVer()) << endl;
	 for (pkgCache::DescIterator D = V.DescriptionList(); D.end() == false; D++)
	 {
	    cout << " Description Language: " << D.LanguageCode() << endl
		 << "                 File: " << D.FileList().File().FileName() << endl
		 << "                  MD5: " << D.md5() << endl;
	 } 
      }      
   }

   for (pkgCache::PkgFileIterator F = Cache.FileBegin(); F.end() == false; F++)
   {
      cout << "File: " << F.FileName() << endl;
      cout << " Type: " << F.IndexType() << endl;
      cout << " Size: " << F->Size << endl;
      cout << " ID: " << F->ID << endl;
      cout << " Flags: " << F->Flags << endl;
      cout << " Time: " << TimeRFC1123(F->mtime) << endl;
      cout << " Archive: " << DeNull(F.Archive()) << endl;
      cout << " Component: " << DeNull(F.Component()) << endl;
      cout << " Version: " << DeNull(F.Version()) << endl;
      cout << " Origin: " << DeNull(F.Origin()) << endl;
      cout << " Site: " << DeNull(F.Site()) << endl;
      cout << " Label: " << DeNull(F.Label()) << endl;
      cout << " Architecture: " << DeNull(F.Architecture()) << endl;
   }

   return true;
}
									/*}}}*/
// DumpAvail - Print out the available list				/*{{{*/
// ---------------------------------------------------------------------
/* This is needed to make dpkg --merge happy.. I spent a bit of time to 
   make this run really fast, perhaps I went a little overboard.. */
bool DumpAvail(CommandLine &Cmd)
{
   pkgCache &Cache = *GCache;

   pkgPolicy Plcy(&Cache);
   if (ReadPinFile(Plcy) == false)
      return false;
   
   unsigned long Count = Cache.HeaderP->PackageCount+1;
   pkgCache::VerFile **VFList = new pkgCache::VerFile *[Count];
   memset(VFList,0,sizeof(*VFList)*Count);
   
   // Map versions that we want to write out onto the VerList array.
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {    
      if (P->VersionList == 0)
	 continue;
      
      /* Find the proper version to use. If the policy says there are no
         possible selections we return the installed version, if available..
       	 This prevents dselect from making it obsolete. */
      pkgCache::VerIterator V = Plcy.GetCandidateVer(P);
      if (V.end() == true)
      {
	 if (P->CurrentVer == 0)
	    continue;
	 V = P.CurrentVer();
      }
      
      pkgCache::VerFileIterator VF = V.FileList();
      for (; VF.end() == false ; VF++)
	 if ((VF.File()->Flags & pkgCache::Flag::NotSource) == 0)
	    break;
      
      /* Okay, here we have a bit of a problem.. The policy has selected the
         currently installed package - however it only exists in the
       	 status file.. We need to write out something or dselect will mark
         the package as obsolete! Thus we emit the status file entry, but
         below we remove the status line to make it valid for the 
         available file. However! We only do this if their do exist *any*
         non-source versions of the package - that way the dselect obsolete
         handling works OK. */
      if (VF.end() == true)
      {
	 for (pkgCache::VerIterator Cur = P.VersionList(); Cur.end() != true; Cur++)
	 {
	    for (VF = Cur.FileList(); VF.end() == false; VF++)
	    {	 
	       if ((VF.File()->Flags & pkgCache::Flag::NotSource) == 0)
	       {
		  VF = V.FileList();
		  break;
	       }
	    }
	    
	    if (VF.end() == false)
	       break;
	 }
      }
      
      VFList[P->ID] = VF;
   }
   
   LocalitySort(VFList,Count,sizeof(*VFList));

   // Iterate over all the package files and write them out.
   char *Buffer = new char[Cache.HeaderP->MaxVerFileSize+10];
   for (pkgCache::VerFile **J = VFList; *J != 0;)
   {
      pkgCache::PkgFileIterator File(Cache,(*J)->File + Cache.PkgFileP);
      if (File.IsOk() == false)
      {
	 _error->Error(_("Package file %s is out of sync."),File.FileName());
	 break;
      }

      FileFd PkgF(File.FileName(),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 break;
      
      /* Write all of the records from this package file, since we
       	 already did locality sorting we can now just seek through the
       	 file in read order. We apply 1 more optimization here, since often
       	 there will be < 1 byte gaps between records (for the \n) we read that
       	 into the next buffer and offset a bit.. */
      unsigned long Pos = 0;
      for (; *J != 0; J++)
      {
	 if ((*J)->File + Cache.PkgFileP != File)
	    break;
	 
	 const pkgCache::VerFile &VF = **J;

	 // Read the record and then write it out again.
	 unsigned long Jitter = VF.Offset - Pos;
	 if (Jitter > 8)
	 {
	    if (PkgF.Seek(VF.Offset) == false)
	       break;
	    Jitter = 0;
	 }
	 
	 if (PkgF.Read(Buffer,VF.Size + Jitter) == false)
	    break;
	 Buffer[VF.Size + Jitter] = '\n';
	 
	 // See above..
	 if ((File->Flags & pkgCache::Flag::NotSource) == pkgCache::Flag::NotSource)
	 {
	    pkgTagSection Tags;
	    TFRewriteData RW[] = {{"Status",0},{"Config-Version",0},{}};
	    const char *Zero = 0;
	    if (Tags.Scan(Buffer+Jitter,VF.Size+1) == false ||
		TFRewrite(stdout,Tags,&Zero,RW) == false)
	    {
	       _error->Error("Internal Error, Unable to parse a package record");
	       break;
	    }
	    fputc('\n',stdout);
	 }
	 else
	 {
	    if (fwrite(Buffer+Jitter,VF.Size+1,1,stdout) != 1)
	       break;
	 }
	 
	 Pos = VF.Offset + VF.Size;
      }

      fflush(stdout);
      if (_error->PendingError() == true)
         break;
   }
   
   delete [] Buffer;
   delete [] VFList;
   return !_error->PendingError();
}
									/*}}}*/
// Depends - Print out a dependency tree				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Depends(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   SPtrArray<unsigned> Colours = new unsigned[Cache.Head().PackageCount];
   memset(Colours,0,sizeof(*Colours)*Cache.Head().PackageCount);
   
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
      Colours[Pkg->ID] = 1;
   }
   
   bool Recurse = _config->FindB("APT::Cache::RecurseDepends",false);
   bool Installed = _config->FindB("APT::Cache::Installed",false);
   bool DidSomething;
   do
   {
      DidSomething = false;
      for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
      {
	 if (Colours[Pkg->ID] != 1)
	    continue;
	 Colours[Pkg->ID] = 2;
	 DidSomething = true;
	 
	 pkgCache::VerIterator Ver = Pkg.VersionList();
	 if (Ver.end() == true)
	 {
	    cout << '<' << Pkg.Name() << '>' << endl;
	    continue;
	 }
	 
	 cout << Pkg.Name() << endl;
	 
	 for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
	 {

	    pkgCache::PkgIterator Trg = D.TargetPkg();

	    if((Installed && Trg->CurrentVer != 0) || !Installed)
	      {

		if ((D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or)
		  cout << " |";
		else
		  cout << "  ";
	    
		// Show the package
		if (Trg->VersionList == 0)
		  cout << D.DepType() << ": <" << Trg.Name() << ">" << endl;
		else
		  cout << D.DepType() << ": " << Trg.Name() << endl;
	    
		if (Recurse == true)
		  Colours[D.TargetPkg()->ID]++;

	      }
	    
	    // Display all solutions
	    SPtrArray<pkgCache::Version *> List = D.AllTargets();
	    pkgPrioSortList(Cache,List);
	    for (pkgCache::Version **I = List; *I != 0; I++)
	    {
	       pkgCache::VerIterator V(Cache,*I);
	       if (V != Cache.VerP + V.ParentPkg()->VersionList ||
		   V->ParentPkg == D->Package)
		  continue;
	       cout << "    " << V.ParentPkg().Name() << endl;
	       
	       if (Recurse == true)
		  Colours[D.ParentPkg()->ID]++;
	    }
	 }
      }      
   }   
   while (DidSomething == true);
   
   return true;
}

// RDepends - Print out a reverse dependency tree - mbc			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool RDepends(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   SPtrArray<unsigned> Colours = new unsigned[Cache.Head().PackageCount];
   memset(Colours,0,sizeof(*Colours)*Cache.Head().PackageCount);
   
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
      Colours[Pkg->ID] = 1;
   }
   
   bool Recurse = _config->FindB("APT::Cache::RecurseDepends",false);
   bool Installed = _config->FindB("APT::Cache::Installed",false);
   bool DidSomething;
   do
   {
      DidSomething = false;
      for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
      {
	 if (Colours[Pkg->ID] != 1)
	    continue;
	 Colours[Pkg->ID] = 2;
	 DidSomething = true;
	 
	 pkgCache::VerIterator Ver = Pkg.VersionList();
	 if (Ver.end() == true)
	 {
	    cout << '<' << Pkg.Name() << '>' << endl;
	    continue;
	 }
	 
	 cout << Pkg.Name() << endl;
	 
	 cout << "Reverse Depends:" << endl;
	 for (pkgCache::DepIterator D = Pkg.RevDependsList(); D.end() == false; D++)
	 {	    
	    // Show the package
	    pkgCache::PkgIterator Trg = D.ParentPkg();

	    if((Installed && Trg->CurrentVer != 0) || !Installed)
	      {

		if ((D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or)
		  cout << " |";
		else
		  cout << "  ";

		if (Trg->VersionList == 0)
		  cout << D.DepType() << ": <" << Trg.Name() << ">" << endl;
		else
		  cout << Trg.Name() << endl;

		if (Recurse == true)
		  Colours[D.ParentPkg()->ID]++;

	      }
	    
	    // Display all solutions
	    SPtrArray<pkgCache::Version *> List = D.AllTargets();
	    pkgPrioSortList(Cache,List);
	    for (pkgCache::Version **I = List; *I != 0; I++)
	    {
	       pkgCache::VerIterator V(Cache,*I);
	       if (V != Cache.VerP + V.ParentPkg()->VersionList ||
		   V->ParentPkg == D->Package)
		  continue;
	       cout << "    " << V.ParentPkg().Name() << endl;
	       
	       if (Recurse == true)
		  Colours[D.ParentPkg()->ID]++;
	    }
	 }
      }      
   }   
   while (DidSomething == true);
   
   return true;
}

									/*}}}*/


// xvcg - Generate a graph for xvcg					/*{{{*/
// ---------------------------------------------------------------------
// Code contributed from Junichi Uekawa <dancer@debian.org> on 20 June 2002.

bool XVcg(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   bool GivenOnly = _config->FindB("APT::Cache::GivenOnly",false);
   
   /* Normal packages are boxes
      Pure Provides are triangles
      Mixed are diamonds
      rhomb are missing packages*/
   const char *Shapes[] = {"ellipse","triangle","box","rhomb"};
   
   /* Initialize the list of packages to show.
      1 = To Show
      2 = To Show no recurse
      3 = Emitted no recurse
      4 = Emitted
      0 = None */
   enum States {None=0, ToShow, ToShowNR, DoneNR, Done};
   enum TheFlags {ForceNR=(1<<0)};
   unsigned char *Show = new unsigned char[Cache.Head().PackageCount];
   unsigned char *Flags = new unsigned char[Cache.Head().PackageCount];
   unsigned char *ShapeMap = new unsigned char[Cache.Head().PackageCount];
   
   // Show everything if no arguments given
   if (CmdL.FileList[1] == 0)
      for (unsigned long I = 0; I != Cache.Head().PackageCount; I++)
	 Show[I] = ToShow;
   else
      for (unsigned long I = 0; I != Cache.Head().PackageCount; I++)
	 Show[I] = None;
   memset(Flags,0,sizeof(*Flags)*Cache.Head().PackageCount);
   
   // Map the shapes
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
   {   
      if (Pkg->VersionList == 0)
      {
	 // Missing
	 if (Pkg->ProvidesList == 0)
	    ShapeMap[Pkg->ID] = 0;
	 else
	    ShapeMap[Pkg->ID] = 1;
      }
      else
      {
	 // Normal
	 if (Pkg->ProvidesList == 0)
	    ShapeMap[Pkg->ID] = 2;
	 else
	    ShapeMap[Pkg->ID] = 3;
      }
   }
   
   // Load the list of packages from the command line into the show list
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      // Process per-package flags
      string P = *I;
      bool Force = false;
      if (P.length() > 3)
      {
	 if (P.end()[-1] == '^')
	 {
	    Force = true;
	    P.erase(P.end()-1);
	 }
	 
	 if (P.end()[-1] == ',')
	    P.erase(P.end()-1);
      }
      
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache.FindPkg(P);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
      Show[Pkg->ID] = ToShow;
      
      if (Force == true)
	 Flags[Pkg->ID] |= ForceNR;
   }
   
   // Little header
   cout << "graph: { title: \"packages\"" << endl <<
     "xmax: 700 ymax: 700 x: 30 y: 30" << endl <<
     "layout_downfactor: 8" << endl;

   bool Act = true;
   while (Act == true)
   {
      Act = false;
      for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
      {
	 // See we need to show this package
	 if (Show[Pkg->ID] == None || Show[Pkg->ID] >= DoneNR)
	    continue;

	 //printf ("node: { title: \"%s\" label: \"%s\" }\n", Pkg.Name(), Pkg.Name());
	 
	 // Colour as done
	 if (Show[Pkg->ID] == ToShowNR || (Flags[Pkg->ID] & ForceNR) == ForceNR)
	 {
	    // Pure Provides and missing packages have no deps!
	    if (ShapeMap[Pkg->ID] == 0 || ShapeMap[Pkg->ID] == 1)
	       Show[Pkg->ID] = Done;
	    else
	       Show[Pkg->ID] = DoneNR;
	 }	 
	 else
	    Show[Pkg->ID] = Done;
	 Act = true;

	 // No deps to map out
	 if (Pkg->VersionList == 0 || Show[Pkg->ID] == DoneNR)
	    continue;
	 
	 pkgCache::VerIterator Ver = Pkg.VersionList();
	 for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
	 {
	    // See if anything can meet this dep
	    // Walk along the actual package providing versions
	    bool Hit = false;
	    pkgCache::PkgIterator DPkg = D.TargetPkg();
	    for (pkgCache::VerIterator I = DPkg.VersionList();
		      I.end() == false && Hit == false; I++)
	    {
	       if (Cache.VS->CheckDep(I.VerStr(),D->CompareOp,D.TargetVer()) == true)
		  Hit = true;
	    }
	    
	    // Follow all provides
	    for (pkgCache::PrvIterator I = DPkg.ProvidesList(); 
		      I.end() == false && Hit == false; I++)
	    {
	       if (Cache.VS->CheckDep(I.ProvideVersion(),D->CompareOp,D.TargetVer()) == false)
		  Hit = true;
	    }
	    

	    // Only graph critical deps	    
	    if (D.IsCritical() == true)
	    {
	       printf ("edge: { sourcename: \"%s\" targetname: \"%s\" class: 2 ",Pkg.Name(), D.TargetPkg().Name() );
	       
	       // Colour the node for recursion
	       if (Show[D.TargetPkg()->ID] <= DoneNR)
	       {
		  /* If a conflicts does not meet anything in the database
		     then show the relation but do not recurse */
		  if (Hit == false && 
		      (D->Type == pkgCache::Dep::Conflicts ||
		       D->Type == pkgCache::Dep::DpkgBreaks ||
		       D->Type == pkgCache::Dep::Obsoletes))
		  {
		     if (Show[D.TargetPkg()->ID] == None && 
			 Show[D.TargetPkg()->ID] != ToShow)
			Show[D.TargetPkg()->ID] = ToShowNR;
		  }		  
		  else
		  {
		     if (GivenOnly == true && Show[D.TargetPkg()->ID] != ToShow)
			Show[D.TargetPkg()->ID] = ToShowNR;
		     else
			Show[D.TargetPkg()->ID] = ToShow;
		  }
	       }
	       
	       // Edge colour
	       switch(D->Type)
	       {
		  case pkgCache::Dep::Conflicts:
		    printf("label: \"conflicts\" color: lightgreen }\n");
		    break;
		  case pkgCache::Dep::DpkgBreaks:
		    printf("label: \"breaks\" color: lightgreen }\n");
		    break;
		  case pkgCache::Dep::Obsoletes:
		    printf("label: \"obsoletes\" color: lightgreen }\n");
		    break;
		  
		  case pkgCache::Dep::PreDepends:
		    printf("label: \"predepends\" color: blue }\n");
		    break;
		  
		  default:
		    printf("}\n");
		  break;
	       }	       
	    }	    
	 }
      }
   }   
   
   /* Draw the box colours after the fact since we can not tell what colour
      they should be until everything is finished drawing */
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
   {
      if (Show[Pkg->ID] < DoneNR)
	 continue;

      if (Show[Pkg->ID] == DoneNR)
	 printf("node: { title: \"%s\" label: \"%s\" color: orange shape: %s }\n", Pkg.Name(), Pkg.Name(),
		Shapes[ShapeMap[Pkg->ID]]);
      else
	printf("node: { title: \"%s\" label: \"%s\" shape: %s }\n", Pkg.Name(), Pkg.Name(), 
		Shapes[ShapeMap[Pkg->ID]]);
      
   }
   
   printf("}\n");
   return true;
}
									/*}}}*/


// Dotty - Generate a graph for Dotty					/*{{{*/
// ---------------------------------------------------------------------
/* Dotty is the graphvis program for generating graphs. It is a fairly
   simple queuing algorithm that just writes dependencies and nodes. 
   http://www.research.att.com/sw/tools/graphviz/ */
bool Dotty(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   bool GivenOnly = _config->FindB("APT::Cache::GivenOnly",false);
   
   /* Normal packages are boxes
      Pure Provides are triangles
      Mixed are diamonds
      Hexagons are missing packages*/
   const char *Shapes[] = {"hexagon","triangle","box","diamond"};
   
   /* Initialize the list of packages to show.
      1 = To Show
      2 = To Show no recurse
      3 = Emitted no recurse
      4 = Emitted
      0 = None */
   enum States {None=0, ToShow, ToShowNR, DoneNR, Done};
   enum TheFlags {ForceNR=(1<<0)};
   unsigned char *Show = new unsigned char[Cache.Head().PackageCount];
   unsigned char *Flags = new unsigned char[Cache.Head().PackageCount];
   unsigned char *ShapeMap = new unsigned char[Cache.Head().PackageCount];
   
   // Show everything if no arguments given
   if (CmdL.FileList[1] == 0)
      for (unsigned long I = 0; I != Cache.Head().PackageCount; I++)
	 Show[I] = ToShow;
   else
      for (unsigned long I = 0; I != Cache.Head().PackageCount; I++)
	 Show[I] = None;
   memset(Flags,0,sizeof(*Flags)*Cache.Head().PackageCount);
   
   // Map the shapes
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
   {   
      if (Pkg->VersionList == 0)
      {
	 // Missing
	 if (Pkg->ProvidesList == 0)
	    ShapeMap[Pkg->ID] = 0;
	 else
	    ShapeMap[Pkg->ID] = 1;
      }
      else
      {
	 // Normal
	 if (Pkg->ProvidesList == 0)
	    ShapeMap[Pkg->ID] = 2;
	 else
	    ShapeMap[Pkg->ID] = 3;
      }
   }
   
   // Load the list of packages from the command line into the show list
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      // Process per-package flags
      string P = *I;
      bool Force = false;
      if (P.length() > 3)
      {
	 if (P.end()[-1] == '^')
	 {
	    Force = true;
	    P.erase(P.end()-1);
	 }
	 
	 if (P.end()[-1] == ',')
	    P.erase(P.end()-1);
      }
      
      // Locate the package
      pkgCache::PkgIterator Pkg = Cache.FindPkg(P);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
      Show[Pkg->ID] = ToShow;
      
      if (Force == true)
	 Flags[Pkg->ID] |= ForceNR;
   }
   
   // Little header
   printf("digraph packages {\n");
   printf("concentrate=true;\n");
   printf("size=\"30,40\";\n");
   
   bool Act = true;
   while (Act == true)
   {
      Act = false;
      for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
      {
	 // See we need to show this package
	 if (Show[Pkg->ID] == None || Show[Pkg->ID] >= DoneNR)
	    continue;
	 
	 // Colour as done
	 if (Show[Pkg->ID] == ToShowNR || (Flags[Pkg->ID] & ForceNR) == ForceNR)
	 {
	    // Pure Provides and missing packages have no deps!
	    if (ShapeMap[Pkg->ID] == 0 || ShapeMap[Pkg->ID] == 1)
	       Show[Pkg->ID] = Done;
	    else
	       Show[Pkg->ID] = DoneNR;
	 }	 
	 else
	    Show[Pkg->ID] = Done;
	 Act = true;

	 // No deps to map out
	 if (Pkg->VersionList == 0 || Show[Pkg->ID] == DoneNR)
	    continue;
	 
	 pkgCache::VerIterator Ver = Pkg.VersionList();
	 for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
	 {
	    // See if anything can meet this dep
	    // Walk along the actual package providing versions
	    bool Hit = false;
	    pkgCache::PkgIterator DPkg = D.TargetPkg();
	    for (pkgCache::VerIterator I = DPkg.VersionList();
		      I.end() == false && Hit == false; I++)
	    {
	       if (Cache.VS->CheckDep(I.VerStr(),D->CompareOp,D.TargetVer()) == true)
		  Hit = true;
	    }
	    
	    // Follow all provides
	    for (pkgCache::PrvIterator I = DPkg.ProvidesList(); 
		      I.end() == false && Hit == false; I++)
	    {
	       if (Cache.VS->CheckDep(I.ProvideVersion(),D->CompareOp,D.TargetVer()) == false)
		  Hit = true;
	    }
	    
	    // Only graph critical deps	    
	    if (D.IsCritical() == true)
	    {
	       printf("\"%s\" -> \"%s\"",Pkg.Name(),D.TargetPkg().Name());
	       
	       // Colour the node for recursion
	       if (Show[D.TargetPkg()->ID] <= DoneNR)
	       {
		  /* If a conflicts does not meet anything in the database
		     then show the relation but do not recurse */
		  if (Hit == false && 
		      (D->Type == pkgCache::Dep::Conflicts ||
		       D->Type == pkgCache::Dep::Obsoletes))
		  {
		     if (Show[D.TargetPkg()->ID] == None && 
			 Show[D.TargetPkg()->ID] != ToShow)
			Show[D.TargetPkg()->ID] = ToShowNR;
		  }		  
		  else
		  {
		     if (GivenOnly == true && Show[D.TargetPkg()->ID] != ToShow)
			Show[D.TargetPkg()->ID] = ToShowNR;
		     else
			Show[D.TargetPkg()->ID] = ToShow;
		  }
	       }
	       
	       // Edge colour
	       switch(D->Type)
	       {
		  case pkgCache::Dep::Conflicts:
		  case pkgCache::Dep::Obsoletes:
		  printf("[color=springgreen];\n");
		  break;
		  
		  case pkgCache::Dep::PreDepends:
		  printf("[color=blue];\n");
		  break;
		  
		  default:
		  printf(";\n");
		  break;
	       }	       
	    }	    
	 }
      }
   }   
   
   /* Draw the box colours after the fact since we can not tell what colour
      they should be until everything is finished drawing */
   for (pkgCache::PkgIterator Pkg = Cache.PkgBegin(); Pkg.end() == false; Pkg++)
   {
      if (Show[Pkg->ID] < DoneNR)
	 continue;
      
      // Orange box for early recursion stoppage
      if (Show[Pkg->ID] == DoneNR)
	 printf("\"%s\" [color=orange,shape=%s];\n",Pkg.Name(),
		Shapes[ShapeMap[Pkg->ID]]);
      else
	 printf("\"%s\" [shape=%s];\n",Pkg.Name(),
		Shapes[ShapeMap[Pkg->ID]]);
   }
   
   printf("}\n");
   return true;
}
									/*}}}*/
// DoAdd - Perform an adding operation					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoAdd(CommandLine &CmdL)
{
   return _error->Error("Unimplemented");
#if 0   
   // Make sure there is at least one argument
   if (CmdL.FileSize() <= 1)
      return _error->Error("You must give at least one file name");
   
   // Open the cache
   FileFd CacheF(_config->FindFile("Dir::Cache::pkgcache"),FileFd::WriteAny);
   if (_error->PendingError() == true)
      return false;
   
   DynamicMMap Map(CacheF,MMap::Public);
   if (_error->PendingError() == true)
      return false;

   OpTextProgress Progress(*_config);
   pkgCacheGenerator Gen(Map,Progress);
   if (_error->PendingError() == true)
      return false;

   unsigned long Length = CmdL.FileSize() - 1;
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      Progress.OverallProgress(I - CmdL.FileList,Length,1,"Generating cache");
      Progress.SubProgress(Length);

      // Do the merge
      FileFd TagF(*I,FileFd::ReadOnly);
      debListParser Parser(TagF);
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",*I);
      
      if (Gen.SelectFile(*I,"") == false)
	 return _error->Error("Problem with SelectFile");
	 
      if (Gen.MergeList(Parser) == false)
	 return _error->Error("Problem with MergeList");
   }

   Progress.Done();
   GCache = &Gen.GetCache();
   Stats(CmdL);
   
   return true;
#endif   
}
									/*}}}*/
// DisplayRecord - Displays the complete record for the package		/*{{{*/
// ---------------------------------------------------------------------
/* This displays the package record from the proper package index file. 
   It is not used by DumpAvail for performance reasons. */
bool DisplayRecord(pkgCache::VerIterator V)
{
   // Find an appropriate file
   pkgCache::VerFileIterator Vf = V.FileList();
   for (; Vf.end() == false; Vf++)
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) == 0)
	 break;
   if (Vf.end() == true)
      Vf = V.FileList();
      
   // Check and load the package list file
   pkgCache::PkgFileIterator I = Vf.File();
   if (I.IsOk() == false)
      return _error->Error(_("Package file %s is out of sync."),I.FileName());
   
   FileFd PkgF(I.FileName(),FileFd::ReadOnly);
   if (_error->PendingError() == true)
      return false;
   
   // Read the record
   unsigned char *Buffer = new unsigned char[GCache->HeaderP->MaxVerFileSize+1];
   Buffer[V.FileList()->Size] = '\n';
   if (PkgF.Seek(V.FileList()->Offset) == false ||
       PkgF.Read(Buffer,V.FileList()->Size) == false)
   {
      delete [] Buffer;
      return false;
   }

   // Get a pointer to start of Description field
   const unsigned char *DescP = (unsigned char*)strstr((char*)Buffer, "Description:");

   // Write all but Description
   if (fwrite(Buffer,1,DescP - Buffer,stdout) < (size_t)(DescP - Buffer))
   {
      delete [] Buffer;
      return false;
   }

   // Show the right description
   pkgRecords Recs(*GCache);
   pkgCache::DescIterator Desc = V.TranslatedDescription();
   pkgRecords::Parser &P = Recs.Lookup(Desc.FileList());
   cout << "Description" << ( (strcmp(Desc.LanguageCode(),"") != 0) ? "-" : "" ) << Desc.LanguageCode() << ": " << P.LongDesc();

   // Find the first field after the description (if there is any)
   for(DescP++;DescP != &Buffer[V.FileList()->Size];DescP++) 
   {
      if(*DescP == '\n' && *(DescP+1) != ' ') 
      {
	 // write the rest of the buffer
	 const unsigned char *end=&Buffer[V.FileList()->Size];
	 if (fwrite(DescP,1,end-DescP,stdout) < (size_t)(end-DescP)) 
	 {
	    delete [] Buffer;
	    return false;
	 }

	 break;
      }
   }
   // write a final newline (after the description)
   cout<<endl;
   delete [] Buffer;

   return true;
}
									/*}}}*/
// Search - Perform a search						/*{{{*/
// ---------------------------------------------------------------------
/* This searches the package names and package descriptions for a pattern */
struct ExDescFile
{
   pkgCache::DescFile *Df;
   bool NameMatch;
};

bool Search(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   bool ShowFull = _config->FindB("APT::Cache::ShowFull",false);
   bool NamesOnly = _config->FindB("APT::Cache::NamesOnly",false);
   unsigned NumPatterns = CmdL.FileSize() -1;
   
   pkgDepCache::Policy Plcy;
   
   // Make sure there is at least one argument
   if (NumPatterns < 1)
      return _error->Error(_("You must give exactly one pattern"));
   
   // Compile the regex pattern
   regex_t *Patterns = new regex_t[NumPatterns];
   memset(Patterns,0,sizeof(*Patterns)*NumPatterns);
   for (unsigned I = 0; I != NumPatterns; I++)
   {
      if (regcomp(&Patterns[I],CmdL.FileList[I+1],REG_EXTENDED | REG_ICASE | 
		  REG_NOSUB) != 0)
      {
	 for (; I != 0; I--)
	    regfree(&Patterns[I]);
	 return _error->Error("Regex compilation error");
      }      
   }
   
   // Create the text record parser
   pkgRecords Recs(Cache);
   if (_error->PendingError() == true)
   {
      for (unsigned I = 0; I != NumPatterns; I++)
	 regfree(&Patterns[I]);
      return false;
   }
   
   ExDescFile *DFList = new ExDescFile[Cache.HeaderP->PackageCount+1];
   memset(DFList,0,sizeof(*DFList)*Cache.HeaderP->PackageCount+1);

   // Map versions that we want to write out onto the VerList array.
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      DFList[P->ID].NameMatch = NumPatterns != 0;
      for (unsigned I = 0; I != NumPatterns; I++)
      {
	 if (regexec(&Patterns[I],P.Name(),0,0,0) == 0)
	    DFList[P->ID].NameMatch &= true;
	 else
	    DFList[P->ID].NameMatch = false;
      }
        
      // Doing names only, drop any that dont match..
      if (NamesOnly == true && DFList[P->ID].NameMatch == false)
	 continue;
	 
      // Find the proper version to use. 
      pkgCache::VerIterator V = Plcy.GetCandidateVer(P);
      if (V.end() == false)
	 DFList[P->ID].Df = V.DescriptionList().FileList();
   }
      
   // Include all the packages that provide matching names too
   for (pkgCache::PkgIterator P = Cache.PkgBegin(); P.end() == false; P++)
   {
      if (DFList[P->ID].NameMatch == false)
	 continue;

      for (pkgCache::PrvIterator Prv = P.ProvidesList() ; Prv.end() == false; Prv++)
      {
	 pkgCache::VerIterator V = Plcy.GetCandidateVer(Prv.OwnerPkg());
	 if (V.end() == false)
	 {
	    DFList[Prv.OwnerPkg()->ID].Df = V.DescriptionList().FileList();
	    DFList[Prv.OwnerPkg()->ID].NameMatch = true;
	 }
      }
   }
   
   LocalitySort(&DFList->Df,Cache.HeaderP->PackageCount,sizeof(*DFList));

   // Iterate over all the version records and check them
   for (ExDescFile *J = DFList; J->Df != 0; J++)
   {
      pkgRecords::Parser &P = Recs.Lookup(pkgCache::DescFileIterator(Cache,J->Df));

      bool Match = true;
      if (J->NameMatch == false)
      {
	 string LongDesc = P.LongDesc();
	 Match = NumPatterns != 0;
	 for (unsigned I = 0; I != NumPatterns; I++)
	 {
	    if (regexec(&Patterns[I],LongDesc.c_str(),0,0,0) == 0)
	       Match &= true;
	    else
	       Match = false;
	 }
      }
      
      if (Match == true)
      {
	 if (ShowFull == true)
	 {
	    const char *Start;
	    const char *End;
	    P.GetRec(Start,End);
	    fwrite(Start,End-Start,1,stdout);
	    putc('\n',stdout);
	 }	 
	 else
	    printf("%s - %s\n",P.Name().c_str(),P.ShortDesc().c_str());
      }
   }
   
   delete [] DFList;
   for (unsigned I = 0; I != NumPatterns; I++)
      regfree(&Patterns[I]);
   if (ferror(stdout))
       return _error->Error("Write to stdout failed");
   return true;
}
									/*}}}*/
// ShowPackage - Dump the package record to the screen			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowPackage(CommandLine &CmdL)
{   
   pkgCache &Cache = *GCache;
   pkgDepCache::Policy Plcy;

   unsigned found = 0;
   
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }

      ++found;

      // Find the proper version to use.
      if (_config->FindB("APT::Cache::AllVersions","true") == true)
      {
	 pkgCache::VerIterator V;
	 for (V = Pkg.VersionList(); V.end() == false; V++)
	 {
	    if (DisplayRecord(V) == false)
	       return false;
	 }
      }
      else
      {
	 pkgCache::VerIterator V = Plcy.GetCandidateVer(Pkg);
	 if (V.end() == true || V.FileList().end() == true)
	    continue;
	 if (DisplayRecord(V) == false)
	    return false;
      }      
   }

   if (found > 0)
        return true;
   return _error->Error(_("No packages found"));
}
									/*}}}*/
// ShowPkgNames - Show package names					/*{{{*/
// ---------------------------------------------------------------------
/* This does a prefix match on the first argument */
bool ShowPkgNames(CommandLine &CmdL)
{
   pkgCache &Cache = *GCache;
   pkgCache::PkgIterator I = Cache.PkgBegin();
   bool All = _config->FindB("APT::Cache::AllNames","false");
   
   if (CmdL.FileList[1] != 0)
   {
      for (;I.end() != true; I++)
      {
	 if (All == false && I->VersionList == 0)
	    continue;
	 
	 if (strncmp(I.Name(),CmdL.FileList[1],strlen(CmdL.FileList[1])) == 0)
	    cout << I.Name() << endl;
      }

      return true;
   }
   
   // Show all pkgs
   for (;I.end() != true; I++)
   {
      if (All == false && I->VersionList == 0)
	 continue;
      cout << I.Name() << endl;
   }
   
   return true;
}
									/*}}}*/
// ShowSrcPackage - Show source package records				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowSrcPackage(CommandLine &CmdL)
{
   pkgSourceList List;
   List.ReadMainList();
   
   // Create the text record parsers
   pkgSrcRecords SrcRecs(List);
   if (_error->PendingError() == true)
      return false;

   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      SrcRecs.Restart();
      
      pkgSrcRecords::Parser *Parse;
      while ((Parse = SrcRecs.Find(*I,false)) != 0)
	 cout << Parse->AsStr() << endl;;
   }      
   return true;
}
									/*}}}*/
// Policy - Show the results of the preferences file			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Policy(CommandLine &CmdL)
{
   if (SrcList == 0)
      return _error->Error("Generate must be enabled for this function");
   
   pkgCache &Cache = *GCache;
   pkgPolicy Plcy(&Cache);
   if (ReadPinFile(Plcy) == false)
      return false;
   
   // Print out all of the package files
   if (CmdL.FileList[1] == 0)
   {
      cout << _("Package files:") << endl;   
      for (pkgCache::PkgFileIterator F = Cache.FileBegin(); F.end() == false; F++)
      {
	 // Locate the associated index files so we can derive a description
	 pkgIndexFile *Indx;
	 if (SrcList->FindIndex(F,Indx) == false &&
	     _system->FindIndex(F,Indx) == false)
	    return _error->Error(_("Cache is out of sync, can't x-ref a package file"));
	 printf(_("%4i %s\n"),
		Plcy.GetPriority(F),Indx->Describe(true).c_str());
	 
	 // Print the reference information for the package
	 string Str = F.RelStr();
	 if (Str.empty() == false)
	    printf("     release %s\n",F.RelStr().c_str());
	 if (F.Site() != 0 && F.Site()[0] != 0)
	    printf("     origin %s\n",F.Site());
      }
      
      // Show any packages have explicit pins
      cout << _("Pinned packages:") << endl;
      pkgCache::PkgIterator I = Cache.PkgBegin();
      for (;I.end() != true; I++)
      {
	 if (Plcy.GetPriority(I) == 0)
	    continue;

	 // Print the package name and the version we are forcing to
	 cout << "     " << I.Name() << " -> ";
	 
	 pkgCache::VerIterator V = Plcy.GetMatch(I);
	 if (V.end() == true)
	    cout << _("(not found)") << endl;
	 else
	    cout << V.VerStr() << endl;
      }     
      
      return true;
   }
   
   // Print out detailed information for each package
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
      if (Pkg.end() == true)
      {
	 _error->Warning(_("Unable to locate package %s"),*I);
	 continue;
      }
      
      cout << Pkg.Name() << ":" << endl;
      
      // Installed version
      cout << _("  Installed: ");
      if (Pkg->CurrentVer == 0)
	 cout << _("(none)") << endl;
      else
	 cout << Pkg.CurrentVer().VerStr() << endl;
      
      // Candidate Version 
      cout << _("  Candidate: ");
      pkgCache::VerIterator V = Plcy.GetCandidateVer(Pkg);
      if (V.end() == true)
	 cout << _("(none)") << endl;
      else
	 cout << V.VerStr() << endl;

      // Pinned version
      if (Plcy.GetPriority(Pkg) != 0)
      {
	 cout << _("  Package pin: ");
	 V = Plcy.GetMatch(Pkg);
	 if (V.end() == true)
	    cout << _("(not found)") << endl;
	 else
	    cout << V.VerStr() << endl;
      }
      
      // Show the priority tables
      cout << _("  Version table:") << endl;
      for (V = Pkg.VersionList(); V.end() == false; V++)
      {
	 if (Pkg.CurrentVer() == V)
	    cout << " *** " << V.VerStr();
	 else
	    cout << "     " << V.VerStr();
	 cout << " " << Plcy.GetPriority(Pkg) << endl;
	 for (pkgCache::VerFileIterator VF = V.FileList(); VF.end() == false; VF++)
	 {
	    // Locate the associated index files so we can derive a description
	    pkgIndexFile *Indx;
	    if (SrcList->FindIndex(VF.File(),Indx) == false &&
		_system->FindIndex(VF.File(),Indx) == false)
	       return _error->Error(_("Cache is out of sync, can't x-ref a package file"));
	    printf(_("       %4i %s\n"),Plcy.GetPriority(VF.File()),
		   Indx->Describe(true).c_str());
	 }	 
      }      
   }
   
   return true;
}
									/*}}}*/
// Madison - Look a bit like katie's madison				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Madison(CommandLine &CmdL)
{
   if (SrcList == 0)
      return _error->Error("Generate must be enabled for this function");

   SrcList->ReadMainList();

   pkgCache &Cache = *GCache;

   // Create the src text record parsers and ignore errors about missing
   // deb-src lines that are generated from pkgSrcRecords::pkgSrcRecords
   pkgSrcRecords SrcRecs(*SrcList);
   if (_error->PendingError() == true)
      _error->Discard();

   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);

      if (Pkg.end() == false)
      {
         for (pkgCache::VerIterator V = Pkg.VersionList(); V.end() == false; V++)
         {
            for (pkgCache::VerFileIterator VF = V.FileList(); VF.end() == false; VF++)
            {
// This might be nice, but wouldn't uniquely identify the source -mdz
//                if (VF.File().Archive() != 0)
//                {
//                   cout << setw(10) << Pkg.Name() << " | " << setw(10) << V.VerStr() << " | "
//                        << VF.File().Archive() << endl;
//                }

               // Locate the associated index files so we can derive a description
               for (pkgSourceList::const_iterator S = SrcList->begin(); S != SrcList->end(); S++)
               {
                    vector<pkgIndexFile *> *Indexes = (*S)->GetIndexFiles();
                    for (vector<pkgIndexFile *>::const_iterator IF = Indexes->begin();
                         IF != Indexes->end(); IF++)
                    {
                         if ((*IF)->FindInCache(*(VF.File().Cache())) == VF.File())
                         {
                                   cout << setw(10) << Pkg.Name() << " | " << setw(10) << V.VerStr() << " | "
                                        << (*IF)->Describe(true) << endl;
                         }
                    }
               }
            }
         }
      }

      
      SrcRecs.Restart();
      pkgSrcRecords::Parser *SrcParser;
      while ((SrcParser = SrcRecs.Find(*I,false)) != 0)
      {
         // Maybe support Release info here too eventually
         cout << setw(10) << SrcParser->Package() << " | "
              << setw(10) << SrcParser->Version() << " | "
              << SrcParser->Index().Describe(true) << endl;
      }
   }

   return true;
}

									/*}}}*/
// GenCaches - Call the main cache generator				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool GenCaches(CommandLine &Cmd)
{
   OpTextProgress Progress(*_config);
   
   pkgSourceList List;
   if (List.ReadMainList() == false)
      return false;   
   return pkgMakeStatusCache(List,Progress);
}
									/*}}}*/
// ShowHelp - Show a help screen					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool ShowHelp(CommandLine &Cmd)
{
   ioprintf(cout,_("%s %s for %s compiled on %s %s\n"),PACKAGE,VERSION,
	    COMMON_ARCH,__DATE__,__TIME__);
   
   if (_config->FindB("version") == true)
     return true;

   cout << 
    _("Usage: apt-cache [options] command\n"
      "       apt-cache [options] add file1 [file2 ...]\n"
      "       apt-cache [options] showpkg pkg1 [pkg2 ...]\n"
      "       apt-cache [options] showsrc pkg1 [pkg2 ...]\n"
      "\n"
      "apt-cache is a low-level tool used to manipulate APT's binary\n"
      "cache files, and query information from them\n"
      "\n"
      "Commands:\n"
      "   add - Add a package file to the source cache\n"
      "   gencaches - Build both the package and source cache\n"
      "   showpkg - Show some general information for a single package\n"
      "   showsrc - Show source records\n"
      "   stats - Show some basic statistics\n"
      "   dump - Show the entire file in a terse form\n"
      "   dumpavail - Print an available file to stdout\n"
      "   unmet - Show unmet dependencies\n"
      "   search - Search the package list for a regex pattern\n"
      "   show - Show a readable record for the package\n"
      "   depends - Show raw dependency information for a package\n"
      "   rdepends - Show reverse dependency information for a package\n"
      "   pkgnames - List the names of all packages in the system\n"
      "   dotty - Generate package graphs for GraphViz\n"
      "   xvcg - Generate package graphs for xvcg\n"
      "   policy - Show policy settings\n"
      "\n"
      "Options:\n"
      "  -h   This help text.\n"
      "  -p=? The package cache.\n"
      "  -s=? The source cache.\n"
      "  -q   Disable progress indicator.\n"
      "  -i   Show only important deps for the unmet command.\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "See the apt-cache(8) and apt.conf(5) manual pages for more information.\n");
   return true;
}
									/*}}}*/
// CacheInitialize - Initialize things for apt-cache			/*{{{*/
// ---------------------------------------------------------------------
/* */
void CacheInitialize()
{
   _config->Set("quiet",0);
   _config->Set("help",false);
}
									/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'p',"pkg-cache","Dir::Cache::pkgcache",CommandLine::HasArg},
      {'s',"src-cache","Dir::Cache::srcpkgcache",CommandLine::HasArg},
      {'q',"quiet","quiet",CommandLine::IntLevel},
      {'i',"important","APT::Cache::Important",0},
      {'f',"full","APT::Cache::ShowFull",0},
      {'g',"generate","APT::Cache::Generate",0},
      {'a',"all-versions","APT::Cache::AllVersions",0},
      {'n',"names-only","APT::Cache::NamesOnly",0},
      {0,"all-names","APT::Cache::AllNames",0},
      {0,"recurse","APT::Cache::RecurseDepends",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,"installed","APT::Cache::Installed",0},
      {0,0,0,0}};
   CommandLine::Dispatch CmdsA[] = {{"help",&ShowHelp},
                                    {"add",&DoAdd},
                                    {"gencaches",&GenCaches},
                                    {"showsrc",&ShowSrcPackage},
                                    {0,0}};
   CommandLine::Dispatch CmdsB[] = {{"showpkg",&DumpPackage},
                                    {"stats",&Stats},
                                    {"dump",&Dump},
                                    {"dumpavail",&DumpAvail},
                                    {"unmet",&UnMet},
                                    {"search",&Search},
                                    {"depends",&Depends},
                                    {"rdepends",&RDepends},
                                    {"dotty",&Dotty},
                                    {"xvcg",&XVcg},
                                    {"show",&ShowPackage},
                                    {"pkgnames",&ShowPkgNames},
                                    {"policy",&Policy},
                                    {"madison",&Madison},
                                    {0,0}};

   CacheInitialize();

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
   {
      ShowHelp(CmdL);
      return 0;
   }
   
   // Deal with stdout not being a tty
   if (isatty(STDOUT_FILENO) && _config->FindI("quiet",0) < 1)
      _config->Set("quiet","1");

   if (CmdL.DispatchArg(CmdsA,false) == false && _error->PendingError() == false)
   { 
      MMap *Map = 0;
      if (_config->FindB("APT::Cache::Generate",true) == false)
      {
	 Map = new MMap(*new FileFd(_config->FindFile("Dir::Cache::pkgcache"),
				    FileFd::ReadOnly),MMap::Public|MMap::ReadOnly);
      }
      else
      {
	 // Open the cache file
	 SrcList = new pkgSourceList;
	 SrcList->ReadMainList();

	 // Generate it and map it
	 OpProgress Prog;
	 pkgMakeStatusCache(*SrcList,Prog,&Map,true);
      }
      
      if (_error->PendingError() == false)
      {
	 pkgCache Cache(Map);   
	 GCache = &Cache;
	 if (_error->PendingError() == false)
	    CmdL.DispatchArg(CmdsB);
      }
      delete Map;
   }
   
   // Print any errors or warnings found during parsing
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }
          
   return 0;
}

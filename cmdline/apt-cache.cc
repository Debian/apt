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
#include<config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/init.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/version.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-cmndline.h>
#include <apt-private/private-depends.h>
#include <apt-private/private-show.h>
#include <apt-private/private-search.h>
#include <apt-private/private-unmet.h>
#include <apt-private/private-main.h>

#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// DumpPackage - Show a dump of a package record			/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool DumpPackage(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   APT::CacheSetHelper helper(true, GlobalError::NOTICE);
   APT::PackageList pkgset = APT::PackageList::FromCommandLine(CacheFile, CmdL.FileList + 1, helper);

   for (APT::PackageList::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
   {
      cout << "Package: " << Pkg.FullName(true) << endl;
      cout << "Versions: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; ++Cur)
      {
	 cout << Cur.VerStr();
	 for (pkgCache::VerFileIterator Vf = Cur.FileList(); Vf.end() == false; ++Vf)
	    cout << " (" << Vf.File().FileName() << ")";
	 cout << endl;
	 for (pkgCache::DescIterator D = Cur.DescriptionList(); D.end() == false; ++D)
	 {
	    cout << " Description Language: " << D.LanguageCode() << endl
		 << "                 File: " << D.FileList().File().FileName() << endl
		 << "                  MD5: " << D.md5() << endl;
	 }
	 cout << endl;
      }
      
      cout << endl;
      
      cout << "Reverse Depends: " << endl;
      for (pkgCache::DepIterator D = Pkg.RevDependsList(); D.end() != true; ++D)
      {
	 cout << "  " << D.ParentPkg().FullName(true) << ',' << D.TargetPkg().FullName(true);
	 if (D->Version != 0)
	    cout << ' ' << DeNull(D.TargetVer()) << endl;
	 else
	    cout << endl;
      }
      
      cout << "Dependencies: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; ++Cur)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::DepIterator Dep = Cur.DependsList(); Dep.end() != true; ++Dep)
	    cout << Dep.TargetPkg().FullName(true) << " (" << (int)Dep->CompareOp << " " << DeNull(Dep.TargetVer()) << ") ";
	 cout << endl;
      }      

      cout << "Provides: " << endl;
      for (pkgCache::VerIterator Cur = Pkg.VersionList(); Cur.end() != true; ++Cur)
      {
	 cout << Cur.VerStr() << " - ";
	 for (pkgCache::PrvIterator Prv = Cur.ProvidesList(); Prv.end() != true; ++Prv)
	    cout << Prv.ParentPkg().FullName(true) << " (= " << (Prv->ProvideVersion == 0 ? "" : Prv.ProvideVersion()) << ") ";
	 cout << endl;
      }
      cout << "Reverse Provides: " << endl;
      for (pkgCache::PrvIterator Prv = Pkg.ProvidesList(); Prv.end() != true; ++Prv)
	 cout << Prv.OwnerPkg().FullName(true) << " " << Prv.OwnerVer().VerStr()  << " (= " << (Prv->ProvideVersion == 0 ? "" : Prv.ProvideVersion()) << ")"<< endl;
   }

   return true;
}
									/*}}}*/
// ShowHashTableStats - Show stats about a hashtable			/*{{{*/
// ---------------------------------------------------------------------
/* */
static map_pointer_t PackageNext(pkgCache::Package const * const P) { return P->NextPackage; }
static map_pointer_t GroupNext(pkgCache::Group const * const G) { return G->Next; }
template<class T>
static void ShowHashTableStats(std::string Type,
                               T *StartP,
                               map_pointer_t *Hashtable,
                               unsigned long Size,
			       map_pointer_t(*Next)(T const * const))
{
   // hashtable stats for the HashTable
   unsigned long NumBuckets = Size;
   unsigned long UsedBuckets = 0;
   unsigned long UnusedBuckets = 0;
   unsigned long LongestBucket = 0;
   unsigned long ShortestBucket = NumBuckets;
   unsigned long Entries = 0;
   for (unsigned int i=0; i < NumBuckets; ++i)
   {
      T *P = StartP + Hashtable[i];
      if(P == 0 || P == StartP)
      {
         ++UnusedBuckets;
         continue;
      }
      ++UsedBuckets;
      unsigned long ThisBucketSize = 0;
      for (; P != StartP; P = StartP + Next(P))
         ++ThisBucketSize;
      Entries += ThisBucketSize;
      LongestBucket = std::max(ThisBucketSize, LongestBucket);
      ShortestBucket = std::min(ThisBucketSize, ShortestBucket);
   }
   cout << "Total buckets in " << Type << ": " << NumBuckets << std::endl;
   cout << "  Unused: " << UnusedBuckets << std::endl;
   cout << "  Used: " << UsedBuckets  << std::endl;
   cout << "  Utilization: " << 100.0 * UsedBuckets/NumBuckets << "%" << std::endl;
   cout << "  Average entries: " << Entries/(double)UsedBuckets << std::endl;
   cout << "  Longest: " << LongestBucket << std::endl;
   cout << "  Shortest: " << ShortestBucket << std::endl;
}
									/*}}}*/
// Stats - Dump some nice statistics					/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool Stats(CommandLine &CmdL)
{
   if (CmdL.FileSize() > 1) {
      _error->Error(_("apt-cache stats does not take any arguments"));
      return false;
   }

   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();

   if (unlikely(Cache == NULL))
      return false;

   cout << _("Total package names: ") << Cache->Head().GroupCount << " (" <<
      SizeToStr(Cache->Head().GroupCount*Cache->Head().GroupSz) << ')' << endl
        << _("Total package structures: ") << Cache->Head().PackageCount << " (" <<
      SizeToStr(Cache->Head().PackageCount*Cache->Head().PackageSz) << ')' << endl;

   int Normal = 0;
   int Virtual = 0;
   int NVirt = 0;
   int DVirt = 0;
   int Missing = 0;
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; ++I)
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

   cout << _("Total distinct versions: ") << Cache->Head().VersionCount << " (" <<
      SizeToStr(Cache->Head().VersionCount*Cache->Head().VersionSz) << ')' << endl;
   cout << _("Total distinct descriptions: ") << Cache->Head().DescriptionCount << " (" <<
      SizeToStr(Cache->Head().DescriptionCount*Cache->Head().DescriptionSz) << ')' << endl;
   cout << _("Total dependencies: ") << Cache->Head().DependsCount << "/" << Cache->Head().DependsDataCount << " (" <<
      SizeToStr((Cache->Head().DependsCount*Cache->Head().DependencySz) +
	    (Cache->Head().DependsDataCount*Cache->Head().DependencyDataSz)) << ')' << endl;
   cout << _("Total ver/file relations: ") << Cache->Head().VerFileCount << " (" <<
      SizeToStr(Cache->Head().VerFileCount*Cache->Head().VerFileSz) << ')' << endl;
   cout << _("Total Desc/File relations: ") << Cache->Head().DescFileCount << " (" <<
      SizeToStr(Cache->Head().DescFileCount*Cache->Head().DescFileSz) << ')' << endl;
   cout << _("Total Provides mappings: ") << Cache->Head().ProvidesCount << " (" <<
      SizeToStr(Cache->Head().ProvidesCount*Cache->Head().ProvidesSz) << ')' << endl;

   // String list stats
   std::set<map_stringitem_t> stritems;
   for (pkgCache::GrpIterator G = Cache->GrpBegin(); G.end() == false; ++G)
      stritems.insert(G->Name);
   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
   {
      stritems.insert(P->Arch);
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
      {
	 if (V->VerStr != 0)
	    stritems.insert(V->VerStr);
	 if (V->Section != 0)
	    stritems.insert(V->Section);
	 stritems.insert(V->SourcePkgName);
	 stritems.insert(V->SourceVerStr);
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false; ++D)
	 {
	    if (D->Version != 0)
	       stritems.insert(D->Version);
	 }
	 for (pkgCache::DescIterator D = V.DescriptionList(); D.end() == false; ++D)
	 {
	    stritems.insert(D->md5sum);
	    stritems.insert(D->language_code);
	 }
      }
      for (pkgCache::PrvIterator Prv = P.ProvidesList(); Prv.end() == false; ++Prv)
      {
	 if (Prv->ProvideVersion != 0)
	    stritems.insert(Prv->ProvideVersion);
      }
   }
   for (pkgCache::RlsFileIterator F = Cache->RlsFileBegin(); F != Cache->RlsFileEnd(); ++F)
   {
      stritems.insert(F->FileName);
      stritems.insert(F->Archive);
      stritems.insert(F->Codename);
      stritems.insert(F->Version);
      stritems.insert(F->Origin);
      stritems.insert(F->Label);
      stritems.insert(F->Site);
   }
   for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F != Cache->FileEnd(); ++F)
   {
      stritems.insert(F->FileName);
      stritems.insert(F->Architecture);
      stritems.insert(F->Component);
      stritems.insert(F->IndexType);
   }

   unsigned long Size = 0;
   for (std::set<map_stringitem_t>::const_iterator i = stritems.begin(); i != stritems.end(); ++i)
      Size += strlen(Cache->StrP + *i) + 1;
   cout << _("Total globbed strings: ") << stritems.size() << " (" << SizeToStr(Size) << ')' << endl;
   stritems.clear();

   unsigned long Slack = 0;
   for (int I = 0; I != 7; I++)
      Slack += Cache->Head().Pools[I].ItemSize*Cache->Head().Pools[I].Count;
   cout << _("Total slack space: ") << SizeToStr(Slack) << endl;

   unsigned long Total = 0;
#define APT_CACHESIZE(X,Y) (Cache->Head().X * Cache->Head().Y)
   Total = Slack + Size +
      APT_CACHESIZE(GroupCount, GroupSz) +
      APT_CACHESIZE(PackageCount, PackageSz) +
      APT_CACHESIZE(VersionCount, VersionSz) +
      APT_CACHESIZE(DescriptionCount, DescriptionSz) +
      APT_CACHESIZE(DependsCount, DependencySz) +
      APT_CACHESIZE(DependsDataCount, DependencyDataSz) +
      APT_CACHESIZE(ReleaseFileCount, ReleaseFileSz) +
      APT_CACHESIZE(PackageFileCount, PackageFileSz) +
      APT_CACHESIZE(VerFileCount, VerFileSz) +
      APT_CACHESIZE(DescFileCount, DescFileSz) +
      APT_CACHESIZE(ProvidesCount, ProvidesSz) +
      (2 * Cache->Head().GetHashTableSize() * sizeof(map_id_t));
   cout << _("Total space accounted for: ") << SizeToStr(Total) << endl;
#undef APT_CACHESIZE

   // hashtable stats
   ShowHashTableStats<pkgCache::Package>("PkgHashTable", Cache->PkgP, Cache->Head().PkgHashTableP(), Cache->Head().GetHashTableSize(), PackageNext);
   ShowHashTableStats<pkgCache::Group>("GrpHashTable", Cache->GrpP, Cache->Head().GrpHashTableP(), Cache->Head().GetHashTableSize(), GroupNext);

   return true;
}
									/*}}}*/
// Dump - show everything						/*{{{*/
// ---------------------------------------------------------------------
/* This is worthless except fer debugging things */
static bool Dump(CommandLine &)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

   std::cout << "Using Versioning System: " << Cache->VS->Label << std::endl;
   
   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
   {
      std::cout << "Package: " << P.FullName(true) << std::endl;
      for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
      {
	 std::cout << " Version: " << V.VerStr() << std::endl;
	 std::cout << "     File: " << V.FileList().File().FileName() << std::endl;
	 for (pkgCache::DepIterator D = V.DependsList(); D.end() == false; ++D)
	    std::cout << "  Depends: " << D.TargetPkg().FullName(true) << ' ' << 
	                     DeNull(D.TargetVer()) << std::endl;
	 for (pkgCache::DescIterator D = V.DescriptionList(); D.end() == false; ++D)
	 {
	    std::cout << " Description Language: " << D.LanguageCode() << std::endl
		 << "                 File: " << D.FileList().File().FileName() << std::endl
		 << "                  MD5: " << D.md5() << std::endl;
	 } 
      }      
   }

   for (pkgCache::PkgFileIterator F = Cache->FileBegin(); F.end() == false; ++F)
   {
      std::cout << "File: " << F.FileName() << std::endl;
      std::cout << " Type: " << F.IndexType() << std::endl;
      std::cout << " Size: " << F->Size << std::endl;
      std::cout << " ID: " << F->ID << std::endl;
      std::cout << " Flags: " << F->Flags << std::endl;
      std::cout << " Time: " << TimeRFC1123(F->mtime, true) << std::endl;
      std::cout << " Archive: " << DeNull(F.Archive()) << std::endl;
      std::cout << " Component: " << DeNull(F.Component()) << std::endl;
      std::cout << " Version: " << DeNull(F.Version()) << std::endl;
      std::cout << " Origin: " << DeNull(F.Origin()) << std::endl;
      std::cout << " Site: " << DeNull(F.Site()) << std::endl;
      std::cout << " Label: " << DeNull(F.Label()) << std::endl;
      std::cout << " Architecture: " << DeNull(F.Architecture()) << std::endl;
   }

   return true;
}
									/*}}}*/
// DumpAvail - Print out the available list				/*{{{*/
// ---------------------------------------------------------------------
/* This is needed to make dpkg --merge happy.. I spent a bit of time to 
   make this run really fast, perhaps I went a little overboard.. */
static bool DumpAvail(CommandLine &)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL || CacheFile.BuildPolicy() == false))
      return false;

   unsigned long Count = Cache->HeaderP->PackageCount+1;
   pkgCache::VerFile **VFList = new pkgCache::VerFile *[Count];
   memset(VFList,0,sizeof(*VFList)*Count);
   
   // Map versions that we want to write out onto the VerList array.
   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
   {    
      if (P->VersionList == 0)
	 continue;
      
      /* Find the proper version to use. If the policy says there are no
         possible selections we return the installed version, if available..
       	 This prevents dselect from making it obsolete. */
      pkgCache::VerIterator V = CacheFile.GetPolicy()->GetCandidateVer(P);
      if (V.end() == true)
      {
	 if (P->CurrentVer == 0)
	    continue;
	 V = P.CurrentVer();
      }
      
      pkgCache::VerFileIterator VF = V.FileList();
      for (; VF.end() == false ; ++VF)
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
	 for (pkgCache::VerIterator Cur = P.VersionList(); Cur.end() != true; ++Cur)
	 {
	    for (VF = Cur.FileList(); VF.end() == false; ++VF)
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

   std::vector<pkgTagSection::Tag> RW;
   RW.push_back(pkgTagSection::Tag::Remove("Status"));
   RW.push_back(pkgTagSection::Tag::Remove("Config-Version"));
   FileFd stdoutfd;
   stdoutfd.OpenDescriptor(STDOUT_FILENO, FileFd::WriteOnly, false);

   // Iterate over all the package files and write them out.
   char *Buffer = new char[Cache->HeaderP->MaxVerFileSize+10];
   for (pkgCache::VerFile **J = VFList; *J != 0;)
   {
      pkgCache::PkgFileIterator File(*Cache,(*J)->File + Cache->PkgFileP);
      if (File.IsOk() == false)
      {
	 _error->Error(_("Package file %s is out of sync."),File.FileName());
	 break;
      }

      FileFd PkgF(File.FileName(),FileFd::ReadOnly, FileFd::Extension);
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
	 if ((*J)->File + Cache->PkgFileP != File)
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
	    if (Tags.Scan(Buffer+Jitter,VF.Size+1) == false ||
		Tags.Write(stdoutfd, NULL, RW) == false ||
		stdoutfd.Write("\n", 1) == false)
	    {
	       _error->Error("Internal Error, Unable to parse a package record");
	       break;
	    }
	 }
	 else
	 {
	    if (stdoutfd.Write(Buffer + Jitter, VF.Size + 1) == false)
	       break;
	 }

	 Pos = VF.Offset + VF.Size;
      }

      if (_error->PendingError() == true)
         break;
   }

   delete [] Buffer;
   delete [] VFList;
   return !_error->PendingError();
}
									/*}}}*/
// xvcg - Generate a graph for xvcg					/*{{{*/
// ---------------------------------------------------------------------
// Code contributed from Junichi Uekawa <dancer@debian.org> on 20 June 2002.

static bool XVcg(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

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
   unsigned char *Show = new unsigned char[Cache->Head().PackageCount];
   unsigned char *Flags = new unsigned char[Cache->Head().PackageCount];
   unsigned char *ShapeMap = new unsigned char[Cache->Head().PackageCount];
   
   // Show everything if no arguments given
   if (CmdL.FileList[1] == 0)
      for (unsigned long I = 0; I != Cache->Head().PackageCount; I++)
	 Show[I] = ToShow;
   else
      for (unsigned long I = 0; I != Cache->Head().PackageCount; I++)
	 Show[I] = None;
   memset(Flags,0,sizeof(*Flags)*Cache->Head().PackageCount);
   
   // Map the shapes
   for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
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
   APT::CacheSetHelper helper(true, GlobalError::NOTICE);
   std::list<APT::CacheSetHelper::PkgModifier> mods;
   mods.push_back(APT::CacheSetHelper::PkgModifier(0, ",", APT::PackageSet::Modifier::POSTFIX));
   mods.push_back(APT::CacheSetHelper::PkgModifier(1, "^", APT::PackageSet::Modifier::POSTFIX));
   std::map<unsigned short, APT::PackageSet> pkgsets =
		APT::PackageSet::GroupedFromCommandLine(CacheFile, CmdL.FileList + 1, mods, 0, helper);

   for (APT::PackageSet::const_iterator Pkg = pkgsets[0].begin();
	Pkg != pkgsets[0].end(); ++Pkg)
      Show[Pkg->ID] = ToShow;
   for (APT::PackageSet::const_iterator Pkg = pkgsets[1].begin();
	Pkg != pkgsets[1].end(); ++Pkg)
   {
      Show[Pkg->ID] = ToShow;
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
      for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
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
	 for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; ++D)
	 {
	    // See if anything can meet this dep
	    // Walk along the actual package providing versions
	    bool Hit = false;
	    pkgCache::PkgIterator DPkg = D.TargetPkg();
	    for (pkgCache::VerIterator I = DPkg.VersionList();
		      I.end() == false && Hit == false; ++I)
	    {
	       if (Cache->VS->CheckDep(I.VerStr(),D->CompareOp,D.TargetVer()) == true)
		  Hit = true;
	    }
	    
	    // Follow all provides
	    for (pkgCache::PrvIterator I = DPkg.ProvidesList(); 
		      I.end() == false && Hit == false; ++I)
	    {
	       if (Cache->VS->CheckDep(I.ProvideVersion(),D->CompareOp,D.TargetVer()) == false)
		  Hit = true;
	    }
	    

	    // Only graph critical deps	    
	    if (D.IsCritical() == true)
	    {
	       printf ("edge: { sourcename: \"%s\" targetname: \"%s\" class: 2 ",Pkg.FullName(true).c_str(), D.TargetPkg().FullName(true).c_str() );
	       
	       // Colour the node for recursion
	       if (Show[D.TargetPkg()->ID] <= DoneNR)
	       {
		  /* If a conflicts does not meet anything in the database
		     then show the relation but do not recurse */
		  if (Hit == false && D.IsNegative() == true)
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
   for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      if (Show[Pkg->ID] < DoneNR)
	 continue;

      if (Show[Pkg->ID] == DoneNR)
	 printf("node: { title: \"%s\" label: \"%s\" color: orange shape: %s }\n", Pkg.FullName(true).c_str(), Pkg.FullName(true).c_str(),
		Shapes[ShapeMap[Pkg->ID]]);
      else
	printf("node: { title: \"%s\" label: \"%s\" shape: %s }\n", Pkg.FullName(true).c_str(), Pkg.FullName(true).c_str(),
		Shapes[ShapeMap[Pkg->ID]]);
      
   }

   delete[] Show;
   delete[] Flags;
   delete[] ShapeMap;

   printf("}\n");
   return true;
}
									/*}}}*/
// Dotty - Generate a graph for Dotty					/*{{{*/
// ---------------------------------------------------------------------
/* Dotty is the graphvis program for generating graphs. It is a fairly
   simple queuing algorithm that just writes dependencies and nodes. 
   http://www.research.att.com/sw/tools/graphviz/ */
static bool Dotty(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == NULL))
      return false;

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
   unsigned char *Show = new unsigned char[Cache->Head().PackageCount];
   unsigned char *Flags = new unsigned char[Cache->Head().PackageCount];
   unsigned char *ShapeMap = new unsigned char[Cache->Head().PackageCount];
   
   // Show everything if no arguments given
   if (CmdL.FileList[1] == 0)
      for (unsigned long I = 0; I != Cache->Head().PackageCount; I++)
	 Show[I] = ToShow;
   else
      for (unsigned long I = 0; I != Cache->Head().PackageCount; I++)
	 Show[I] = None;
   memset(Flags,0,sizeof(*Flags)*Cache->Head().PackageCount);
   
   // Map the shapes
   for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
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
   APT::CacheSetHelper helper(true, GlobalError::NOTICE);
   std::list<APT::CacheSetHelper::PkgModifier> mods;
   mods.push_back(APT::CacheSetHelper::PkgModifier(0, ",", APT::PackageSet::Modifier::POSTFIX));
   mods.push_back(APT::CacheSetHelper::PkgModifier(1, "^", APT::PackageSet::Modifier::POSTFIX));
   std::map<unsigned short, APT::PackageSet> pkgsets =
		APT::PackageSet::GroupedFromCommandLine(CacheFile, CmdL.FileList + 1, mods, 0, helper);

   for (APT::PackageSet::const_iterator Pkg = pkgsets[0].begin();
	Pkg != pkgsets[0].end(); ++Pkg)
      Show[Pkg->ID] = ToShow;
   for (APT::PackageSet::const_iterator Pkg = pkgsets[1].begin();
	Pkg != pkgsets[1].end(); ++Pkg)
   {
      Show[Pkg->ID] = ToShow;
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
      for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
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
	 for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; ++D)
	 {
	    // See if anything can meet this dep
	    // Walk along the actual package providing versions
	    bool Hit = false;
	    pkgCache::PkgIterator DPkg = D.TargetPkg();
	    for (pkgCache::VerIterator I = DPkg.VersionList();
		      I.end() == false && Hit == false; ++I)
	    {
	       if (Cache->VS->CheckDep(I.VerStr(),D->CompareOp,D.TargetVer()) == true)
		  Hit = true;
	    }
	    
	    // Follow all provides
	    for (pkgCache::PrvIterator I = DPkg.ProvidesList(); 
		      I.end() == false && Hit == false; ++I)
	    {
	       if (Cache->VS->CheckDep(I.ProvideVersion(),D->CompareOp,D.TargetVer()) == false)
		  Hit = true;
	    }
	    
	    // Only graph critical deps	    
	    if (D.IsCritical() == true)
	    {
	       printf("\"%s\" -> \"%s\"",Pkg.FullName(true).c_str(),D.TargetPkg().FullName(true).c_str());
	       
	       // Colour the node for recursion
	       if (Show[D.TargetPkg()->ID] <= DoneNR)
	       {
		  /* If a conflicts does not meet anything in the database
		     then show the relation but do not recurse */
		  if (Hit == false && D.IsNegative() == true)
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
		  case pkgCache::Dep::DpkgBreaks:
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
   for (pkgCache::PkgIterator Pkg = Cache->PkgBegin(); Pkg.end() == false; ++Pkg)
   {
      if (Show[Pkg->ID] < DoneNR)
	 continue;
      
      // Orange box for early recursion stoppage
      if (Show[Pkg->ID] == DoneNR)
	 printf("\"%s\" [color=orange,shape=%s];\n",Pkg.FullName(true).c_str(),
		Shapes[ShapeMap[Pkg->ID]]);
      else
	 printf("\"%s\" [shape=%s];\n",Pkg.FullName(true).c_str(),
		Shapes[ShapeMap[Pkg->ID]]);
   }
   
   printf("}\n");
   delete[] Show;
   delete[] Flags;
   delete[] ShapeMap;
   return true;
}
									/*}}}*/
/* ShowAuto - show automatically installed packages (sorted)		{{{*/
static bool ShowAuto(CommandLine &)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache *DepCache = CacheFile.GetDepCache();
   if (unlikely(Cache == NULL || DepCache == NULL))
      return false;

   std::vector<string> packages;
   packages.reserve(Cache->HeaderP->PackageCount / 3);
   
   for (pkgCache::PkgIterator P = Cache->PkgBegin(); P.end() == false; ++P)
      if ((*DepCache)[P].Flags & pkgCache::Flag::Auto)
         packages.push_back(P.Name());

    std::sort(packages.begin(), packages.end());
    
    for (vector<string>::iterator I = packages.begin(); I != packages.end(); ++I)
            cout << *I << "\n";

   _error->Notice(_("This command is deprecated. Please use 'apt-mark showauto' instead."));
   return true;
}
									/*}}}*/
// ShowPkgNames - Show package names					/*{{{*/
// ---------------------------------------------------------------------
/* This does a prefix match on the first argument */
static bool ShowPkgNames(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   if (unlikely(CacheFile.BuildCaches(NULL, false) == false))
      return false;
   pkgCache::GrpIterator I = CacheFile.GetPkgCache()->GrpBegin();
   bool const All = _config->FindB("APT::Cache::AllNames","false");

   if (CmdL.FileList[1] != 0)
   {
      for (;I.end() != true; ++I)
      {
	 if (All == false && I->FirstPackage == 0)
	    continue;
	 if (I.FindPkg("any")->VersionList == 0)
	    continue;
	 if (strncmp(I.Name(),CmdL.FileList[1],strlen(CmdL.FileList[1])) == 0)
	    cout << I.Name() << endl;
      }

      return true;
   }
   
   // Show all pkgs
   for (;I.end() != true; ++I)
   {
      if (All == false && I->FirstPackage == 0)
	 continue;
      if (I.FindPkg("any")->VersionList == 0)
	 continue;
      cout << I.Name() << endl;
   }
   
   return true;
}
									/*}}}*/
// Madison - Look a bit like katie's madison				/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool Madison(CommandLine &CmdL)
{
   pkgCacheFile CacheFile;
   pkgSourceList *SrcList = CacheFile.GetSourceList();

   if (SrcList == 0)
      return false;

   // Create the src text record parsers and ignore errors about missing
   // deb-src lines that are generated from pkgSrcRecords::pkgSrcRecords
   pkgSrcRecords SrcRecs(*SrcList);
   if (_error->PendingError() == true)
      _error->Discard();

   APT::CacheSetHelper helper(true, GlobalError::NOTICE);
   for (const char **I = CmdL.FileList + 1; *I != 0; I++)
   {
      _error->PushToStack();
      APT::PackageList pkgset = APT::PackageList::FromString(CacheFile, *I, helper);
      for (APT::PackageList::const_iterator Pkg = pkgset.begin(); Pkg != pkgset.end(); ++Pkg)
      {
         for (pkgCache::VerIterator V = Pkg.VersionList(); V.end() == false; ++V)
         {
            for (pkgCache::VerFileIterator VF = V.FileList(); VF.end() == false; ++VF)
            {
// This might be nice, but wouldn't uniquely identify the source -mdz
//                if (VF.File().Archive() != 0)
//                {
//                   cout << setw(10) << Pkg.Name() << " | " << setw(10) << V.VerStr() << " | "
//                        << VF.File().Archive() << endl;
//                }

               // Locate the associated index files so we can derive a description
               for (pkgSourceList::const_iterator S = SrcList->begin(); S != SrcList->end(); ++S)
               {
                    vector<pkgIndexFile *> *Indexes = (*S)->GetIndexFiles();
                    for (vector<pkgIndexFile *>::const_iterator IF = Indexes->begin();
                         IF != Indexes->end(); ++IF)
                    {
                         if ((*IF)->FindInCache(*(VF.File().Cache())) == VF.File())
                         {
                                   cout << setw(10) << Pkg.FullName(true) << " | " << setw(10) << V.VerStr() << " | "
                                        << (*IF)->Describe(true) << endl;
                         }
                    }
               }
            }
         }
      }

      SrcRecs.Restart();
      pkgSrcRecords::Parser *SrcParser;
      bool foundSomething = false;
      while ((SrcParser = SrcRecs.Find(*I, false)) != 0)
      {
         foundSomething = true;
         // Maybe support Release info here too eventually
         cout << setw(10) << SrcParser->Package() << " | "
              << setw(10) << SrcParser->Version() << " | "
              << SrcParser->Index().Describe(true) << endl;
      }
      if (foundSomething == true)
	 _error->RevertToStack();
      else
	 _error->MergeWithStack();
   }

   return true;
}
									/*}}}*/
// GenCaches - Call the main cache generator				/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool GenCaches(CommandLine &)
{
   OpTextProgress Progress(*_config);

   pkgCacheFile CacheFile;
   return CacheFile.BuildCaches(&Progress, true);
}
									/*}}}*/
static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
    _("Usage: apt-cache [options] command\n"
      "       apt-cache [options] show pkg1 [pkg2 ...]\n"
      "\n"
      "apt-cache queries and displays available information about installed\n"
      "and installable packages. It works exclusively on the data acquired\n"
      "into the local cache via the 'update' command of e.g. apt-get. The\n"
      "displayed information may therefore be outdated if the last update was\n"
      "too long ago, but in exchange apt-cache works independently of the\n"
      "availability of the configured sources (e.g. offline).\n");
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {"gencaches",&GenCaches, nullptr},
      {"showsrc",&ShowSrcPackage, _("Show source records")},
      {"showpkg",&DumpPackage, nullptr},
      {"stats",&Stats, nullptr},
      {"dump",&Dump, nullptr},
      {"dumpavail",&DumpAvail, nullptr},
      {"unmet",&UnMet, nullptr},
      {"search",&DoSearch, _("Search the package list for a regex pattern")},
      {"depends",&Depends, _("Show raw dependency information for a package")},
      {"rdepends",&RDepends, _("Show reverse dependency information for a package")},
      {"dotty",&Dotty, nullptr},
      {"xvcg",&XVcg, nullptr},
      {"show",&ShowPackage, _("Show a readable record for the package")},
      {"pkgnames",&ShowPkgNames, _("List the names of all packages in the system")},
      {"showauto",&ShowAuto, nullptr},
      {"policy",&Policy, _("Show policy settings")},
      {"madison",&Madison, nullptr},
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   // Parse the command line and initialize the package library
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_CACHE, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);

   InitOutput();

   if (_config->Exists("APT::Cache::Generate") == true)
      _config->Set("pkgCacheFile::Generate", _config->FindB("APT::Cache::Generate", true));

   return DispatchCommandLine(CmdL, Cmds);
}
									/*}}}*/

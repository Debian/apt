// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.53.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#define APT_COMPATIBILITY 986

#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>

#include <apt-pkg/tagfile.h>

#include <apti18n.h>

#include <vector>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <system.h>
									/*}}}*/
typedef vector<pkgIndexFile *>::iterator FileIterator;

// CacheGenerator::pkgCacheGenerator - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* We set the diry flag and make sure that is written to the disk */
pkgCacheGenerator::pkgCacheGenerator(DynamicMMap *pMap,OpProgress *Prog) :
		    Map(*pMap), Cache(pMap,false), Progress(Prog),
		    FoundFileDeps(0)
{
   CurrentFile = 0;
   memset(UniqHash,0,sizeof(UniqHash));
   
   if (_error->PendingError() == true)
      return;

   if (Map.Size() == 0)
   {
      // Setup the map interface..
      Cache.HeaderP = (pkgCache::Header *)Map.Data();
      Map.RawAllocate(sizeof(pkgCache::Header));
      Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));
      
      // Starting header
      *Cache.HeaderP = pkgCache::Header();
      Cache.HeaderP->VerSysName = Map.WriteString(_system->VS->Label);
      Cache.HeaderP->Architecture = Map.WriteString(_config->Find("APT::Architecture"));
      Cache.ReMap(); 
   }
   else
   {
      // Map directly from the existing file
      Cache.ReMap(); 
      Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));
      if (Cache.VS != _system->VS)
      {
	 _error->Error(_("Cache has an incompatible versioning system"));
	 return;
      }      
   }
   
   Cache.HeaderP->Dirty = true;
   Map.Sync(0,sizeof(pkgCache::Header));
}
									/*}}}*/
// CacheGenerator::~pkgCacheGenerator - Destructor 			/*{{{*/
// ---------------------------------------------------------------------
/* We sync the data then unset the dirty flag in two steps so as to
   advoid a problem during a crash */
pkgCacheGenerator::~pkgCacheGenerator()
{
   if (_error->PendingError() == true)
      return;
   if (Map.Sync() == false)
      return;
   
   Cache.HeaderP->Dirty = false;
   Map.Sync(0,sizeof(pkgCache::Header));
}
									/*}}}*/
// CacheGenerator::MergeList - Merge the package list			/*{{{*/
// ---------------------------------------------------------------------
/* This provides the generation of the entries in the cache. Each loop
   goes through a single package record from the underlying parse engine. */
bool pkgCacheGenerator::MergeList(ListParser &List,
				  pkgCache::VerIterator *OutVer)
{
   List.Owner = this;

   unsigned int Counter = 0;
   while (List.Step() == true)
   {
      // Get a pointer to the package structure
      string PackageName = List.Package();
      if (PackageName.empty() == true)
	 return false;
      
      pkgCache::PkgIterator Pkg;
      if (NewPackage(Pkg,PackageName) == false)
	 return _error->Error(_("Error occurred while processing %s (NewPackage)"),PackageName.c_str());
      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      /* Get a pointer to the version structure. We know the list is sorted
         so we use that fact in the search. Insertion of new versions is
	 done with correct sorting */
      string Version = List.Version();
      if (Version.empty() == true)
      {
	 // we first process the package, then the descriptions
	 // (this has the bonus that we get MMap error when we run out
	 //  of MMap space)
	 if (List.UsePackage(Pkg,pkgCache::VerIterator(Cache)) == false)
	    return _error->Error(_("Error occurred while processing %s (UsePackage1)"),
				 PackageName.c_str());

 	 // Find the right version to write the description
 	 MD5SumValue CurMd5 = List.Description_md5();
 	 pkgCache::VerIterator Ver = Pkg.VersionList();
 	 map_ptrloc *LastVer = &Pkg->VersionList;

  	 for (; Ver.end() == false; LastVer = &Ver->NextVer, Ver++) 
 	 {
 	    pkgCache::DescIterator Desc = Ver.DescriptionList();
 	    map_ptrloc *LastDesc = &Ver->DescriptionList;
	    bool duplicate=false;

	    // don't add a new description if we have one for the given
	    // md5 && language
 	    for ( ; Desc.end() == false; Desc++)
	       if (MD5SumValue(Desc.md5()) == CurMd5 && 
	           Desc.LanguageCode() == List.DescriptionLanguage())
		  duplicate=true;
	    if(duplicate)
	       continue;
	    
 	    for (Desc = Ver.DescriptionList();
		 Desc.end() == false; 
		 LastDesc = &Desc->NextDesc, Desc++)
	    {
 	       if (MD5SumValue(Desc.md5()) == CurMd5) 
               {
 		  // Add new description
 		  *LastDesc = NewDescription(Desc, List.DescriptionLanguage(), CurMd5, *LastDesc);
 		  Desc->ParentPkg = Pkg.Index();
		  
 		  if (NewFileDesc(Desc,List) == false)
 		     return _error->Error(_("Error occurred while processing %s (NewFileDesc1)"),PackageName.c_str());
 		  break;
 	       }
	    }
 	 }

	 continue;
      }

      pkgCache::VerIterator Ver = Pkg.VersionList();
      map_ptrloc *LastVer = &Pkg->VersionList;
      int Res = 1;
      for (; Ver.end() == false; LastVer = &Ver->NextVer, Ver++)
      {
	 Res = Cache.VS->CmpVersion(Version,Ver.VerStr());
	 if (Res >= 0)
	    break;
      }
      
      /* We already have a version for this item, record that we
         saw it */
      unsigned long Hash = List.VersionHash();
      if (Res == 0 && Ver->Hash == Hash)
      {
	 if (List.UsePackage(Pkg,Ver) == false)
	    return _error->Error(_("Error occurred while processing %s (UsePackage2)"),
				 PackageName.c_str());

	 if (NewFileVer(Ver,List) == false)
	    return _error->Error(_("Error occurred while processing %s (NewFileVer1)"),
				 PackageName.c_str());
	 
	 // Read only a single record and return
	 if (OutVer != 0)
	 {
	    *OutVer = Ver;
	    FoundFileDeps |= List.HasFileDeps();
	    return true;
	 }
	 
	 continue;
      }      

      // Skip to the end of the same version set.
      if (Res == 0)
      {
	 for (; Ver.end() == false; LastVer = &Ver->NextVer, Ver++)
	 {
	    Res = Cache.VS->CmpVersion(Version,Ver.VerStr());
	    if (Res != 0)
	       break;
	 }
      }

      // Add a new version
      *LastVer = NewVersion(Ver,Version,*LastVer);
      Ver->ParentPkg = Pkg.Index();
      Ver->Hash = Hash;

      if (List.NewVersion(Ver) == false)
	 return _error->Error(_("Error occurred while processing %s (NewVersion1)"),
			      PackageName.c_str());

      if (List.UsePackage(Pkg,Ver) == false)
	 return _error->Error(_("Error occurred while processing %s (UsePackage3)"),
			      PackageName.c_str());
      
      if (NewFileVer(Ver,List) == false)
	 return _error->Error(_("Error occurred while processing %s (NewVersion2)"),
			      PackageName.c_str());

      // Read only a single record and return
      if (OutVer != 0)
      {
	 *OutVer = Ver;
	 FoundFileDeps |= List.HasFileDeps();
	 return true;
      }      

      /* Record the Description data. Description data always exist in
	 Packages and Translation-* files. */
      pkgCache::DescIterator Desc = Ver.DescriptionList();
      map_ptrloc *LastDesc = &Ver->DescriptionList;
      
      // Skip to the end of description set
      for (; Desc.end() == false; LastDesc = &Desc->NextDesc, Desc++);

      // Add new description
      *LastDesc = NewDescription(Desc, List.DescriptionLanguage(), List.Description_md5(), *LastDesc);
      Desc->ParentPkg = Pkg.Index();

      if (NewFileDesc(Desc,List) == false)
	 return _error->Error(_("Error occurred while processing %s (NewFileDesc2)"),PackageName.c_str());
   }

   FoundFileDeps |= List.HasFileDeps();

   if (Cache.HeaderP->PackageCount >= (1ULL<<sizeof(Cache.PkgP->ID)*8)-1)
      return _error->Error(_("Wow, you exceeded the number of package "
			     "names this APT is capable of."));
   if (Cache.HeaderP->VersionCount >= (1ULL<<(sizeof(Cache.VerP->ID)*8))-1)
      return _error->Error(_("Wow, you exceeded the number of versions "
			     "this APT is capable of."));
   if (Cache.HeaderP->DescriptionCount >= (1ULL<<(sizeof(Cache.DescP->ID)*8))-1)
      return _error->Error(_("Wow, you exceeded the number of descriptions "
			     "this APT is capable of."));
   if (Cache.HeaderP->DependsCount >= (1ULL<<(sizeof(Cache.DepP->ID)*8))-1ULL)
      return _error->Error(_("Wow, you exceeded the number of dependencies "
			     "this APT is capable of."));
   return true;
}
									/*}}}*/
// CacheGenerator::MergeFileProvides - Merge file provides   		/*{{{*/
// ---------------------------------------------------------------------
/* If we found any file depends while parsing the main list we need to 
   resolve them. Since it is undesired to load the entire list of files
   into the cache as virtual packages we do a two stage effort. MergeList
   identifies the file depends and this creates Provdies for them by
   re-parsing all the indexs. */
bool pkgCacheGenerator::MergeFileProvides(ListParser &List)
{
   List.Owner = this;
   
   unsigned int Counter = 0;
   while (List.Step() == true)
   {
      string PackageName = List.Package();
      if (PackageName.empty() == true)
	 return false;
      string Version = List.Version();
      if (Version.empty() == true)
	 continue;
      
      pkgCache::PkgIterator Pkg = Cache.FindPkg(PackageName);
      if (Pkg.end() == true)
	 return _error->Error(_("Error occurred while processing %s (FindPkg)"),
				PackageName.c_str());
      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      unsigned long Hash = List.VersionHash();
      pkgCache::VerIterator Ver = Pkg.VersionList();
      for (; Ver.end() == false; Ver++)
      {
	 if (Ver->Hash == Hash && Version.c_str() == Ver.VerStr())
	 {
	    if (List.CollectFileProvides(Cache,Ver) == false)
	       return _error->Error(_("Error occurred while processing %s (CollectFileProvides)"),PackageName.c_str());
	    break;
	 }
      }
      
      if (Ver.end() == true)
	 _error->Warning(_("Package %s %s was not found while processing file dependencies"),PackageName.c_str(),Version.c_str());
   }

   return true;
}
									/*}}}*/
// CacheGenerator::NewPackage - Add a new package			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new package structure and adds it to the hash table */
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator &Pkg,const string &Name)
{
   Pkg = Cache.FindPkg(Name);
   if (Pkg.end() == false)
      return true;

   // Get a structure
   unsigned long Package = Map.Allocate(sizeof(pkgCache::Package));
   if (Package == 0)
      return false;
   
   Pkg = pkgCache::PkgIterator(Cache,Cache.PkgP + Package);
   
   // Insert it into the hash table
   unsigned long Hash = Cache.Hash(Name);
   Pkg->NextPackage = Cache.HeaderP->HashTable[Hash];
   Cache.HeaderP->HashTable[Hash] = Package;
   
   // Set the name and the ID
   Pkg->Name = Map.WriteString(Name);
   if (Pkg->Name == 0)
      return false;
   Pkg->ID = Cache.HeaderP->PackageCount++;
   
   return true;
}
									/*}}}*/
// CacheGenerator::NewFileVer - Create a new File<->Version association	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheGenerator::NewFileVer(pkgCache::VerIterator &Ver,
				   ListParser &List)
{
   if (CurrentFile == 0)
      return true;
   
   // Get a structure
   unsigned long VerFile = Map.Allocate(sizeof(pkgCache::VerFile));
   if (VerFile == 0)
      return 0;
   
   pkgCache::VerFileIterator VF(Cache,Cache.VerFileP + VerFile);
   VF->File = CurrentFile - Cache.PkgFileP;
   
   // Link it to the end of the list
   map_ptrloc *Last = &Ver->FileList;
   for (pkgCache::VerFileIterator V = Ver.FileList(); V.end() == false; V++)
      Last = &V->NextFile;
   VF->NextFile = *Last;
   *Last = VF.Index();
   
   VF->Offset = List.Offset();
   VF->Size = List.Size();
   if (Cache.HeaderP->MaxVerFileSize < VF->Size)
      Cache.HeaderP->MaxVerFileSize = VF->Size;
   Cache.HeaderP->VerFileCount++;
   
   return true;
}
									/*}}}*/
// CacheGenerator::NewVersion - Create a new Version 			/*{{{*/
// ---------------------------------------------------------------------
/* This puts a version structure in the linked list */
unsigned long pkgCacheGenerator::NewVersion(pkgCache::VerIterator &Ver,
					    const string &VerStr,
					    unsigned long Next)
{
   // Get a structure
   unsigned long Version = Map.Allocate(sizeof(pkgCache::Version));
   if (Version == 0)
      return 0;
   
   // Fill it in
   Ver = pkgCache::VerIterator(Cache,Cache.VerP + Version);
   Ver->NextVer = Next;
   Ver->ID = Cache.HeaderP->VersionCount++;
   Ver->VerStr = Map.WriteString(VerStr);
   if (Ver->VerStr == 0)
      return 0;
   
   return Version;
}
									/*}}}*/
// CacheGenerator::NewFileDesc - Create a new File<->Desc association	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheGenerator::NewFileDesc(pkgCache::DescIterator &Desc,
				   ListParser &List)
{
   if (CurrentFile == 0)
      return true;
   
   // Get a structure
   unsigned long DescFile = Map.Allocate(sizeof(pkgCache::DescFile));
   if (DescFile == 0)
      return 0;

   pkgCache::DescFileIterator DF(Cache,Cache.DescFileP + DescFile);
   DF->File = CurrentFile - Cache.PkgFileP;

   // Link it to the end of the list
   map_ptrloc *Last = &Desc->FileList;
   for (pkgCache::DescFileIterator D = Desc.FileList(); D.end() == false; D++)
      Last = &D->NextFile;

   DF->NextFile = *Last;
   *Last = DF.Index();
   
   DF->Offset = List.Offset();
   DF->Size = List.Size();
   if (Cache.HeaderP->MaxDescFileSize < DF->Size)
      Cache.HeaderP->MaxDescFileSize = DF->Size;
   Cache.HeaderP->DescFileCount++;
   
   return true;
}
									/*}}}*/
// CacheGenerator::NewDescription - Create a new Description		/*{{{*/
// ---------------------------------------------------------------------
/* This puts a description structure in the linked list */
map_ptrloc pkgCacheGenerator::NewDescription(pkgCache::DescIterator &Desc,
					    const string &Lang, 
                                            const MD5SumValue &md5sum,
					    map_ptrloc Next)
{
   // Get a structure
   map_ptrloc Description = Map.Allocate(sizeof(pkgCache::Description));
   if (Description == 0)
      return 0;

   // Fill it in
   Desc = pkgCache::DescIterator(Cache,Cache.DescP + Description);
   Desc->NextDesc = Next;
   Desc->ID = Cache.HeaderP->DescriptionCount++;
   Desc->language_code = Map.WriteString(Lang);
   Desc->md5sum = Map.WriteString(md5sum.Value());

   return Description;
}
									/*}}}*/
// ListParser::NewDepends - Create a dependency element			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a dependency element in the tree. It is linked to the
   version and to the package that it is pointing to. */
bool pkgCacheGenerator::ListParser::NewDepends(pkgCache::VerIterator Ver,
					       const string &PackageName,
					       const string &Version,
					       unsigned int Op,
					       unsigned int Type)
{
   pkgCache &Cache = Owner->Cache;
   
   // Get a structure
   unsigned long Dependency = Owner->Map.Allocate(sizeof(pkgCache::Dependency));
   if (Dependency == 0)
      return false;
   
   // Fill it in
   pkgCache::DepIterator Dep(Cache,Cache.DepP + Dependency);
   Dep->ParentVer = Ver.Index();
   Dep->Type = Type;
   Dep->CompareOp = Op;
   Dep->ID = Cache.HeaderP->DependsCount++;
   
   // Locate the target package
   pkgCache::PkgIterator Pkg;
   if (Owner->NewPackage(Pkg,PackageName) == false)
      return false;
   
   // Probe the reverse dependency list for a version string that matches
   if (Version.empty() == false)
   {
/*      for (pkgCache::DepIterator I = Pkg.RevDependsList(); I.end() == false; I++)
	 if (I->Version != 0 && I.TargetVer() == Version)
	    Dep->Version = I->Version;*/
      if (Dep->Version == 0)
	 if ((Dep->Version = WriteString(Version)) == 0)
	    return false;
   }
      
   // Link it to the package
   Dep->Package = Pkg.Index();
   Dep->NextRevDepends = Pkg->RevDepends;
   Pkg->RevDepends = Dep.Index();
   
   /* Link it to the version (at the end of the list)
      Caching the old end point speeds up generation substantially */
   if (OldDepVer != Ver)
   {
      OldDepLast = &Ver->DependsList;
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
	 OldDepLast = &D->NextDepends;
      OldDepVer = Ver;
   }

   // Is it a file dependency?
   if (PackageName[0] == '/')
      FoundFileDeps = true;
   
   Dep->NextDepends = *OldDepLast;
   *OldDepLast = Dep.Index();
   OldDepLast = &Dep->NextDepends;

   return true;
}
									/*}}}*/
// ListParser::NewProvides - Create a Provides element			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheGenerator::ListParser::NewProvides(pkgCache::VerIterator Ver,
					        const string &PackageName,
						const string &Version)
{
   pkgCache &Cache = Owner->Cache;

   // We do not add self referencing provides
   if (Ver.ParentPkg().Name() == PackageName)
      return true;
   
   // Get a structure
   unsigned long Provides = Owner->Map.Allocate(sizeof(pkgCache::Provides));
   if (Provides == 0)
      return false;
   Cache.HeaderP->ProvidesCount++;
   
   // Fill it in
   pkgCache::PrvIterator Prv(Cache,Cache.ProvideP + Provides,Cache.PkgP);
   Prv->Version = Ver.Index();
   Prv->NextPkgProv = Ver->ProvidesList;
   Ver->ProvidesList = Prv.Index();
   if (Version.empty() == false && (Prv->ProvideVersion = WriteString(Version)) == 0)
      return false;
   
   // Locate the target package
   pkgCache::PkgIterator Pkg;
   if (Owner->NewPackage(Pkg,PackageName) == false)
      return false;
   
   // Link it to the package
   Prv->ParentPkg = Pkg.Index();
   Prv->NextProvides = Pkg->ProvidesList;
   Pkg->ProvidesList = Prv.Index();
   
   return true;
}
									/*}}}*/
// CacheGenerator::SelectFile - Select the current file being parsed	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to select which file is to be associated with all newly
   added versions. The caller is responsible for setting the IMS fields. */
bool pkgCacheGenerator::SelectFile(const string &File,const string &Site,
				   const pkgIndexFile &Index,
				   unsigned long Flags)
{
   // Get some space for the structure
   CurrentFile = Cache.PkgFileP + Map.Allocate(sizeof(*CurrentFile));
   if (CurrentFile == Cache.PkgFileP)
      return false;
   
   // Fill it in
   CurrentFile->FileName = Map.WriteString(File);
   CurrentFile->Site = WriteUniqString(Site);
   CurrentFile->NextFile = Cache.HeaderP->FileList;
   CurrentFile->Flags = Flags;
   CurrentFile->ID = Cache.HeaderP->PackageFileCount;
   CurrentFile->IndexType = WriteUniqString(Index.GetType()->Label);
   PkgFileName = File;
   Cache.HeaderP->FileList = CurrentFile - Cache.PkgFileP;
   Cache.HeaderP->PackageFileCount++;

   if (CurrentFile->FileName == 0)
      return false;
   
   if (Progress != 0)
      Progress->SubProgress(Index.Size());
   return true;
}
									/*}}}*/
// CacheGenerator::WriteUniqueString - Insert a unique string		/*{{{*/
// ---------------------------------------------------------------------
/* This is used to create handles to strings. Given the same text it
   always returns the same number */
unsigned long pkgCacheGenerator::WriteUniqString(const char *S,
						 unsigned int Size)
{
   /* We use a very small transient hash table here, this speeds up generation
      by a fair amount on slower machines */
   pkgCache::StringItem *&Bucket = UniqHash[(S[0]*5 + S[1]) % _count(UniqHash)];
   if (Bucket != 0 && 
       stringcmp(S,S+Size,Cache.StrP + Bucket->String) == 0)
      return Bucket->String;
   
   // Search for an insertion point
   pkgCache::StringItem *I = Cache.StringItemP + Cache.HeaderP->StringList;
   int Res = 1;
   map_ptrloc *Last = &Cache.HeaderP->StringList;
   for (; I != Cache.StringItemP; Last = &I->NextItem, 
        I = Cache.StringItemP + I->NextItem)
   {
      Res = stringcmp(S,S+Size,Cache.StrP + I->String);
      if (Res >= 0)
	 break;
   }
   
   // Match
   if (Res == 0)
   {
      Bucket = I;
      return I->String;
   }
   
   // Get a structure
   unsigned long Item = Map.Allocate(sizeof(pkgCache::StringItem));
   if (Item == 0)
      return 0;

   // Fill in the structure
   pkgCache::StringItem *ItemP = Cache.StringItemP + Item;
   ItemP->NextItem = I - Cache.StringItemP;
   *Last = Item;
   ItemP->String = Map.WriteString(S,Size);
   if (ItemP->String == 0)
      return 0;
   
   Bucket = ItemP;
   return ItemP->String;
}
									/*}}}*/

// CheckValidity - Check that a cache is up-to-date			/*{{{*/
// ---------------------------------------------------------------------
/* This just verifies that each file in the list of index files exists,
   has matching attributes with the cache and the cache does not have
   any extra files. */
static bool CheckValidity(const string &CacheFile, FileIterator Start, 
                          FileIterator End,MMap **OutMap = 0)
{
   // No file, certainly invalid
   if (CacheFile.empty() == true || FileExists(CacheFile) == false)
      return false;
   
   // Map it
   FileFd CacheF(CacheFile,FileFd::ReadOnly);
   SPtr<MMap> Map = new MMap(CacheF,MMap::Public | MMap::ReadOnly);
   pkgCache Cache(Map);
   if (_error->PendingError() == true || Map->Size() == 0)
   {
      _error->Discard();
      return false;
   }
   
   /* Now we check every index file, see if it is in the cache,
      verify the IMS data and check that it is on the disk too.. */
   SPtrArray<bool> Visited = new bool[Cache.HeaderP->PackageFileCount];
   memset(Visited,0,sizeof(*Visited)*Cache.HeaderP->PackageFileCount);
   for (; Start != End; Start++)
   {      
      if ((*Start)->HasPackages() == false)
	 continue;
    
      if ((*Start)->Exists() == false)
      {
#if 0 // mvo: we no longer give a message here (Default Sources spec)
	 _error->WarningE("stat",_("Couldn't stat source package list %s"),
			  (*Start)->Describe().c_str());
#endif
	 continue;
      }

      // FindInCache is also expected to do an IMS check.
      pkgCache::PkgFileIterator File = (*Start)->FindInCache(Cache);
      if (File.end() == true)
	 return false;

      Visited[File->ID] = true;
   }
   
   for (unsigned I = 0; I != Cache.HeaderP->PackageFileCount; I++)
      if (Visited[I] == false)
	 return false;
   
   if (_error->PendingError() == true)
   {
      _error->Discard();
      return false;
   }
   
   if (OutMap != 0)
      *OutMap = Map.UnGuard();
   return true;
}
									/*}}}*/
// ComputeSize - Compute the total size of a bunch of files		/*{{{*/
// ---------------------------------------------------------------------
/* Size is kind of an abstract notion that is only used for the progress
   meter */
static unsigned long ComputeSize(FileIterator Start,FileIterator End)
{
   unsigned long TotalSize = 0;
   for (; Start != End; Start++)
   {
      if ((*Start)->HasPackages() == false)
	 continue;      
      TotalSize += (*Start)->Size();
   }
   return TotalSize;
}
									/*}}}*/
// BuildCache - Merge the list of index files into the cache		/*{{{*/
// ---------------------------------------------------------------------
/* */
static bool BuildCache(pkgCacheGenerator &Gen,
		       OpProgress &Progress,
		       unsigned long &CurrentSize,unsigned long TotalSize,
		       FileIterator Start, FileIterator End)
{
   FileIterator I;
   for (I = Start; I != End; I++)
   {
      if ((*I)->HasPackages() == false)
	 continue;
      
      if ((*I)->Exists() == false)
	 continue;

      if ((*I)->FindInCache(Gen.GetCache()).end() == false)
      {
	 _error->Warning("Duplicate sources.list entry %s",
			 (*I)->Describe().c_str());
	 continue;
      }
      
      unsigned long Size = (*I)->Size();
      Progress.OverallProgress(CurrentSize,TotalSize,Size,_("Reading package lists"));
      CurrentSize += Size;
      
      if ((*I)->Merge(Gen,Progress) == false)
	 return false;
   }   

   if (Gen.HasFileDeps() == true)
   {
      Progress.Done();
      TotalSize = ComputeSize(Start, End);
      CurrentSize = 0;
      for (I = Start; I != End; I++)
      {
	 unsigned long Size = (*I)->Size();
	 Progress.OverallProgress(CurrentSize,TotalSize,Size,_("Collecting File Provides"));
	 CurrentSize += Size;
	 if ((*I)->MergeFileProvides(Gen,Progress) == false)
	    return false;
      }
   }
   
   return true;
}
									/*}}}*/
// MakeStatusCache - Construct the status cache				/*{{{*/
// ---------------------------------------------------------------------
/* This makes sure that the status cache (the cache that has all 
   index files from the sources list and all local ones) is ready
   to be mmaped. If OutMap is not zero then a MMap object representing
   the cache will be stored there. This is pretty much mandetory if you
   are using AllowMem. AllowMem lets the function be run as non-root
   where it builds the cache 'fast' into a memory buffer. */
bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap,bool AllowMem)
{
   unsigned long MapSize = _config->FindI("APT::Cache-Limit",24*1024*1024);
   
   vector<pkgIndexFile *> Files;
   for (vector<metaIndex *>::const_iterator i = List.begin();
        i != List.end();
        i++)
   {
      vector <pkgIndexFile *> *Indexes = (*i)->GetIndexFiles();
      for (vector<pkgIndexFile *>::const_iterator j = Indexes->begin();
	   j != Indexes->end();
	   j++)
         Files.push_back (*j);
   }
   
   unsigned long EndOfSource = Files.size();
   if (_system->AddStatusFiles(Files) == false)
      return false;
   
   // Decide if we can write to the files..
   string CacheFile = _config->FindFile("Dir::Cache::pkgcache");
   string SrcCacheFile = _config->FindFile("Dir::Cache::srcpkgcache");
   
   // Decide if we can write to the cache
   bool Writeable = false;
   if (CacheFile.empty() == false)
      Writeable = access(flNotFile(CacheFile).c_str(),W_OK) == 0;
   else
      if (SrcCacheFile.empty() == false)
	 Writeable = access(flNotFile(SrcCacheFile).c_str(),W_OK) == 0;
   
   if (Writeable == false && AllowMem == false && CacheFile.empty() == false)
      return _error->Error(_("Unable to write to %s"),flNotFile(CacheFile).c_str());
   
   Progress.OverallProgress(0,1,1,_("Reading package lists"));
   
   // Cache is OK, Fin.
   if (CheckValidity(CacheFile,Files.begin(),Files.end(),OutMap) == true)
   {
      Progress.OverallProgress(1,1,1,_("Reading package lists"));
      return true;
   }
   
   /* At this point we know we need to reconstruct the package cache,
      begin. */
   SPtr<FileFd> CacheF;
   SPtr<DynamicMMap> Map;
   if (Writeable == true && CacheFile.empty() == false)
   {
      unlink(CacheFile.c_str());
      CacheF = new FileFd(CacheFile,FileFd::WriteEmpty);
      fchmod(CacheF->Fd(),0644);
      Map = new DynamicMMap(*CacheF,MMap::Public,MapSize);
      if (_error->PendingError() == true)
	 return false;
   }
   else
   {
      // Just build it in memory..
      Map = new DynamicMMap(MMap::Public,MapSize);
   }
   
   // Lets try the source cache.
   unsigned long CurrentSize = 0;
   unsigned long TotalSize = 0;
   if (CheckValidity(SrcCacheFile,Files.begin(),
		     Files.begin()+EndOfSource) == true)
   {
      // Preload the map with the source cache
      FileFd SCacheF(SrcCacheFile,FileFd::ReadOnly);
      if (SCacheF.Read((unsigned char *)Map->Data() + Map->RawAllocate(SCacheF.Size()),
		       SCacheF.Size()) == false)
	 return false;

      TotalSize = ComputeSize(Files.begin()+EndOfSource,Files.end());
      
      // Build the status cache
      pkgCacheGenerator Gen(Map.Get(),&Progress);
      if (_error->PendingError() == true)
	 return false;
      if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		     Files.begin()+EndOfSource,Files.end()) == false)
	 return false;
   }
   else
   {
      TotalSize = ComputeSize(Files.begin(),Files.end());
      
      // Build the source cache
      pkgCacheGenerator Gen(Map.Get(),&Progress);
      if (_error->PendingError() == true)
	 return false;
      if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		     Files.begin(),Files.begin()+EndOfSource) == false)
	 return false;
      
      // Write it back
      if (Writeable == true && SrcCacheFile.empty() == false)
      {
	 FileFd SCacheF(SrcCacheFile,FileFd::WriteEmpty);
	 if (_error->PendingError() == true)
	    return false;
	 
	 fchmod(SCacheF.Fd(),0644);
	 
	 // Write out the main data
	 if (SCacheF.Write(Map->Data(),Map->Size()) == false)
	    return _error->Error(_("IO Error saving source cache"));
	 SCacheF.Sync();
	 
	 // Write out the proper header
	 Gen.GetCache().HeaderP->Dirty = false;
	 if (SCacheF.Seek(0) == false ||
	     SCacheF.Write(Map->Data(),sizeof(*Gen.GetCache().HeaderP)) == false)
	    return _error->Error(_("IO Error saving source cache"));
	 Gen.GetCache().HeaderP->Dirty = true;
	 SCacheF.Sync();
      }
      
      // Build the status cache
      if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		     Files.begin()+EndOfSource,Files.end()) == false)
	 return false;
   }

   if (_error->PendingError() == true)
      return false;
   if (OutMap != 0)
   {
      if (CacheF != 0)
      {
	 delete Map.UnGuard();
	 *OutMap = new MMap(*CacheF,MMap::Public | MMap::ReadOnly);
      }
      else
      {
	 *OutMap = Map.UnGuard();
      }      
   }
   
   return true;
}
									/*}}}*/
// MakeOnlyStatusCache - Build a cache with just the status files	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap)
{
   unsigned long MapSize = _config->FindI("APT::Cache-Limit",20*1024*1024);
   vector<pkgIndexFile *> Files;
   unsigned long EndOfSource = Files.size();
   if (_system->AddStatusFiles(Files) == false)
      return false;
   
   SPtr<DynamicMMap> Map;   
   Map = new DynamicMMap(MMap::Public,MapSize);
   unsigned long CurrentSize = 0;
   unsigned long TotalSize = 0;
   
   TotalSize = ComputeSize(Files.begin()+EndOfSource,Files.end());
   
   // Build the status cache
   Progress.OverallProgress(0,1,1,_("Reading package lists"));
   pkgCacheGenerator Gen(Map.Get(),&Progress);
   if (_error->PendingError() == true)
      return false;
   if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		  Files.begin()+EndOfSource,Files.end()) == false)
      return false;
   
   if (_error->PendingError() == true)
      return false;
   *OutMap = Map.UnGuard();
   
   return true;
}
									/*}}}*/

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.10 1998/07/16 06:08:38 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/pkgcachegen.h"
#endif

#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/error.h>
#include <apt-pkg/version.h>
#include <strutl.h>

#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

// CacheGenerator::pkgCacheGenerator - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* We set the diry flag and make sure that is written to the disk */
pkgCacheGenerator::pkgCacheGenerator(DynamicMMap &Map) : Map(Map), Cache(Map)
{
   if (_error->PendingError() == true)
      return;
   
   if (Map.Size() == 0)
   {
      Map.RawAllocate(sizeof(pkgCache::Header));
      *Cache.HeaderP = pkgCache::Header();
   }
   Cache.HeaderP->Dirty = true;
   Map.Sync(0,sizeof(pkgCache::Header));
   Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));
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
bool pkgCacheGenerator::MergeList(ListParser &List)
{
   List.Owner = this;

   while (List.Step() == true)
   {
      // Get a pointer to the package structure
      string PackageName = List.Package();
      pkgCache::PkgIterator Pkg;
      if (NewPackage(Pkg,PackageName) == false)
	 return false;
      
      /* Get a pointer to the version structure. We know the list is sorted
         so we use that fact in the search. Insertion of new versions is
	 done with correct sorting */
      string Version = List.Version();
      if (Version.empty() == true)
      {
	 if (List.UsePackage(Pkg,pkgCache::VerIterator(Cache)) == false)
	    return false;
	 continue;
      }

      pkgCache::VerIterator Ver = Pkg.VersionList();
      unsigned long *Last = &Pkg->VersionList;
      int Res = 1;
      for (; Ver.end() == false; Last = &Ver->NextVer, Ver++)
      {
	 Res = pkgVersionCompare(Version.begin(),Version.end(),Ver.VerStr(),
				 Ver.VerStr() + strlen(Ver.VerStr()));
	 if (Res >= 0)
	    break;
      }
      
      /* We already have a version for this item, record that we
         saw it */
      if (Res == 0)
      {
	 if (List.UsePackage(Pkg,Ver) == false)
	    return false;
	 
	 if (NewFileVer(Ver,List) == false)
	    return false;
	 
	 continue;
      }      

      // Add a new version
      *Last = NewVersion(Ver,Version,*Last);
      Ver->ParentPkg = Pkg.Index();
      if (List.NewVersion(Ver) == false)
	 return false;

      if (List.UsePackage(Pkg,Ver) == false)
	 return false;
      
      if (NewFileVer(Ver,List) == false)
	 return false;
   }

   return true;
}
									/*}}}*/
// CacheGenerator::NewPackage - Add a new package			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new package structure and adds it to the hash table */
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator &Pkg,string Name)
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
   // Get a structure
   unsigned long VerFile = Map.Allocate(sizeof(pkgCache::VerFile));
   if (VerFile == 0)
      return 0;
   
   pkgCache::VerFileIterator VF(Cache,Cache.VerFileP + VerFile);
   VF->File = CurrentFile - Cache.PkgFileP;
   VF->NextFile = Ver->FileList;
   Ver->FileList = VF.Index();
   VF->Offset = List.Offset();
   VF->Size = List.Size();
      
   return true;
}
									/*}}}*/
// CacheGenerator::NewVersion - Create a new Version 			/*{{{*/
// ---------------------------------------------------------------------
/* This puts a version structure in the linked list */
unsigned long pkgCacheGenerator::NewVersion(pkgCache::VerIterator &Ver,
					    string VerStr,
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
// ListParser::NewDepends - Create a dependency element			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a dependency element in the tree. It is linked to the
   version and to the package that it is pointing to. */
bool pkgCacheGenerator::ListParser::NewDepends(pkgCache::VerIterator Ver,
					       string PackageName,
					       string Version,
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
      for (pkgCache::DepIterator I = Pkg.RevDependsList(); I.end() == false; I++)
	 if (I->Version != 0 && I.TargetVer() == Version)
	    Dep->Version = I->Version;
      if (Dep->Version == 0)
	 if ((Dep->Version = WriteString(Version)) == 0)
	    return false;
   }
   
   // Link it to the package
   Dep->Package = Pkg.Index();
   Dep->NextRevDepends = Pkg->RevDepends;
   Pkg->RevDepends = Dep.Index();
   
   // Link it to the version (at the end of the list)
   unsigned long *Last = &Ver->DependsList;
   for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
      Last = &D->NextDepends;
   Dep->NextDepends = *Last;
   *Last = Dep.Index();
   
   return true;
}
									/*}}}*/
// ListParser::NewProvides - Create a Provides element			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheGenerator::ListParser::NewProvides(pkgCache::VerIterator Ver,
					        string PackageName,
						string Version)
{
   pkgCache &Cache = Owner->Cache;

   // We do not add self referencing provides
   if (Ver.ParentPkg().Name() == PackageName)
      return true;
   
   // Get a structure
   unsigned long Provides = Owner->Map.Allocate(sizeof(pkgCache::Provides));
   if (Provides == 0)
      return false;
   
   // Fill it in
   pkgCache::PrvIterator Prv(Cache,Cache.ProvideP + Provides,Cache.PkgP);
   Prv->Version = Ver.Index();
   Prv->NextPkgProv = Ver->ProvidesList;
   Ver->ProvidesList = Prv.Index();
   if (Version.empty() == false && (Prv->Version = WriteString(Version)) == 0)
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
   added versions. */
bool pkgCacheGenerator::SelectFile(string File,unsigned long Flags)
{
   struct stat Buf;
   if (stat(File.c_str(),&Buf) == -1)
      return _error->Errno("stat","Couldn't stat ",File.c_str());
   
   // Get some space for the structure
   CurrentFile = Cache.PkgFileP + Map.Allocate(sizeof(*CurrentFile));
   if (CurrentFile == Cache.PkgFileP)
      return false;
   
   // Fill it in
   CurrentFile->FileName = Map.WriteString(File);
   CurrentFile->Size = Buf.st_size;
   CurrentFile->mtime = Buf.st_mtime;
   CurrentFile->NextFile = Cache.HeaderP->FileList;
   CurrentFile->Flags = Flags;
   PkgFileName = File;

   if (CurrentFile->FileName == 0)
      return false;
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
   // Search for an insertion point
   pkgCache::StringItem *I = Cache.StringItemP + Cache.HeaderP->StringList;
   int Res = 1;
   unsigned long *Last = &Cache.HeaderP->StringList;
   for (; I != Cache.StringItemP; Last = &I->NextItem, 
        I = Cache.StringItemP + I->NextItem)
   {
      Res = stringcmp(S,S+Size,Cache.StrP + I->String);
      if (Res >= 0)
	 break;
   }
   
   // Match
   if (Res == 0)
      return I->String;
   
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
   
   return ItemP->String;
}
									/*}}}*/

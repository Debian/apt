// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.1 1998/07/02 02:58:12 jgg Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <pkglib/pkgcachegen.h>
#include <pkglib/error.h>
#include <pkglib/version.h>

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
   
   do
   {
      // Get a pointer to the package structure
      string Package = List.Package();
      pkgCache::PkgIterator Pkg = Cache.FindPkg(Package);
      if (Pkg.end() == false)
      {
	 if (NewPackage(Pkg,Package) == false)
	    return false;

	 if (List.NewPackage(Pkg) == false)
	    return false;
      }
      if (List.UsePackage(Pkg) == false)
	 return false;
      
      /* Get a pointer to the version structure. We know the list is sorted
         so we use that fact in the search. Insertion of new versions is
	 done with correct sorting */
      string Version = List.Version();
      pkgCache::VerIterator Ver = Pkg.VersionList();
      unsigned long *Last = &Pkg->VersionList;
      int Res;
      for (; Ver.end() == false; Ver++, Last = &Ver->NextVer)
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
	 if (NewFileVer(Ver,List) == false)
	    return false;
	 
	 continue;
      }      

      // Add a new version
      *Last = NewVersion(Ver,*Last);
      if (List.NewVersion(Ver) == false)
	 return false;
      
      if (NewFileVer(Ver,List) == false)
	 return false;
   }
   while (List.Step() == true);
      
   return true;
}
									/*}}}*/
// CacheGenerator::NewPackage - Add a new package			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new package structure and adds it to the hash table */
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator Pkg,string Name)
{
   // Get a structure
   unsigned long Package = Map.Allocate(sizeof(pkgCache::Package));
   if (Package == 0)
      return false;
   
   Pkg = pkgCache::PkgIterator(Cache,Cache.PackageP + Package);
   
   // Insert it into the hash table
   unsigned long Hash = Map.Hash(Name);
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
bool pkgCacheGenerator::NewFileVer(pkgCache::VerIterator Ver,
				   ListParser &List)
{
}
									/*}}}*/
// CacheGenerator::NewVersion - Create a new Version 			/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long pkgCacheGenerator::NewVersion(pkgCache::VerIterator &Ver,
					    unsigned long Next)
{
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
}
									/*}}}*/

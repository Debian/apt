// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.2 1998/07/04 05:57:37 jgg Exp $
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
      if (Pkg.end() == true)
      {
	 if (NewPackage(Pkg,Package) == false)
	    return false;

	 if (List.NewPackage(Pkg) == false)
	    return false;
      }
      
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
      int Res;
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
   while (List.Step() == true);
      
   return true;
}
									/*}}}*/
// CacheGenerator::NewPackage - Add a new package			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new package structure and adds it to the hash table */
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator &Pkg,string Name)
{
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
      return false;
   
   // Fill it in
   Ver = pkgCache::VerIterator(Cache,Cache.VerP + Version);
   Ver->File = CurrentFile - Cache.PkgFileP;
   Ver->NextVer = Next;
   Ver->ID = Cache.HeaderP->VersionCount++;
   Ver->VerStr = Map.WriteString(VerStr);
   if (Ver->VerStr == 0)
      return false;
   
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
      Res = strncmp(Cache.StrP + I->String,S,Size);
      if (Res == 0 && *(Cache.StrP + I->String + Size) != 0)
	 Res = 1;
      if (Res >= 0)
	 break;
   }
   
   // Match
   if (Res == 0)
      return I - Cache.StringItemP;
   
   // Get a structure
   unsigned long Item = Map.Allocate(sizeof(pkgCache::StringItem));
   if (Item == 0)
      return false;
   
   // Fill in the structure
   pkgCache::StringItem *ItemP = Cache.StringItemP + Item;
   ItemP->NextItem = I - Cache.StringItemP;
   *Last = Item;
   ItemP->String = Map.WriteString(S,Size);
   if (ItemP->String == 0)
      return false;
   
   return true;
}
									/*}}}*/

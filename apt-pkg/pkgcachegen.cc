// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.45 2000/01/14 06:26:36 jgg Exp $
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
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/strutl.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <system.h>
									/*}}}*/

// CacheGenerator::pkgCacheGenerator - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* We set the diry flag and make sure that is written to the disk */
pkgCacheGenerator::pkgCacheGenerator(DynamicMMap &Map,OpProgress &Prog) :
                    Map(Map), Cache(Map), Progress(&Prog)
{
   CurrentFile = 0;
   
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
   memset(UniqHash,0,sizeof(UniqHash));
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
	 return _error->Error("Error occured while processing %s (NewPackage)",PackageName.c_str());
      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      /* Get a pointer to the version structure. We know the list is sorted
         so we use that fact in the search. Insertion of new versions is
	 done with correct sorting */
      string Version = List.Version();
      if (Version.empty() == true)
      {
	 if (List.UsePackage(Pkg,pkgCache::VerIterator(Cache)) == false)
	    return _error->Error("Error occured while processing %s (UsePackage1)",PackageName.c_str());
	 continue;
      }

      pkgCache::VerIterator Ver = Pkg.VersionList();
      map_ptrloc *Last = &Pkg->VersionList;
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
      unsigned long Hash = List.VersionHash();
      if (Res == 0 && Ver->Hash == Hash)
      {
	 if (List.UsePackage(Pkg,Ver) == false)
	    return _error->Error("Error occured while processing %s (UsePackage2)",PackageName.c_str());

	 if (NewFileVer(Ver,List) == false)
	    return _error->Error("Error occured while processing %s (NewFileVer1)",PackageName.c_str());
	 
	 // Read only a single record and return
	 if (OutVer != 0)
	 {
	    *OutVer = Ver;
	    return true;
	 }
	 
	 continue;
      }      

      // Skip to the end of the same version set.
      if (Res == 0)
      {
	 for (; Ver.end() == false; Last = &Ver->NextVer, Ver++)
	 {
	    Res = pkgVersionCompare(Version.begin(),Version.end(),Ver.VerStr(),
				    Ver.VerStr() + strlen(Ver.VerStr()));
	    if (Res != 0)
	       break;
	 }
      }

      // Add a new version
      *Last = NewVersion(Ver,Version,*Last);
      Ver->ParentPkg = Pkg.Index();
      Ver->Hash = Hash;
      if (List.NewVersion(Ver) == false)
	 return _error->Error("Error occured while processing %s (NewVersion1)",PackageName.c_str());

      if (List.UsePackage(Pkg,Ver) == false)
	 return _error->Error("Error occured while processing %s (UsePackage3)",PackageName.c_str());
      
      if (NewFileVer(Ver,List) == false)
	 return _error->Error("Error occured while processing %s (NewVersion2)",PackageName.c_str());

      // Read only a single record and return
      if (OutVer != 0)
      {
	 *OutVer = Ver;
	 return true;
      }      
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
/*      for (pkgCache::DepIterator I = Pkg.RevDependsList(); I.end() == false; I++, Hit++)
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
   Cache.HeaderP->ProvidesCount++;
   
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
   CurrentFile->ID = Cache.HeaderP->PackageFileCount;
   PkgFileName = File;
   Cache.HeaderP->FileList = CurrentFile - Cache.PkgFileP;
   Cache.HeaderP->PackageFileCount++;
      
   if (CurrentFile->FileName == 0)
      return false;
   
   if (Progress != 0)
      Progress->SubProgress(Buf.st_size);
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

// SrcCacheCheck - Check if the source package cache is uptodate	/*{{{*/
// ---------------------------------------------------------------------
/* The source cache is checked against the source list and the files 
   on disk, any difference results in a false. */
bool pkgSrcCacheCheck(pkgSourceList &List)
{
   if (_error->PendingError() == true)
      return false;

   string CacheFile = _config->FindFile("Dir::Cache::srcpkgcache");
   string ListDir = _config->FindDir("Dir::State::lists");

   // Count the number of missing files
   int Missing = 0;
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      // Only cache deb source types.
      if (I->Type != pkgSourceList::Item::Deb)
      {
	 Missing++;
	 continue;
      }
      
      string File = ListDir + URItoFileName(I->PackagesURI());
      struct stat Buf;
      if (stat(File.c_str(),&Buf) != 0)
      {
	 // Old format file name.. rename it
	 if (File[0] == '_' && stat(File.c_str()+1,&Buf) == 0)
	 {
	    if (rename(File.c_str()+1,File.c_str()) != 0)
	       return _error->Errno("rename","Failed to rename %s to %s",
				    File.c_str()+1,File.c_str());
	    continue;
	 }	 
	 
	 _error->WarningE("stat","Couldn't stat source package list '%s' (%s)",
			  I->PackagesInfo().c_str(),File.c_str());	 
	 Missing++;
      }      
   }
   
   // Open the source package cache
   if (FileExists(CacheFile) == false)
      return false;
   
   FileFd CacheF(CacheFile,FileFd::ReadOnly);
   if (_error->PendingError() == true)
   {
      _error->Discard();
      return false;
   }
   
   MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true || Map.Size() == 0)
   {
      _error->Discard();
      return false;
   }
   
   pkgCache Cache(Map);
   if (_error->PendingError() == true)
   {
      _error->Discard();
      return false;
   }

   // They are certianly out of sync
   if (Cache.Head().PackageFileCount != List.size() - Missing)
      return false;
   
   for (pkgCache::PkgFileIterator F(Cache); F.end() == false; F++)
   {      
      // Search for a match in the source list
      bool Bad = true;
      for (pkgSourceList::const_iterator I = List.begin(); 
	   I != List.end(); I++)
      {
	 // Only cache deb source types.
	 if (I->Type != pkgSourceList::Item::Deb)
	    continue;
	 
	 string File = ListDir + URItoFileName(I->PackagesURI());
	 if (F.FileName() == File)
	 {
	    Bad = false;
	    break;
	 }
      }
      
      // Check if the file matches what was cached
      Bad |= !F.IsOk();
      if (Bad == true)
	 return false;
   }
   
   return true;
}
									/*}}}*/
// PkgCacheCheck - Check if the package cache is uptodate		/*{{{*/
// ---------------------------------------------------------------------
/* This does a simple check of all files used to compose the cache */
bool pkgPkgCacheCheck(string CacheFile)
{
   if (_error->PendingError() == true)
      return false;
   
   // Open the source package cache
   if (FileExists(CacheFile) == false)
      return false;
   
   FileFd CacheF(CacheFile,FileFd::ReadOnly);
   if (_error->PendingError() == true)
   {
      _error->Discard();
      return false;
   }
   
   MMap Map(CacheF,MMap::Public | MMap::ReadOnly);
   if (_error->PendingError() == true || Map.Size() == 0)
   {
      _error->Discard();
      return false;
   }
   
   pkgCache Cache(Map);
   if (_error->PendingError() == true)
   {
      _error->Discard();
      return false;
   }

   // Status files that must be in the cache
   string Status[3];
   Status[0] = _config->FindFile("Dir::State::xstatus");
   Status[1]= _config->FindFile("Dir::State::userstatus");
   Status[2] = _config->FindFile("Dir::State::status");
   
   // Cheack each file
   for (pkgCache::PkgFileIterator F(Cache); F.end() == false; F++)
   {
      if (F.IsOk() == false)
	 return false;
      
      // See if this is one of the status files
      for (int I = 0; I != 3; I++)
	 if (F.FileName() == Status[I])
	    Status[I] = string();
   }
   
   // Make sure all the status files are loaded.
   for (int I = 0; I != 3; I++)
   {
      if (Status[I].empty() == false && FileExists(Status[I]) == true)
	 return false;
   }   
   
   return true;
}
									/*}}}*/
// AddStatusSize - Add the size of the status files			/*{{{*/
// ---------------------------------------------------------------------
/* This adds the size of all the status files to the size counter */
bool pkgAddStatusSize(unsigned long &TotalSize)
{
   // Grab the file names
   string xstatus = _config->FindFile("Dir::State::xstatus");
   string userstatus = _config->FindFile("Dir::State::userstatus");
   string status = _config->FindFile("Dir::State::status");
   
   // Grab the sizes
   struct stat Buf;
   if (stat(xstatus.c_str(),&Buf) == 0)
      TotalSize += Buf.st_size;
   if (stat(userstatus.c_str(),&Buf) == 0)
      TotalSize += Buf.st_size;
   if (stat(status.c_str(),&Buf) != 0)
      return _error->Errno("stat","Couldn't stat the status file %s",status.c_str());
   TotalSize += Buf.st_size;
   
   return true;
}
									/*}}}*/
// MergeStatus - Add the status files to the cache			/*{{{*/
// ---------------------------------------------------------------------
/* This adds the status files to the map */
bool pkgMergeStatus(OpProgress &Progress,pkgCacheGenerator &Gen,
		    unsigned long &CurrentSize,unsigned long TotalSize)
{
   // Grab the file names   
   string Status[3];
   Status[0] = _config->FindFile("Dir::State::xstatus");
   Status[1]= _config->FindFile("Dir::State::userstatus");
   Status[2] = _config->FindFile("Dir::State::status");
   
   for (int I = 0; I != 3; I++)
   {
      // Check if the file exists and it is not the primary status file.
      string File = Status[I];
      if (I != 2 && FileExists(File) == false)
	 continue;
	 
      FileFd Pkg(File,FileFd::ReadOnly);
      debListParser Parser(Pkg);
      Progress.OverallProgress(CurrentSize,TotalSize,Pkg.Size(),"Reading Package Lists");
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",File.c_str());
      CurrentSize += Pkg.Size();

      Progress.SubProgress(0,"Local Package State - " + flNotDir(File));
      if (Gen.SelectFile(File,pkgCache::Flag::NotSource) == false)
	 return _error->Error("Problem with SelectFile %s",File.c_str());
      
      if (Gen.MergeList(Parser) == false)
	 return _error->Error("Problem with MergeList %s",File.c_str());
      Progress.Progress(Pkg.Size());
   }
   
   return true;
}
									/*}}}*/
// GenerateSrcCache - Write the source package lists to the map		/*{{{*/
// ---------------------------------------------------------------------
/* This puts the source package cache into the given generator. */
bool pkgGenerateSrcCache(pkgSourceList &List,OpProgress &Progress,
			 pkgCacheGenerator &Gen,
			 unsigned long &CurrentSize,unsigned long &TotalSize)
{
   string ListDir = _config->FindDir("Dir::State::lists");
   
   // Prepare the progress indicator
   TotalSize = 0;
   struct stat Buf;
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      string File = ListDir + URItoFileName(I->PackagesURI());
      if (stat(File.c_str(),&Buf) != 0)
	 continue;
      TotalSize += Buf.st_size;
   }
   
   if (pkgAddStatusSize(TotalSize) == false)
      return false;
   
   // Generate the pkg source cache
   CurrentSize = 0;
   for (pkgSourceList::const_iterator I = List.begin(); I != List.end(); I++)
   {
      // Only cache deb source types.
      if (I->Type != pkgSourceList::Item::Deb)
	 continue;
      
      string File = ListDir + URItoFileName(I->PackagesURI());
      
      if (FileExists(File) == false)
	 continue;
      
      FileFd Pkg(File,FileFd::ReadOnly);
      debListParser Parser(Pkg);
      Progress.OverallProgress(CurrentSize,TotalSize,Pkg.Size(),"Reading Package Lists");
      if (_error->PendingError() == true)
	 return _error->Error("Problem opening %s",File.c_str());
      CurrentSize += Pkg.Size();
      
      Progress.SubProgress(0,I->PackagesInfo());
      if (Gen.SelectFile(File) == false)
	 return _error->Error("Problem with SelectFile %s",File.c_str());
      
      if (Gen.MergeList(Parser) == false)
	 return _error->Error("Problem with MergeList %s",File.c_str());
      
      // Check the release file
      string RFile = ListDir + URItoFileName(I->ReleaseURI());
      if (FileExists(RFile) == true)
      {
	 FileFd Rel(RFile,FileFd::ReadOnly);
	 if (_error->PendingError() == true)
	    return false;
	 Parser.LoadReleaseInfo(Gen.GetCurFile(),Rel);
      }
   }
   
   return true;
}
									/*}}}*/
// MakeStatusCache - Generates a cache that includes the status files	/*{{{*/
// ---------------------------------------------------------------------
/* This copies the package source cache and then merges the status and 
   xstatus files into it. */
bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress)
{
   unsigned long MapSize = _config->FindI("APT::Cache-Limit",4*1024*1024);
   
   Progress.OverallProgress(0,1,1,"Reading Package Lists");
   
   string CacheFile = _config->FindFile("Dir::Cache::pkgcache");
   bool SrcOk = pkgSrcCacheCheck(List);
   bool PkgOk = SrcOk && pkgPkgCacheCheck(CacheFile);

   // Rebuild the source and package caches   
   if (SrcOk == false)
   {      
      string SCacheFile = _config->FindFile("Dir::Cache::srcpkgcache");
      FileFd SCacheF(SCacheFile,FileFd::WriteEmpty);
      
      /* Open the pkgcache, we want a new inode here so we do no corrupt
       	 existing mmaps */
      unlink(CacheFile.c_str());             
      FileFd CacheF(CacheFile,FileFd::WriteEmpty);
      DynamicMMap Map(CacheF,MMap::Public,MapSize);
      if (_error->PendingError() == true)
	 return false;

      pkgCacheGenerator Gen(Map,Progress);
      unsigned long CurrentSize = 0;
      unsigned long TotalSize = 0;
      if (pkgGenerateSrcCache(List,Progress,Gen,CurrentSize,TotalSize) == false)
	 return false;
      
      // Write the src cache
      Gen.GetCache().HeaderP->Dirty = false;
      if (SCacheF.Write(Map.Data(),Map.Size()) == false)
	 return _error->Error("IO Error saving source cache");
      Gen.GetCache().HeaderP->Dirty = true;
      
      // Merge in the source caches
      return pkgMergeStatus(Progress,Gen,CurrentSize,TotalSize);
   }

   if (PkgOk == true)
   {
      Progress.OverallProgress(1,1,1,"Reading Package Lists");      
      return true;
   }
   
   // We use the source cache to generate the package cache
   string SCacheFile = _config->FindFile("Dir::Cache::srcpkgcache");
   FileFd SCacheF(SCacheFile,FileFd::ReadOnly);
   
   /* Open the pkgcache, we want a new inode here so we do no corrupt
      existing mmaps */
   unlink(CacheFile.c_str());             
   FileFd CacheF(CacheFile,FileFd::WriteEmpty);
   DynamicMMap Map(CacheF,MMap::Public,MapSize);
   if (_error->PendingError() == true)
      return false;
   
   // Preload the map with the source cache
   if (SCacheF.Read((unsigned char *)Map.Data() + Map.RawAllocate(SCacheF.Size()),
		    SCacheF.Size()) == false)
      return false;
      
   pkgCacheGenerator Gen(Map,Progress);
   
   // Compute the progress
   unsigned long TotalSize = 0;
   if (pkgAddStatusSize(TotalSize) == false)
      return false;

   unsigned long CurrentSize = 0;
   return pkgMergeStatus(Progress,Gen,CurrentSize,TotalSize);
}
									/*}}}*/
// MakeStatusCacheMem - Returns a map for the status cache		/*{{{*/
// ---------------------------------------------------------------------
/* This creates a map object for the status cache. If the process has write
   access to the caches then it is the same as MakeStatusCache, otherwise it
   creates a memory block and puts the cache in there. */
MMap *pkgMakeStatusCacheMem(pkgSourceList &List,OpProgress &Progress)
{
   unsigned long MapSize = _config->FindI("APT::Cache-Limit",4*1024*1024);
   
   /* If the cache file is writeable this is just a wrapper for
      MakeStatusCache */
   string CacheFile = _config->FindFile("Dir::Cache::pkgcache");
   bool Writeable = (access(CacheFile.c_str(),W_OK) == 0) ||
                    (errno == ENOENT);

   if (Writeable == true)
   {
      if (pkgMakeStatusCache(List,Progress) == false)
	 return 0;
      
      // Open the cache file
      FileFd File(_config->FindFile("Dir::Cache::pkgcache"),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return 0;
      
      MMap *Map = new MMap(File,MMap::Public | MMap::ReadOnly);
      if (_error->PendingError() == true)
      {
	 delete Map;
	 return 0;
      }
      return Map;
   }      

   // Mostly from MakeStatusCache..
   Progress.OverallProgress(0,1,1,"Reading Package Lists");
   
   bool SrcOk = pkgSrcCacheCheck(List);
   bool PkgOk = SrcOk && pkgPkgCacheCheck(CacheFile);
			   
   // Rebuild the source and package caches   
   if (SrcOk == false)
   {
      DynamicMMap *Map = new DynamicMMap(MMap::Public,MapSize);
      if (_error->PendingError() == true)
      {
	 delete Map;
	 return 0;
      }
      
      pkgCacheGenerator Gen(*Map,Progress);
      unsigned long CurrentSize = 0;
      unsigned long TotalSize = 0;
      if (pkgGenerateSrcCache(List,Progress,Gen,CurrentSize,TotalSize) == false)
      {
	 delete Map;
	 return 0;
      }
      
      // Merge in the source caches
      if (pkgMergeStatus(Progress,Gen,CurrentSize,TotalSize) == false)
      {
	 delete Map;
	 return 0;
      }
      
      return Map;
   }

   if (PkgOk == true)
   {
      Progress.OverallProgress(1,1,1,"Reading Package Lists");
      
      // Open the cache file
      FileFd File(_config->FindFile("Dir::Cache::pkgcache"),FileFd::ReadOnly);
      if (_error->PendingError() == true)
	 return 0;
      
      MMap *Map = new MMap(File,MMap::Public | MMap::ReadOnly);
      if (_error->PendingError() == true)
      {
	 delete Map;
	 return 0;
      }
      return Map;
   }
   
   // We use the source cache to generate the package cache
   string SCacheFile = _config->FindFile("Dir::Cache::srcpkgcache");
   FileFd SCacheF(SCacheFile,FileFd::ReadOnly);
   DynamicMMap *Map = new DynamicMMap(MMap::Public,MapSize);
   if (_error->PendingError() == true)
   {
      delete Map;
      return 0;
   }
   
   // Preload the map with the source cache
   if (SCacheF.Read((unsigned char *)Map->Data() + Map->RawAllocate(SCacheF.Size()),
		    SCacheF.Size()) == false)
   {
      delete Map;
      return 0;
   }
      
   pkgCacheGenerator Gen(*Map,Progress);
   
   // Compute the progress
   unsigned long TotalSize = 0;
   if (pkgAddStatusSize(TotalSize) == false)
   {
      delete Map;
      return 0;
   }

   unsigned long CurrentSize = 0;
   if (pkgMergeStatus(Progress,Gen,CurrentSize,TotalSize) == false)
   {
      delete Map;
      return 0;
   }
    
   return Map;
}
									/*}}}*/

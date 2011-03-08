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
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/macros.h>

#include <apt-pkg/tagfile.h>

#include <apti18n.h>

#include <vector>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
									/*}}}*/
typedef vector<pkgIndexFile *>::iterator FileIterator;
template <typename Iter> std::vector<Iter*> pkgCacheGenerator::Dynamic<Iter>::toReMap;

// CacheGenerator::pkgCacheGenerator - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* We set the dirty flag and make sure that is written to the disk */
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
      if (Map.RawAllocate(sizeof(pkgCache::Header)) == 0 && _error->PendingError() == true)
	 return;

      Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));

      // Starting header
      *Cache.HeaderP = pkgCache::Header();
      map_ptrloc const idxVerSysName = WriteStringInMap(_system->VS->Label);
      Cache.HeaderP->VerSysName = idxVerSysName;
      map_ptrloc const idxArchitecture = WriteStringInMap(_config->Find("APT::Architecture"));
      Cache.HeaderP->Architecture = idxArchitecture;
      if (unlikely(idxVerSysName == 0 || idxArchitecture == 0))
	 return;
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
void pkgCacheGenerator::ReMap(void const * const oldMap, void const * const newMap) {/*{{{*/
   if (oldMap == newMap)
      return;

   Cache.ReMap(false);

   CurrentFile += (pkgCache::PackageFile*) newMap - (pkgCache::PackageFile*) oldMap;

   for (size_t i = 0; i < _count(UniqHash); ++i)
      if (UniqHash[i] != 0)
	 UniqHash[i] += (pkgCache::StringItem*) newMap - (pkgCache::StringItem*) oldMap;

   for (std::vector<pkgCache::GrpIterator*>::const_iterator i = Dynamic<pkgCache::GrpIterator>::toReMap.begin();
	i != Dynamic<pkgCache::GrpIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PkgIterator*>::const_iterator i = Dynamic<pkgCache::PkgIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PkgIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::VerIterator*>::const_iterator i = Dynamic<pkgCache::VerIterator>::toReMap.begin();
	i != Dynamic<pkgCache::VerIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::DepIterator*>::const_iterator i = Dynamic<pkgCache::DepIterator>::toReMap.begin();
	i != Dynamic<pkgCache::DepIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::DescIterator*>::const_iterator i = Dynamic<pkgCache::DescIterator>::toReMap.begin();
	i != Dynamic<pkgCache::DescIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PrvIterator*>::const_iterator i = Dynamic<pkgCache::PrvIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PrvIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PkgFileIterator*>::const_iterator i = Dynamic<pkgCache::PkgFileIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PkgFileIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
}									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_ptrloc pkgCacheGenerator::WriteStringInMap(const char *String,
					const unsigned long &Len) {
   void const * const oldMap = Map.Data();
   map_ptrloc const index = Map.WriteString(String, Len);
   if (index != 0)
      ReMap(oldMap, Map.Data());
   return index;
}
									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_ptrloc pkgCacheGenerator::WriteStringInMap(const char *String) {
   void const * const oldMap = Map.Data();
   map_ptrloc const index = Map.WriteString(String);
   if (index != 0)
      ReMap(oldMap, Map.Data());
   return index;
}
									/*}}}*/
map_ptrloc pkgCacheGenerator::AllocateInMap(const unsigned long &size) {/*{{{*/
   void const * const oldMap = Map.Data();
   map_ptrloc const index = Map.Allocate(size);
   if (index != 0)
      ReMap(oldMap, Map.Data());
   return index;
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
      string const PackageName = List.Package();
      if (PackageName.empty() == true)
	 return false;

      /* Treat Arch all packages as the same as the native arch. */
      string Arch;
      if (List.ArchitectureAll() == true)
	 Arch = _config->Find("APT::Architecture");
      else
	 Arch = List.Architecture();
 
      // Get a pointer to the package structure
      pkgCache::PkgIterator Pkg;
      Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
      if (NewPackage(Pkg, PackageName, Arch) == false)
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
	 pkgCache::VerIterator Ver(Cache);
	 Dynamic<pkgCache::VerIterator> DynVer(Ver);
	 if (List.UsePackage(Pkg, Ver) == false)
	    return _error->Error(_("Error occurred while processing %s (UsePackage1)"),
				 PackageName.c_str());

 	 // Find the right version to write the description
 	 MD5SumValue CurMd5 = List.Description_md5();
 	 Ver = Pkg.VersionList();

	 for (; Ver.end() == false; ++Ver)
 	 {
 	    pkgCache::DescIterator Desc = Ver.DescriptionList();
	    Dynamic<pkgCache::DescIterator> DynDesc(Desc);
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
		  void const * const oldMap = Map.Data();
		  map_ptrloc const descindex = NewDescription(Desc, List.DescriptionLanguage(), CurMd5, *LastDesc);
		  if (oldMap != Map.Data())
		     LastDesc += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
		  *LastDesc = descindex;
 		  Desc->ParentPkg = Pkg.Index();
		  
		  if ((*LastDesc == 0 && _error->PendingError()) || NewFileDesc(Desc,List) == false)
 		     return _error->Error(_("Error occurred while processing %s (NewFileDesc1)"),PackageName.c_str());
 		  break;
 	       }
	    }
 	 }

	 continue;
      }

      pkgCache::VerIterator Ver = Pkg.VersionList();
      Dynamic<pkgCache::VerIterator> DynVer(Ver);
      map_ptrloc *LastVer = &Pkg->VersionList;
      void const * oldMap = Map.Data();
      int Res = 1;
      unsigned long const Hash = List.VersionHash();
      for (; Ver.end() == false; LastVer = &Ver->NextVer, Ver++)
      {
	 Res = Cache.VS->CmpVersion(Version,Ver.VerStr());
	 // Version is higher as current version - insert here
	 if (Res > 0)
	    break;
	 // Versionstrings are equal - is hash also equal?
	 if (Res == 0 && Ver->Hash == Hash)
	    break;
	 // proceed with the next till we have either the right
	 // or we found another version (which will be lower)
      }

      /* We already have a version for this item, record that we saw it */
      if (Res == 0 && Ver.end() == false && Ver->Hash == Hash)
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

      // Add a new version
      map_ptrloc const verindex = NewVersion(Ver,Version,*LastVer);
      if (verindex == 0 && _error->PendingError())
	 return _error->Error(_("Error occurred while processing %s (NewVersion%d)"),
			      PackageName.c_str(), 1);

      if (oldMap != Map.Data())
	 LastVer += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
      *LastVer = verindex;
      Ver->ParentPkg = Pkg.Index();
      Ver->Hash = Hash;

      if (List.NewVersion(Ver) == false)
	 return _error->Error(_("Error occurred while processing %s (NewVersion%d)"),
			      PackageName.c_str(), 2);

      if (List.UsePackage(Pkg,Ver) == false)
	 return _error->Error(_("Error occurred while processing %s (UsePackage3)"),
			      PackageName.c_str());
      
      if (NewFileVer(Ver,List) == false)
	 return _error->Error(_("Error occurred while processing %s (NewVersion%d)"),
			      PackageName.c_str(), 3);

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
      Dynamic<pkgCache::DescIterator> DynDesc(Desc);
      map_ptrloc *LastDesc = &Ver->DescriptionList;

      // Skip to the end of description set
      for (; Desc.end() == false; LastDesc = &Desc->NextDesc, Desc++);

      // Add new description
      oldMap = Map.Data();
      map_ptrloc const descindex = NewDescription(Desc, List.DescriptionLanguage(), List.Description_md5(), *LastDesc);
      if (oldMap != Map.Data())
	 LastDesc += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
      *LastDesc = descindex;
      Desc->ParentPkg = Pkg.Index();

      if ((*LastDesc == 0 && _error->PendingError()) || NewFileDesc(Desc,List) == false)
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
      Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
      if (Pkg.end() == true)
	 return _error->Error(_("Error occurred while processing %s (FindPkg)"),
				PackageName.c_str());
      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      unsigned long Hash = List.VersionHash();
      pkgCache::VerIterator Ver = Pkg.VersionList();
      Dynamic<pkgCache::VerIterator> DynVer(Ver);
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
// CacheGenerator::NewGroup - Add a new group				/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new group structure and adds it to the hash table */
bool pkgCacheGenerator::NewGroup(pkgCache::GrpIterator &Grp, const string &Name)
{
   Grp = Cache.FindGrp(Name);
   if (Grp.end() == false)
      return true;

   // Get a structure
   map_ptrloc const Group = AllocateInMap(sizeof(pkgCache::Group));
   if (unlikely(Group == 0))
      return false;

   Grp = pkgCache::GrpIterator(Cache, Cache.GrpP + Group);
   map_ptrloc const idxName = WriteStringInMap(Name);
   if (unlikely(idxName == 0))
      return false;
   Grp->Name = idxName;

   // Insert it into the hash table
   unsigned long const Hash = Cache.Hash(Name);
   Grp->Next = Cache.HeaderP->GrpHashTable[Hash];
   Cache.HeaderP->GrpHashTable[Hash] = Group;

   Grp->ID = Cache.HeaderP->GroupCount++;
   return true;
}
									/*}}}*/
// CacheGenerator::NewPackage - Add a new package			/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new package structure and adds it to the hash table */
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator &Pkg,const string &Name,
					const string &Arch) {
   pkgCache::GrpIterator Grp;
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);
   if (unlikely(NewGroup(Grp, Name) == false))
      return false;

   Pkg = Grp.FindPkg(Arch);
      if (Pkg.end() == false)
	 return true;

   // Get a structure
   map_ptrloc const Package = AllocateInMap(sizeof(pkgCache::Package));
   if (unlikely(Package == 0))
      return false;
   Pkg = pkgCache::PkgIterator(Cache,Cache.PkgP + Package);

   // Insert the package into our package list
   if (Grp->FirstPackage == 0) // the group is new
   {
      // Insert it into the hash table
      unsigned long const Hash = Cache.Hash(Name);
      Pkg->NextPackage = Cache.HeaderP->PkgHashTable[Hash];
      Cache.HeaderP->PkgHashTable[Hash] = Package;
      Grp->FirstPackage = Package;
   }
   else // Group the Packages together
   {
      // this package is the new last package
      pkgCache::PkgIterator LastPkg(Cache, Cache.PkgP + Grp->LastPackage);
      Pkg->NextPackage = LastPkg->NextPackage;
      LastPkg->NextPackage = Package;
   }
   Grp->LastPackage = Package;

   // Set the name, arch and the ID
   Pkg->Name = Grp->Name;
   Pkg->Group = Grp.Index();
   map_ptrloc const idxArch = WriteUniqString(Arch.c_str());
   if (unlikely(idxArch == 0))
      return false;
   Pkg->Arch = idxArch;
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
   map_ptrloc const VerFile = AllocateInMap(sizeof(pkgCache::VerFile));
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
   map_ptrloc const Version = AllocateInMap(sizeof(pkgCache::Version));
   if (Version == 0)
      return 0;
   
   // Fill it in
   Ver = pkgCache::VerIterator(Cache,Cache.VerP + Version);
   Ver->NextVer = Next;
   Ver->ID = Cache.HeaderP->VersionCount++;
   map_ptrloc const idxVerStr = WriteStringInMap(VerStr);
   if (unlikely(idxVerStr == 0))
      return 0;
   Ver->VerStr = idxVerStr;
   
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
   map_ptrloc const DescFile = AllocateInMap(sizeof(pkgCache::DescFile));
   if (DescFile == 0)
      return false;

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
   map_ptrloc const Description = AllocateInMap(sizeof(pkgCache::Description));
   if (Description == 0)
      return 0;

   // Fill it in
   Desc = pkgCache::DescIterator(Cache,Cache.DescP + Description);
   Desc->NextDesc = Next;
   Desc->ID = Cache.HeaderP->DescriptionCount++;
   map_ptrloc const idxlanguage_code = WriteStringInMap(Lang);
   map_ptrloc const idxmd5sum = WriteStringInMap(md5sum.Value());
   if (unlikely(idxlanguage_code == 0 || idxmd5sum == 0))
      return 0;
   Desc->language_code = idxlanguage_code;
   Desc->md5sum = idxmd5sum;

   return Description;
}
									/*}}}*/
// CacheGenerator::FinishCache - do various finish operations		/*{{{*/
// ---------------------------------------------------------------------
/* This prepares the Cache for delivery */
bool pkgCacheGenerator::FinishCache(OpProgress *Progress)
{
   // FIXME: add progress reporting for this operation
   // Do we have different architectures in your groups ?
   vector<string> archs = APT::Configuration::getArchitectures();
   if (archs.size() > 1)
   {
      // Create Conflicts in between the group
      pkgCache::GrpIterator G = GetCache().GrpBegin();
      Dynamic<pkgCache::GrpIterator> DynG(G);
      for (; G.end() != true; G++)
      {
	 string const PkgName = G.Name();
	 pkgCache::PkgIterator P = G.PackageList();
	 Dynamic<pkgCache::PkgIterator> DynP(P);
	 for (; P.end() != true; P = G.NextPkg(P))
	 {
	    pkgCache::PkgIterator allPkg;
	    Dynamic<pkgCache::PkgIterator> DynallPkg(allPkg);
	    pkgCache::VerIterator V = P.VersionList();
	    Dynamic<pkgCache::VerIterator> DynV(V);
	    for (; V.end() != true; V++)
	    {
	       char const * const Arch = P.Arch();
	       map_ptrloc *OldDepLast = NULL;
	       /* MultiArch handling introduces a lot of implicit Dependencies:
		- MultiArch: same → Co-Installable if they have the same version
		- Architecture: all → Need to be Co-Installable for internal reasons
		- All others conflict with all other group members */
	       bool const coInstall = (V->MultiArch == pkgCache::Version::Same);
	       for (vector<string>::const_iterator A = archs.begin(); A != archs.end(); ++A)
	       {
		  if (*A == Arch)
		     continue;
		  /* We allow only one installed arch at the time
		     per group, therefore each group member conflicts
		     with all other group members */
		  pkgCache::PkgIterator D = G.FindPkg(*A);
		  Dynamic<pkgCache::PkgIterator> DynD(D);
		  if (D.end() == true)
		     continue;
		  if (coInstall == true)
		  {
		     // Replaces: ${self}:other ( << ${binary:Version})
		     NewDepends(D, V, V.VerStr(),
				pkgCache::Dep::Less, pkgCache::Dep::Replaces,
				OldDepLast);
		     // Breaks: ${self}:other (!= ${binary:Version})
		     NewDepends(D, V, V.VerStr(),
				pkgCache::Dep::NotEquals, pkgCache::Dep::DpkgBreaks,
				OldDepLast);
		  } else {
			// Conflicts: ${self}:other
			NewDepends(D, V, "",
				   pkgCache::Dep::NoOp, pkgCache::Dep::Conflicts,
				   OldDepLast);
		  }
	       }
	    }
	 }
      }
   }
   return true;
}
									/*}}}*/
// CacheGenerator::NewDepends - Create a dependency element		/*{{{*/
// ---------------------------------------------------------------------
/* This creates a dependency element in the tree. It is linked to the
   version and to the package that it is pointing to. */
bool pkgCacheGenerator::NewDepends(pkgCache::PkgIterator &Pkg,
				   pkgCache::VerIterator &Ver,
				   string const &Version,
				   unsigned int const &Op,
				   unsigned int const &Type,
				   map_ptrloc *OldDepLast)
{
   void const * const oldMap = Map.Data();
   // Get a structure
   map_ptrloc const Dependency = AllocateInMap(sizeof(pkgCache::Dependency));
   if (unlikely(Dependency == 0))
      return false;
   
   // Fill it in
   pkgCache::DepIterator Dep(Cache,Cache.DepP + Dependency);
   Dynamic<pkgCache::DepIterator> DynDep(Dep);
   Dep->ParentVer = Ver.Index();
   Dep->Type = Type;
   Dep->CompareOp = Op;
   Dep->ID = Cache.HeaderP->DependsCount++;

   // Probe the reverse dependency list for a version string that matches
   if (Version.empty() == false)
   {
/*      for (pkgCache::DepIterator I = Pkg.RevDependsList(); I.end() == false; I++)
	 if (I->Version != 0 && I.TargetVer() == Version)
	    Dep->Version = I->Version;*/
      if (Dep->Version == 0) {
	 map_ptrloc const index = WriteStringInMap(Version);
	 if (unlikely(index == 0))
	    return false;
	 Dep->Version = index;
      }
   }

   // Link it to the package
   Dep->Package = Pkg.Index();
   Dep->NextRevDepends = Pkg->RevDepends;
   Pkg->RevDepends = Dep.Index();

   // Do we know where to link the Dependency to?
   if (OldDepLast == NULL)
   {
      OldDepLast = &Ver->DependsList;
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; D++)
	 OldDepLast = &D->NextDepends;
   } else if (oldMap != Map.Data())
      OldDepLast += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;

   Dep->NextDepends = *OldDepLast;
   *OldDepLast = Dep.Index();
   OldDepLast = &Dep->NextDepends;

   return true;
}
									/*}}}*/
// ListParser::NewDepends - Create the environment for a new dependency	/*{{{*/
// ---------------------------------------------------------------------
/* This creates a Group and the Package to link this dependency to if
   needed and handles also the caching of the old endpoint */
bool pkgCacheGenerator::ListParser::NewDepends(pkgCache::VerIterator &Ver,
					       const string &PackageName,
					       const string &Arch,
					       const string &Version,
					       unsigned int Op,
					       unsigned int Type)
{
   pkgCache::GrpIterator Grp;
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);
   if (unlikely(Owner->NewGroup(Grp, PackageName) == false))
      return false;

   // Locate the target package
   pkgCache::PkgIterator Pkg = Grp.FindPkg(Arch);
   Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
   if (Pkg.end() == true) {
      if (unlikely(Owner->NewPackage(Pkg, PackageName, Arch) == false))
	 return false;
   }

   // Is it a file dependency?
   if (unlikely(PackageName[0] == '/'))
      FoundFileDeps = true;

   /* Caching the old end point speeds up generation substantially */
   if (OldDepVer != Ver) {
      OldDepLast = NULL;
      OldDepVer = Ver;
   }

   return Owner->NewDepends(Pkg, Ver, Version, Op, Type, OldDepLast);
}
									/*}}}*/
// ListParser::NewProvides - Create a Provides element			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCacheGenerator::ListParser::NewProvides(pkgCache::VerIterator &Ver,
					        const string &PkgName,
						const string &PkgArch,
						const string &Version)
{
   pkgCache &Cache = Owner->Cache;

   // We do not add self referencing provides
   if (Ver.ParentPkg().Name() == PkgName && PkgArch == Ver.Arch())
      return true;
   
   // Get a structure
   map_ptrloc const Provides = Owner->AllocateInMap(sizeof(pkgCache::Provides));
   if (unlikely(Provides == 0))
      return false;
   Cache.HeaderP->ProvidesCount++;
   
   // Fill it in
   pkgCache::PrvIterator Prv(Cache,Cache.ProvideP + Provides,Cache.PkgP);
   Dynamic<pkgCache::PrvIterator> DynPrv(Prv);
   Prv->Version = Ver.Index();
   Prv->NextPkgProv = Ver->ProvidesList;
   Ver->ProvidesList = Prv.Index();
   if (Version.empty() == false && unlikely((Prv->ProvideVersion = WriteString(Version)) == 0))
      return false;
   
   // Locate the target package
   pkgCache::PkgIterator Pkg;
   Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
   if (unlikely(Owner->NewPackage(Pkg,PkgName, PkgArch) == false))
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
   map_ptrloc const idxFile = AllocateInMap(sizeof(*CurrentFile));
   if (unlikely(idxFile == 0))
      return false;
   CurrentFile = Cache.PkgFileP + idxFile;

   // Fill it in
   map_ptrloc const idxFileName = WriteStringInMap(File);
   map_ptrloc const idxSite = WriteUniqString(Site);
   if (unlikely(idxFileName == 0 || idxSite == 0))
      return false;
   CurrentFile->FileName = idxFileName;
   CurrentFile->Site = idxSite;
   CurrentFile->NextFile = Cache.HeaderP->FileList;
   CurrentFile->Flags = Flags;
   CurrentFile->ID = Cache.HeaderP->PackageFileCount;
   map_ptrloc const idxIndexType = WriteUniqString(Index.GetType()->Label);
   if (unlikely(idxIndexType == 0))
      return false;
   CurrentFile->IndexType = idxIndexType;
   PkgFileName = File;
   Cache.HeaderP->FileList = CurrentFile - Cache.PkgFileP;
   Cache.HeaderP->PackageFileCount++;

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
   void const * const oldMap = Map.Data();
   map_ptrloc const Item = AllocateInMap(sizeof(pkgCache::StringItem));
   if (Item == 0)
      return 0;

   map_ptrloc const idxString = WriteStringInMap(S,Size);
   if (unlikely(idxString == 0))
      return 0;
   if (oldMap != Map.Data()) {
      Last += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
      I += (pkgCache::StringItem*) Map.Data() - (pkgCache::StringItem*) oldMap;
   }
   *Last = Item;

   // Fill in the structure
   pkgCache::StringItem *ItemP = Cache.StringItemP + Item;
   ItemP->NextItem = I - Cache.StringItemP;
   ItemP->String = idxString;

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
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);
   // No file, certainly invalid
   if (CacheFile.empty() == true || FileExists(CacheFile) == false)
   {
      if (Debug == true)
	 std::clog << "CacheFile doesn't exist" << std::endl;
      return false;
   }

   // Map it
   FileFd CacheF(CacheFile,FileFd::ReadOnly);
   SPtr<MMap> Map = new MMap(CacheF,0);
   pkgCache Cache(Map);
   if (_error->PendingError() == true || Map->Size() == 0)
   {
      if (Debug == true)
	 std::clog << "Errors are pending or Map is empty()" << std::endl;
      _error->Discard();
      return false;
   }
   
   /* Now we check every index file, see if it is in the cache,
      verify the IMS data and check that it is on the disk too.. */
   SPtrArray<bool> Visited = new bool[Cache.HeaderP->PackageFileCount];
   memset(Visited,0,sizeof(*Visited)*Cache.HeaderP->PackageFileCount);
   for (; Start != End; Start++)
   {
      if (Debug == true)
	 std::clog << "Checking PkgFile " << (*Start)->Describe() << ": ";
      if ((*Start)->HasPackages() == false)
      {
         if (Debug == true)
	    std::clog << "Has NO packages" << std::endl;
	 continue;
      }
    
      if ((*Start)->Exists() == false)
      {
#if 0 // mvo: we no longer give a message here (Default Sources spec)
	 _error->WarningE("stat",_("Couldn't stat source package list %s"),
			  (*Start)->Describe().c_str());
#endif
         if (Debug == true)
	    std::clog << "file doesn't exist" << std::endl;
	 continue;
      }

      // FindInCache is also expected to do an IMS check.
      pkgCache::PkgFileIterator File = (*Start)->FindInCache(Cache);
      if (File.end() == true)
      {
	 if (Debug == true)
	    std::clog << "FindInCache returned end-Pointer" << std::endl;
	 return false;
      }

      Visited[File->ID] = true;
      if (Debug == true)
	 std::clog << "with ID " << File->ID << " is valid" << std::endl;
   }
   
   for (unsigned I = 0; I != Cache.HeaderP->PackageFileCount; I++)
      if (Visited[I] == false)
      {
	 if (Debug == true)
	    std::clog << "File with ID" << I << " wasn't visited" << std::endl;
	 return false;
      }
   
   if (_error->PendingError() == true)
   {
      if (Debug == true)
      {
	 std::clog << "Validity failed because of pending errors:" << std::endl;
	 _error->DumpErrors();
      }
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
		       OpProgress *Progress,
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
      if (Progress != NULL)
	 Progress->OverallProgress(CurrentSize,TotalSize,Size,_("Reading package lists"));
      CurrentSize += Size;
      
      if ((*I)->Merge(Gen,Progress) == false)
	 return false;
   }   

   if (Gen.HasFileDeps() == true)
   {
      if (Progress != NULL)
	 Progress->Done();
      TotalSize = ComputeSize(Start, End);
      CurrentSize = 0;
      for (I = Start; I != End; I++)
      {
	 unsigned long Size = (*I)->Size();
	 if (Progress != NULL)
	    Progress->OverallProgress(CurrentSize,TotalSize,Size,_("Collecting File Provides"));
	 CurrentSize += Size;
	 if ((*I)->MergeFileProvides(Gen,Progress) == false)
	    return false;
      }
   }
   
   return true;
}
									/*}}}*/
// CacheGenerator::CreateDynamicMMap - load an mmap with configuration options	/*{{{*/
DynamicMMap* pkgCacheGenerator::CreateDynamicMMap(FileFd *CacheF, unsigned long Flags) {
   unsigned long const MapStart = _config->FindI("APT::Cache-Start", 24*1024*1024);
   unsigned long const MapGrow = _config->FindI("APT::Cache-Grow", 1*1024*1024);
   unsigned long const MapLimit = _config->FindI("APT::Cache-Limit", 0);
   Flags |= MMap::Moveable;
   if (_config->FindB("APT::Cache-Fallback", false) == true)
      Flags |= MMap::Fallback;
   if (CacheF != NULL)
      return new DynamicMMap(*CacheF, Flags, MapStart, MapGrow, MapLimit);
   else
      return new DynamicMMap(Flags, MapStart, MapGrow, MapLimit);
}
									/*}}}*/
// CacheGenerator::MakeStatusCache - Construct the status cache		/*{{{*/
// ---------------------------------------------------------------------
/* This makes sure that the status cache (the cache that has all 
   index files from the sources list and all local ones) is ready
   to be mmaped. If OutMap is not zero then a MMap object representing
   the cache will be stored there. This is pretty much mandetory if you
   are using AllowMem. AllowMem lets the function be run as non-root
   where it builds the cache 'fast' into a memory buffer. */
__deprecated bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap, bool AllowMem)
   { return pkgCacheGenerator::MakeStatusCache(List, &Progress, OutMap, AllowMem); }
bool pkgCacheGenerator::MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap,bool AllowMem)
{
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);
   
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
   
   unsigned long const EndOfSource = Files.size();
   if (_system->AddStatusFiles(Files) == false)
      return false;

   // Decide if we can write to the files..
   string const CacheFile = _config->FindFile("Dir::Cache::pkgcache");
   string const SrcCacheFile = _config->FindFile("Dir::Cache::srcpkgcache");

   // ensure the cache directory exists
   if (CacheFile.empty() == false || SrcCacheFile.empty() == false)
   {
      string dir = _config->FindDir("Dir::Cache");
      size_t const len = dir.size();
      if (len > 5 && dir.find("/apt/", len - 6, 5) == len - 5)
	 dir = dir.substr(0, len - 5);
      if (CacheFile.empty() == false)
	 CreateDirectory(dir, flNotFile(CacheFile));
      if (SrcCacheFile.empty() == false)
	 CreateDirectory(dir, flNotFile(SrcCacheFile));
   }

   // Decide if we can write to the cache
   bool Writeable = false;
   if (CacheFile.empty() == false)
      Writeable = access(flNotFile(CacheFile).c_str(),W_OK) == 0;
   else
      if (SrcCacheFile.empty() == false)
	 Writeable = access(flNotFile(SrcCacheFile).c_str(),W_OK) == 0;
   if (Debug == true)
      std::clog << "Do we have write-access to the cache files? " << (Writeable ? "YES" : "NO") << std::endl;

   if (Writeable == false && AllowMem == false && CacheFile.empty() == false)
      return _error->Error(_("Unable to write to %s"),flNotFile(CacheFile).c_str());

   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));

   // Cache is OK, Fin.
   if (CheckValidity(CacheFile,Files.begin(),Files.end(),OutMap) == true)
   {
      if (Progress != NULL)
	 Progress->OverallProgress(1,1,1,_("Reading package lists"));
      if (Debug == true)
	 std::clog << "pkgcache.bin is valid - no need to build anything" << std::endl;
      return true;
   }
   else if (Debug == true)
	 std::clog << "pkgcache.bin is NOT valid" << std::endl;
   
   /* At this point we know we need to reconstruct the package cache,
      begin. */
   SPtr<FileFd> CacheF;
   SPtr<DynamicMMap> Map;
   if (Writeable == true && CacheFile.empty() == false)
   {
      unlink(CacheFile.c_str());
      CacheF = new FileFd(CacheFile,FileFd::WriteAtomic);
      fchmod(CacheF->Fd(),0644);
      Map = CreateDynamicMMap(CacheF, MMap::Public);
      if (_error->PendingError() == true)
	 return false;
      if (Debug == true)
	 std::clog << "Open filebased MMap" << std::endl;
   }
   else
   {
      // Just build it in memory..
      Map = CreateDynamicMMap(NULL);
      if (Debug == true)
	 std::clog << "Open memory Map (not filebased)" << std::endl;
   }
   
   // Lets try the source cache.
   unsigned long CurrentSize = 0;
   unsigned long TotalSize = 0;
   if (CheckValidity(SrcCacheFile,Files.begin(),
		     Files.begin()+EndOfSource) == true)
   {
      if (Debug == true)
	 std::clog << "srcpkgcache.bin is valid - populate MMap with it." << std::endl;
      // Preload the map with the source cache
      FileFd SCacheF(SrcCacheFile,FileFd::ReadOnly);
      unsigned long const alloc = Map->RawAllocate(SCacheF.Size());
      if ((alloc == 0 && _error->PendingError())
		|| SCacheF.Read((unsigned char *)Map->Data() + alloc,
				SCacheF.Size()) == false)
	 return false;

      TotalSize = ComputeSize(Files.begin()+EndOfSource,Files.end());

      // Build the status cache
      pkgCacheGenerator Gen(Map.Get(),Progress);
      if (_error->PendingError() == true)
	 return false;
      if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		     Files.begin()+EndOfSource,Files.end()) == false)
	 return false;

      // FIXME: move me to a better place
      Gen.FinishCache(Progress);
   }
   else
   {
      if (Debug == true)
	 std::clog << "srcpkgcache.bin is NOT valid - rebuild" << std::endl;
      TotalSize = ComputeSize(Files.begin(),Files.end());
      
      // Build the source cache
      pkgCacheGenerator Gen(Map.Get(),Progress);
      if (_error->PendingError() == true)
	 return false;
      if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		     Files.begin(),Files.begin()+EndOfSource) == false)
	 return false;
      
      // Write it back
      if (Writeable == true && SrcCacheFile.empty() == false)
      {
	 FileFd SCacheF(SrcCacheFile,FileFd::WriteAtomic);
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

      // FIXME: move me to a better place
      Gen.FinishCache(Progress);
   }
   if (Debug == true)
      std::clog << "Caches are ready for shipping" << std::endl;

   if (_error->PendingError() == true)
      return false;
   if (OutMap != 0)
   {
      if (CacheF != 0)
      {
	 delete Map.UnGuard();
	 *OutMap = new MMap(*CacheF,0);
      }
      else
      {
	 *OutMap = Map.UnGuard();
      }      
   }
   
   return true;
}
									/*}}}*/
// CacheGenerator::MakeOnlyStatusCache - Build only a status files cache/*{{{*/
// ---------------------------------------------------------------------
/* */
__deprecated bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap)
   { return pkgCacheGenerator::MakeOnlyStatusCache(&Progress, OutMap); }
bool pkgCacheGenerator::MakeOnlyStatusCache(OpProgress *Progress,DynamicMMap **OutMap)
{
   vector<pkgIndexFile *> Files;
   unsigned long EndOfSource = Files.size();
   if (_system->AddStatusFiles(Files) == false)
      return false;

   SPtr<DynamicMMap> Map = CreateDynamicMMap(NULL);
   unsigned long CurrentSize = 0;
   unsigned long TotalSize = 0;
   
   TotalSize = ComputeSize(Files.begin()+EndOfSource,Files.end());
   
   // Build the status cache
   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));
   pkgCacheGenerator Gen(Map.Get(),Progress);
   if (_error->PendingError() == true)
      return false;
   if (BuildCache(Gen,Progress,CurrentSize,TotalSize,
		  Files.begin()+EndOfSource,Files.end()) == false)
      return false;

   // FIXME: move me to a better place
   Gen.FinishCache(Progress);

   if (_error->PendingError() == true)
      return false;
   *OutMap = Map.UnGuard();
   
   return true;
}
									/*}}}*/

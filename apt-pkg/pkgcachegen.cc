// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcachegen.cc,v 1.53.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################
   
   Package Cache Generator - Generator for the cache structure.
   
   This builds the cache structure from the abstract package list parser. 
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

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
#include <apt-pkg/metaindex.h>
#include <apt-pkg/fileutl.h>

#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include <apti18n.h>
									/*}}}*/
typedef std::vector<pkgIndexFile *>::iterator FileIterator;
template <typename Iter> std::vector<Iter*> pkgCacheGenerator::Dynamic<Iter>::toReMap;

static bool IsDuplicateDescription(pkgCache::DescIterator Desc,
			    MD5SumValue const &CurMd5, std::string const &CurLang);

using std::string;

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
      // this pointer is set in ReMap, but we need it now for WriteUniqString
      Cache.StringItemP = (pkgCache::StringItem *)Map.Data();
      map_ptrloc const idxArchitecture = WriteUniqString(_config->Find("APT::Architecture"));
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
   Cache.HeaderP->CacheFileSize = Map.Size();
   Map.Sync(0,sizeof(pkgCache::Header));
}
									/*}}}*/
void pkgCacheGenerator::ReMap(void const * const oldMap, void const * const newMap) {/*{{{*/
   if (oldMap == newMap)
      return;

   if (_config->FindB("Debug::pkgCacheGen", false))
      std::clog << "Remaping from " << oldMap << " to " << newMap << std::endl;

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

      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      string Arch = List.Architecture();
      string const Version = List.Version();
      if (Version.empty() == true && Arch.empty() == true)
      {
	 // package descriptions
	 if (MergeListGroup(List, PackageName) == false)
	    return false;
	 continue;
      }

      if (Arch.empty() == true)
      {
	 // use the pseudo arch 'none' for arch-less packages
	 Arch = "none";
	 /* We might built a SingleArchCache here, which we don't want to blow up
	    just for these :none packages to a proper MultiArchCache, so just ensure
	    that we have always a native package structure first for SingleArch */
	 pkgCache::PkgIterator NP;
	 Dynamic<pkgCache::PkgIterator> DynPkg(NP);
	 if (NewPackage(NP, PackageName, _config->Find("APT::Architecture")) == false)
	 // TRANSLATOR: The first placeholder is a package name,
	 // the other two should be copied verbatim as they include debug info
	 return _error->Error(_("Error occurred while processing %s (%s%d)"),
			      PackageName.c_str(), "NewPackage", 0);
      }

      // Get a pointer to the package structure
      pkgCache::PkgIterator Pkg;
      Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
      if (NewPackage(Pkg, PackageName, Arch) == false)
	 // TRANSLATOR: The first placeholder is a package name,
	 // the other two should be copied verbatim as they include debug info
	 return _error->Error(_("Error occurred while processing %s (%s%d)"),
			      PackageName.c_str(), "NewPackage", 1);


      if (Version.empty() == true)
      {
	 if (MergeListPackage(List, Pkg) == false)
	    return false;
      }
      else
      {
	 if (MergeListVersion(List, Pkg, Version, OutVer) == false)
	    return false;
      }

      if (OutVer != 0)
      {
	 FoundFileDeps |= List.HasFileDeps();
	 return true;
      }
   }

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

   FoundFileDeps |= List.HasFileDeps();
   return true;
}
// CacheGenerator::MergeListGroup					/*{{{*/
bool pkgCacheGenerator::MergeListGroup(ListParser &List, std::string const &GrpName)
{
   pkgCache::GrpIterator Grp = Cache.FindGrp(GrpName);
   // a group has no data on it's own, only packages have it but these
   // stanzas like this come from Translation- files to add descriptions,
   // but without a version we don't need a description for it…
   if (Grp.end() == true)
      return true;
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);

   pkgCache::PkgIterator Pkg;
   Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
   for (Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
      if (MergeListPackage(List, Pkg) == false)
	 return false;

   return true;
}
									/*}}}*/
// CacheGenerator::MergeListPackage					/*{{{*/
bool pkgCacheGenerator::MergeListPackage(ListParser &List, pkgCache::PkgIterator &Pkg)
{
   // we first process the package, then the descriptions
   // (for deb this package processing is in fact a no-op)
   pkgCache::VerIterator Ver(Cache);
   Dynamic<pkgCache::VerIterator> DynVer(Ver);
   if (List.UsePackage(Pkg, Ver) == false)
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "UsePackage", 1);

   // Find the right version to write the description
   MD5SumValue CurMd5 = List.Description_md5();
   if (CurMd5.Value().empty() == true || List.Description().empty() == true)
      return true;
   std::string CurLang = List.DescriptionLanguage();

   for (Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
   {
      pkgCache::DescIterator VerDesc = Ver.DescriptionList();

      // a version can only have one md5 describing it
      if (VerDesc.end() == true || MD5SumValue(VerDesc.md5()) != CurMd5)
	 continue;

      // don't add a new description if we have one for the given
      // md5 && language
      if (IsDuplicateDescription(VerDesc, CurMd5, CurLang) == true)
	 continue;

      pkgCache::DescIterator Desc;
      Dynamic<pkgCache::DescIterator> DynDesc(Desc);

      map_ptrloc const descindex = NewDescription(Desc, CurLang, CurMd5, VerDesc->md5sum);
      if (unlikely(descindex == 0 && _error->PendingError()))
	 return _error->Error(_("Error occurred while processing %s (%s%d)"),
			      Pkg.Name(), "NewDescription", 1);

      Desc->ParentPkg = Pkg.Index();

      // we add at the end, so that the start is constant as we need
      // that to be able to efficiently share these lists
      VerDesc = Ver.DescriptionList(); // old value might be invalid after ReMap
      for (;VerDesc.end() == false && VerDesc->NextDesc != 0; ++VerDesc);
      map_ptrloc * const LastNextDesc = (VerDesc.end() == true) ? &Ver->DescriptionList : &VerDesc->NextDesc;
      *LastNextDesc = descindex;

      if (NewFileDesc(Desc,List) == false)
	 return _error->Error(_("Error occurred while processing %s (%s%d)"),
			      Pkg.Name(), "NewFileDesc", 1);

      // we can stop here as all "same" versions will share the description
      break;
   }

   return true;
}
									/*}}}*/
// CacheGenerator::MergeListVersion					/*{{{*/
bool pkgCacheGenerator::MergeListVersion(ListParser &List, pkgCache::PkgIterator &Pkg,
					 std::string const &Version, pkgCache::VerIterator* &OutVer)
{
   pkgCache::VerIterator Ver = Pkg.VersionList();
   Dynamic<pkgCache::VerIterator> DynVer(Ver);
   map_ptrloc *LastVer = &Pkg->VersionList;
   void const * oldMap = Map.Data();

   unsigned long const Hash = List.VersionHash();
   if (Ver.end() == false)
   {
      /* We know the list is sorted so we use that fact in the search.
         Insertion of new versions is done with correct sorting */
      int Res = 1;
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
	    return _error->Error(_("Error occurred while processing %s (%s%d)"),
				 Pkg.Name(), "UsePackage", 2);

	 if (NewFileVer(Ver,List) == false)
	    return _error->Error(_("Error occurred while processing %s (%s%d)"),
				 Pkg.Name(), "NewFileVer", 1);

	 // Read only a single record and return
	 if (OutVer != 0)
	 {
	    *OutVer = Ver;
	    return true;
	 }

	 return true;
      }
   }

   // Add a new version
   map_ptrloc const verindex = NewVersion(Ver, Version, Pkg.Index(), Hash, *LastVer);
   if (verindex == 0 && _error->PendingError())
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewVersion", 1);

   if (oldMap != Map.Data())
	 LastVer += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
   *LastVer = verindex;

   if (unlikely(List.NewVersion(Ver) == false))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewVersion", 2);

   if (unlikely(List.UsePackage(Pkg,Ver) == false))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "UsePackage", 3);

   if (unlikely(NewFileVer(Ver,List) == false))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewFileVer", 2);

   pkgCache::GrpIterator Grp = Pkg.Group();
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);

   /* If it is the first version of this package we need to add implicit
      Multi-Arch dependencies to all other package versions in the group now -
      otherwise we just add them for this new version */
   if (Pkg.VersionList()->NextVer == 0)
   {
      pkgCache::PkgIterator P = Grp.PackageList();
      Dynamic<pkgCache::PkgIterator> DynP(P);
      for (; P.end() != true; P = Grp.NextPkg(P))
      {
	 if (P->ID == Pkg->ID)
	    continue;
	 pkgCache::VerIterator V = P.VersionList();
	 Dynamic<pkgCache::VerIterator> DynV(V);
	 for (; V.end() != true; ++V)
	    if (unlikely(AddImplicitDepends(V, Pkg) == false))
	       return _error->Error(_("Error occurred while processing %s (%s%d)"),
				    Pkg.Name(), "AddImplicitDepends", 1);
      }
      /* :none packages are packages without an architecture. They are forbidden by
	 debian-policy, so usually they will only be in (old) dpkg status files -
	 and dpkg will complain about them - and are pretty rare. We therefore do
	 usually not create conflicts while the parent is created, but only if a :none
	 package (= the target) appears. This creates incorrect dependencies on :none
	 for architecture-specific dependencies on the package we copy from, but we
	 will ignore this bug as architecture-specific dependencies are only allowed
	 in jessie and until then the :none packages should be extinct (hopefully).
	 In other words: This should work long enough to allow graceful removal of
	 these packages, it is not supposed to allow users to keep using them … */
      if (strcmp(Pkg.Arch(), "none") == 0)
      {
	 pkgCache::PkgIterator M = Grp.FindPreferredPkg();
	 if (M.end() == false && Pkg != M)
	 {
	    pkgCache::DepIterator D = M.RevDependsList();
	    Dynamic<pkgCache::DepIterator> DynD(D);
	    for (; D.end() == false; ++D)
	    {
	       if ((D->Type != pkgCache::Dep::Conflicts &&
		    D->Type != pkgCache::Dep::DpkgBreaks &&
		    D->Type != pkgCache::Dep::Replaces) ||
		   D.ParentPkg().Group() == Grp)
		  continue;

	       map_ptrloc *OldDepLast = NULL;
	       pkgCache::VerIterator ConVersion = D.ParentVer();
	       Dynamic<pkgCache::VerIterator> DynV(ConVersion);
	       // duplicate the Conflicts/Breaks/Replaces for :none arch
	       NewDepends(Pkg, ConVersion, D->Version,
		     D->CompareOp, D->Type, OldDepLast);
	    }
	 }
      }
   }
   if (unlikely(AddImplicitDepends(Grp, Pkg, Ver) == false))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "AddImplicitDepends", 2);

   // Read only a single record and return
   if (OutVer != 0)
   {
      *OutVer = Ver;
      return true;
   }

   /* Record the Description (it is not translated) */
   MD5SumValue CurMd5 = List.Description_md5();
   if (CurMd5.Value().empty() == true || List.Description().empty() == true)
      return true;
   std::string CurLang = List.DescriptionLanguage();

   /* Before we add a new description we first search in the group for
      a version with a description of the same MD5 - if so we reuse this
      description group instead of creating our own for this version */
   for (pkgCache::PkgIterator P = Grp.PackageList();
	P.end() == false; P = Grp.NextPkg(P))
   {
      for (pkgCache::VerIterator V = P.VersionList();
	   V.end() == false; ++V)
      {
	 if (IsDuplicateDescription(V.DescriptionList(), CurMd5, "") == false)
	    continue;
	 Ver->DescriptionList = V->DescriptionList;
	 return true;
      }
   }

   // We haven't found reusable descriptions, so add the first description
   pkgCache::DescIterator Desc = Ver.DescriptionList();
   Dynamic<pkgCache::DescIterator> DynDesc(Desc);

   map_ptrloc const descindex = NewDescription(Desc, CurLang, CurMd5, 0);
   if (unlikely(descindex == 0 && _error->PendingError()))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewDescription", 2);

   Desc->ParentPkg = Pkg.Index();
   Ver->DescriptionList = descindex;

   if (NewFileDesc(Desc,List) == false)
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewFileDesc", 2);

   return true;
}
									/*}}}*/
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
	 return _error->Error(_("Error occurred while processing %s (%s%d)"),
				PackageName.c_str(), "FindPkg", 1);
      Counter++;
      if (Counter % 100 == 0 && Progress != 0)
	 Progress->Progress(List.Offset());

      unsigned long Hash = List.VersionHash();
      pkgCache::VerIterator Ver = Pkg.VersionList();
      Dynamic<pkgCache::VerIterator> DynVer(Ver);
      for (; Ver.end() == false; ++Ver)
      {
	 if (Ver->Hash == Hash && Version == Ver.VerStr())
	 {
	    if (List.CollectFileProvides(Cache,Ver) == false)
	       return _error->Error(_("Error occurred while processing %s (%s%d)"),
				    PackageName.c_str(), "CollectFileProvides", 1);
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
   map_ptrloc *insertAt = &Cache.HeaderP->GrpHashTable[Hash];
   while (*insertAt != 0 && strcasecmp(Name.c_str(), Cache.StrP + (Cache.GrpP + *insertAt)->Name) > 0)
      insertAt = &(Cache.GrpP + *insertAt)->Next;
   Grp->Next = *insertAt;
   *insertAt = Group;

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
      Grp->FirstPackage = Package;
      // Insert it into the hash table
      unsigned long const Hash = Cache.Hash(Name);
      map_ptrloc *insertAt = &Cache.HeaderP->PkgHashTable[Hash];
      while (*insertAt != 0 && strcasecmp(Name.c_str(), Cache.StrP + (Cache.PkgP + *insertAt)->Name) > 0)
	 insertAt = &(Cache.PkgP + *insertAt)->NextPackage;
      Pkg->NextPackage = *insertAt;
      *insertAt = Package;
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
   // all is mapped to the native architecture
   map_ptrloc const idxArch = (Arch == "all") ? Cache.HeaderP->Architecture : WriteUniqString(Arch.c_str());
   if (unlikely(idxArch == 0))
      return false;
   Pkg->Arch = idxArch;
   Pkg->ID = Cache.HeaderP->PackageCount++;

   return true;
}
									/*}}}*/
// CacheGenerator::AddImplicitDepends					/*{{{*/
bool pkgCacheGenerator::AddImplicitDepends(pkgCache::GrpIterator &G,
					   pkgCache::PkgIterator &P,
					   pkgCache::VerIterator &V)
{
   // copy P.Arch() into a string here as a cache remap
   // in NewDepends() later may alter the pointer location
   string Arch = P.Arch() == NULL ? "" : P.Arch();
   map_ptrloc *OldDepLast = NULL;
   /* MultiArch handling introduces a lot of implicit Dependencies:
      - MultiArch: same → Co-Installable if they have the same version
      - All others conflict with all other group members */
   bool const coInstall = ((V->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same);
   pkgCache::PkgIterator D = G.PackageList();
   Dynamic<pkgCache::PkgIterator> DynD(D);
   map_ptrloc const VerStrIdx = V->VerStr;
   for (; D.end() != true; D = G.NextPkg(D))
   {
      if (Arch == D.Arch() || D->VersionList == 0)
	 continue;
      /* We allow only one installed arch at the time
	 per group, therefore each group member conflicts
	 with all other group members */
      if (coInstall == true)
      {
	 // Replaces: ${self}:other ( << ${binary:Version})
	 NewDepends(D, V, VerStrIdx,
		    pkgCache::Dep::Less, pkgCache::Dep::Replaces,
		    OldDepLast);
	 // Breaks: ${self}:other (!= ${binary:Version})
	 NewDepends(D, V, VerStrIdx,
		    pkgCache::Dep::NotEquals, pkgCache::Dep::DpkgBreaks,
		    OldDepLast);
      } else {
	 // Conflicts: ${self}:other
	 NewDepends(D, V, 0,
		    pkgCache::Dep::NoOp, pkgCache::Dep::Conflicts,
		    OldDepLast);
      }
   }
   return true;
}
bool pkgCacheGenerator::AddImplicitDepends(pkgCache::VerIterator &V,
					   pkgCache::PkgIterator &D)
{
   /* MultiArch handling introduces a lot of implicit Dependencies:
      - MultiArch: same → Co-Installable if they have the same version
      - All others conflict with all other group members */
   map_ptrloc *OldDepLast = NULL;
   bool const coInstall = ((V->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same);
   if (coInstall == true)
   {
      map_ptrloc const VerStrIdx = V->VerStr;
      // Replaces: ${self}:other ( << ${binary:Version})
      NewDepends(D, V, VerStrIdx,
		 pkgCache::Dep::Less, pkgCache::Dep::Replaces,
		 OldDepLast);
      // Breaks: ${self}:other (!= ${binary:Version})
      NewDepends(D, V, VerStrIdx,
		 pkgCache::Dep::NotEquals, pkgCache::Dep::DpkgBreaks,
		 OldDepLast);
   } else {
      // Conflicts: ${self}:other
      NewDepends(D, V, 0,
		 pkgCache::Dep::NoOp, pkgCache::Dep::Conflicts,
		 OldDepLast);
   }
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
   for (pkgCache::VerFileIterator V = Ver.FileList(); V.end() == false; ++V)
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
					    map_ptrloc const ParentPkg,
					    unsigned long const Hash,
					    unsigned long Next)
{
   // Get a structure
   map_ptrloc const Version = AllocateInMap(sizeof(pkgCache::Version));
   if (Version == 0)
      return 0;
   
   // Fill it in
   Ver = pkgCache::VerIterator(Cache,Cache.VerP + Version);
   //Dynamic<pkgCache::VerIterator> DynV(Ver); // caller MergeListVersion already takes care of it
   Ver->NextVer = Next;
   Ver->ParentPkg = ParentPkg;
   Ver->Hash = Hash;
   Ver->ID = Cache.HeaderP->VersionCount++;

   // try to find the version string in the group for reuse
   pkgCache::PkgIterator Pkg = Ver.ParentPkg();
   pkgCache::GrpIterator Grp = Pkg.Group();
   if (Pkg.end() == false && Grp.end() == false)
   {
      for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() == false; P = Grp.NextPkg(P))
      {
	 if (Pkg == P)
	    continue;
	 for (pkgCache::VerIterator V = P.VersionList(); V.end() == false; ++V)
	 {
	    int const cmp = strcmp(V.VerStr(), VerStr.c_str());
	    if (cmp == 0)
	    {
	       Ver->VerStr = V->VerStr;
	       return Version;
	    }
	    else if (cmp < 0)
	       break;
	 }
      }
   }
   // haven't found the version string, so create
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
   for (pkgCache::DescFileIterator D = Desc.FileList(); D.end() == false; ++D)
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
					    map_ptrloc idxmd5str)
{
   // Get a structure
   map_ptrloc const Description = AllocateInMap(sizeof(pkgCache::Description));
   if (Description == 0)
      return 0;

   // Fill it in
   Desc = pkgCache::DescIterator(Cache,Cache.DescP + Description);
   Desc->ID = Cache.HeaderP->DescriptionCount++;
   map_ptrloc const idxlanguage_code = WriteUniqString(Lang);
   if (unlikely(idxlanguage_code == 0))
      return 0;
   Desc->language_code = idxlanguage_code;

   if (idxmd5str != 0)
      Desc->md5sum = idxmd5str;
   else
   {
      map_ptrloc const idxmd5sum = WriteStringInMap(md5sum.Value());
      if (unlikely(idxmd5sum == 0))
	 return 0;
      Desc->md5sum = idxmd5sum;
   }

   return Description;
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
				   map_ptrloc* &OldDepLast)
{
   map_ptrloc index = 0;
   if (Version.empty() == false)
   {
      int const CmpOp = Op & 0x0F;
      // =-deps are used (79:1) for lockstep on same-source packages (e.g. data-packages)
      if (CmpOp == pkgCache::Dep::Equals && strcmp(Version.c_str(), Ver.VerStr()) == 0)
	 index = Ver->VerStr;

      if (index == 0)
      {
	 void const * const oldMap = Map.Data();
	 index = WriteStringInMap(Version);
	 if (unlikely(index == 0))
	    return false;
	 if (OldDepLast != 0 && oldMap != Map.Data())
	    OldDepLast += (map_ptrloc*) Map.Data() - (map_ptrloc*) oldMap;
      }
   }
   return NewDepends(Pkg, Ver, index, Op, Type, OldDepLast);
}
bool pkgCacheGenerator::NewDepends(pkgCache::PkgIterator &Pkg,
				   pkgCache::VerIterator &Ver,
				   map_ptrloc const Version,
				   unsigned int const &Op,
				   unsigned int const &Type,
				   map_ptrloc* &OldDepLast)
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
   Dep->Version = Version;
   Dep->ID = Cache.HeaderP->DependsCount++;

   // Link it to the package
   Dep->Package = Pkg.Index();
   Dep->NextRevDepends = Pkg->RevDepends;
   Pkg->RevDepends = Dep.Index();

   // Do we know where to link the Dependency to?
   if (OldDepLast == NULL)
   {
      OldDepLast = &Ver->DependsList;
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; ++D)
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
   // we don't create 'none' packages and their dependencies if we can avoid it …
   if (Pkg.end() == true && Arch == "none" && strcmp(Ver.ParentPkg().Arch(), "none") != 0)
      return true;
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
   if (Ver.ParentPkg().Name() == PkgName && (PkgArch == Ver.ParentPkg().Arch() ||
	(PkgArch == "all" && strcmp((Cache.StrP + Cache.HeaderP->Architecture), Ver.ParentPkg().Arch()) == 0)))
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
   if (Version.empty() == false) {
      map_ptrloc const idxProvideVersion = WriteString(Version);
      Prv->ProvideVersion = idxProvideVersion;
      if (unlikely(idxProvideVersion == 0))
	 return false;
   }
   
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
static bool CheckValidity(const string &CacheFile, 
                          pkgSourceList &List,
                          FileIterator Start, 
                          FileIterator End,
                          MMap **OutMap = 0)
{
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);
   // No file, certainly invalid
   if (CacheFile.empty() == true || FileExists(CacheFile) == false)
   {
      if (Debug == true)
	 std::clog << "CacheFile doesn't exist" << std::endl;
      return false;
   }

   if (List.GetLastModifiedTime() > GetModificationTime(CacheFile))
   {
      if (Debug == true)
	 std::clog << "sources.list is newer than the cache" << std::endl;
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
   for (; Start != End; ++Start)
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
   for (; Start != End; ++Start)
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
   for (I = Start; I != End; ++I)
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
      for (I = Start; I != End; ++I)
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

   std::vector<pkgIndexFile *> Files;
   for (std::vector<metaIndex *>::const_iterator i = List.begin();
        i != List.end();
        ++i)
   {
      std::vector <pkgIndexFile *> *Indexes = (*i)->GetIndexFiles();
      for (std::vector<pkgIndexFile *>::const_iterator j = Indexes->begin();
	   j != Indexes->end();
	   ++j)
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
   if (CheckValidity(CacheFile, List, Files.begin(),Files.end(),OutMap) == true)
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
      _error->PushToStack();
      unlink(CacheFile.c_str());
      CacheF = new FileFd(CacheFile,FileFd::WriteAtomic);
      fchmod(CacheF->Fd(),0644);
      Map = CreateDynamicMMap(CacheF, MMap::Public);
      if (_error->PendingError() == true)
      {
	 delete CacheF.UnGuard();
	 delete Map.UnGuard();
	 if (Debug == true)
	    std::clog << "Open filebased MMap FAILED" << std::endl;
	 Writeable = false;
	 if (AllowMem == false)
	 {
	    _error->MergeWithStack();
	    return false;
	 }
	 _error->RevertToStack();
      }
      else
      {
	 _error->MergeWithStack();
	 if (Debug == true)
	    std::clog << "Open filebased MMap" << std::endl;
      }
   }
   if (Writeable == false || CacheFile.empty() == true)
   {
      // Just build it in memory..
      Map = CreateDynamicMMap(NULL);
      if (Debug == true)
	 std::clog << "Open memory Map (not filebased)" << std::endl;
   }
   
   // Lets try the source cache.
   unsigned long CurrentSize = 0;
   unsigned long TotalSize = 0;
   if (CheckValidity(SrcCacheFile, List, Files.begin(),
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
   std::vector<pkgIndexFile *> Files;
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

   if (_error->PendingError() == true)
      return false;
   *OutMap = Map.UnGuard();
   
   return true;
}
									/*}}}*/
// IsDuplicateDescription						/*{{{*/
static bool IsDuplicateDescription(pkgCache::DescIterator Desc,
			    MD5SumValue const &CurMd5, std::string const &CurLang)
{
   // Descriptions in the same link-list have all the same md5
   if (Desc.end() == true || MD5SumValue(Desc.md5()) != CurMd5)
      return false;
   for (; Desc.end() == false; ++Desc)
      if (Desc.LanguageCode() == CurLang)
	 return true;
   return false;
}
									/*}}}*/
// CacheGenerator::FinishCache						/*{{{*/
bool pkgCacheGenerator::FinishCache(OpProgress *Progress)
{
   return true;
}
									/*}}}*/

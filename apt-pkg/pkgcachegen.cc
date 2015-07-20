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
#include <apt-pkg/strutl.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/metaindex.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashsum_template.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <stddef.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

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
		     CurrentRlsFile(NULL), CurrentFile(NULL), FoundFileDeps(0), d(NULL)
{
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

      // make room for the hashtables for packages and groups
      if (Map.RawAllocate(2 * (Cache.HeaderP->GetHashTableSize() * sizeof(map_pointer_t))) == 0)
	 return;

      map_stringitem_t const idxVerSysName = WriteStringInMap(_system->VS->Label);
      if (unlikely(idxVerSysName == 0))
	 return;
      Cache.HeaderP->VerSysName = idxVerSysName;
      map_stringitem_t const idxArchitecture = StoreString(MIXED, _config->Find("APT::Architecture"));
      if (unlikely(idxArchitecture == 0))
	 return;
      Cache.HeaderP->Architecture = idxArchitecture;

      std::vector<std::string> archs = APT::Configuration::getArchitectures();
      if (archs.size() > 1)
      {
	 std::vector<std::string>::const_iterator a = archs.begin();
	 std::string list = *a;
	 for (++a; a != archs.end(); ++a)
	    list.append(",").append(*a);
	 map_stringitem_t const idxArchitectures = WriteStringInMap(list);
	 if (unlikely(idxArchitectures == 0))
	    return;
	 Cache.HeaderP->SetArchitectures(idxArchitectures);
      }
      else
	 Cache.HeaderP->SetArchitectures(idxArchitecture);

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

   CurrentFile += (pkgCache::PackageFile const * const) newMap - (pkgCache::PackageFile const * const) oldMap;
   CurrentRlsFile += (pkgCache::ReleaseFile const * const) newMap - (pkgCache::ReleaseFile const * const) oldMap;

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
   for (std::vector<pkgCache::RlsFileIterator*>::const_iterator i = Dynamic<pkgCache::RlsFileIterator>::toReMap.begin();
	i != Dynamic<pkgCache::RlsFileIterator>::toReMap.end(); ++i)
      (*i)->ReMap(oldMap, newMap);
}									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_stringitem_t pkgCacheGenerator::WriteStringInMap(const char *String,
					const unsigned long &Len) {
   void const * const oldMap = Map.Data();
   map_stringitem_t const index = Map.WriteString(String, Len);
   if (index != 0)
      ReMap(oldMap, Map.Data());
   return index;
}
									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_stringitem_t pkgCacheGenerator::WriteStringInMap(const char *String) {
   void const * const oldMap = Map.Data();
   map_stringitem_t const index = Map.WriteString(String);
   if (index != 0)
      ReMap(oldMap, Map.Data());
   return index;
}
									/*}}}*/
map_pointer_t pkgCacheGenerator::AllocateInMap(const unsigned long &size) {/*{{{*/
   void const * const oldMap = Map.Data();
   map_pointer_t const index = Map.Allocate(size);
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

   if (Cache.HeaderP->PackageCount >= std::numeric_limits<map_id_t>::max())
      return _error->Error(_("Wow, you exceeded the number of package "
			     "names this APT is capable of."));
   if (Cache.HeaderP->VersionCount >= std::numeric_limits<map_id_t>::max())
      return _error->Error(_("Wow, you exceeded the number of versions "
			     "this APT is capable of."));
   if (Cache.HeaderP->DescriptionCount >= std::numeric_limits<map_id_t>::max())
      return _error->Error(_("Wow, you exceeded the number of descriptions "
			     "this APT is capable of."));
   if (Cache.HeaderP->DependsCount >= std::numeric_limits<map_id_t>::max())
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
   if (CurMd5.Value().empty() == true && List.Description("").empty() == true)
      return true;
   std::vector<std::string> availDesc = List.AvailableDescriptionLanguages();
   for (Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
   {
      pkgCache::DescIterator VerDesc = Ver.DescriptionList();

      // a version can only have one md5 describing it
      if (VerDesc.end() == true || MD5SumValue(VerDesc.md5()) != CurMd5)
	 continue;

      map_stringitem_t md5idx = VerDesc->md5sum;
      for (std::vector<std::string>::const_iterator CurLang = availDesc.begin(); CurLang != availDesc.end(); ++CurLang)
      {
	 // don't add a new description if we have one for the given
	 // md5 && language
	 if (IsDuplicateDescription(VerDesc, CurMd5, *CurLang) == true)
	    continue;

	 AddNewDescription(List, Ver, *CurLang, CurMd5, md5idx);
      }

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
   map_pointer_t *LastVer = &Pkg->VersionList;
   void const * oldMap = Map.Data();

   unsigned short const Hash = List.VersionHash();
   if (Ver.end() == false)
   {
      /* We know the list is sorted so we use that fact in the search.
         Insertion of new versions is done with correct sorting */
      int Res = 1;
      for (; Ver.end() == false; LastVer = &Ver->NextVer, ++Ver)
      {
	 Res = Cache.VS->CmpVersion(Version,Ver.VerStr());
	 // Version is higher as current version - insert here
	 if (Res > 0)
	    break;
	 // Versionstrings are equal - is hash also equal?
	 if (Res == 0 && List.SameVersion(Hash, Ver) == true)
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
   map_pointer_t const verindex = NewVersion(Ver, Version, Pkg.Index(), Hash, *LastVer);
   if (verindex == 0 && _error->PendingError())
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
			   Pkg.Name(), "NewVersion", 1);

   if (oldMap != Map.Data())
	 LastVer += (map_pointer_t const * const) Map.Data() - (map_pointer_t const * const) oldMap;
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

	       map_pointer_t *OldDepLast = NULL;
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

   /* Record the Description(s) based on their master md5sum */
   MD5SumValue CurMd5 = List.Description_md5();
   if (CurMd5.Value().empty() == true && List.Description("").empty() == true)
      return true;

   /* Before we add a new description we first search in the group for
      a version with a description of the same MD5 - if so we reuse this
      description group instead of creating our own for this version */
   for (pkgCache::PkgIterator P = Grp.PackageList();
	P.end() == false; P = Grp.NextPkg(P))
   {
      for (pkgCache::VerIterator V = P.VersionList();
	   V.end() == false; ++V)
      {
	 if (V->DescriptionList == 0 || MD5SumValue(V.DescriptionList().md5()) != CurMd5)
	    continue;
	 Ver->DescriptionList = V->DescriptionList;
      }
   }

   // We haven't found reusable descriptions, so add the first description(s)
   map_stringitem_t md5idx = Ver->DescriptionList == 0 ? 0 : Ver.DescriptionList()->md5sum;
   std::vector<std::string> availDesc = List.AvailableDescriptionLanguages();
   for (std::vector<std::string>::const_iterator CurLang = availDesc.begin(); CurLang != availDesc.end(); ++CurLang)
      if (AddNewDescription(List, Ver, *CurLang, CurMd5, md5idx) == false)
	 return false;
   return true;
}
									/*}}}*/
bool pkgCacheGenerator::AddNewDescription(ListParser &List, pkgCache::VerIterator &Ver, std::string const &lang, MD5SumValue const &CurMd5, map_stringitem_t &md5idx) /*{{{*/
{
   pkgCache::DescIterator Desc;
   Dynamic<pkgCache::DescIterator> DynDesc(Desc);

   map_pointer_t const descindex = NewDescription(Desc, lang, CurMd5, md5idx);
   if (unlikely(descindex == 0 && _error->PendingError()))
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
	    Ver.ParentPkg().Name(), "NewDescription", 1);

   md5idx = Desc->md5sum;
   Desc->ParentPkg = Ver.ParentPkg().Index();

   // we add at the end, so that the start is constant as we need
   // that to be able to efficiently share these lists
   pkgCache::DescIterator VerDesc = Ver.DescriptionList(); // old value might be invalid after ReMap
   for (;VerDesc.end() == false && VerDesc->NextDesc != 0; ++VerDesc);
   map_pointer_t * const LastNextDesc = (VerDesc.end() == true) ? &Ver->DescriptionList : &VerDesc->NextDesc;
   *LastNextDesc = descindex;

   if (NewFileDesc(Desc,List) == false)
      return _error->Error(_("Error occurred while processing %s (%s%d)"),
	    Ver.ParentPkg().Name(), "NewFileDesc", 1);

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

      unsigned short Hash = List.VersionHash();
      pkgCache::VerIterator Ver = Pkg.VersionList();
      Dynamic<pkgCache::VerIterator> DynVer(Ver);
      for (; Ver.end() == false; ++Ver)
      {
	 if (List.SameVersion(Hash, Ver) == true && Version == Ver.VerStr())
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
   map_pointer_t const Group = AllocateInMap(sizeof(pkgCache::Group));
   if (unlikely(Group == 0))
      return false;

   Grp = pkgCache::GrpIterator(Cache, Cache.GrpP + Group);
   map_stringitem_t const idxName = StoreString(PKGNAME, Name);
   if (unlikely(idxName == 0))
      return false;
   Grp->Name = idxName;

   // Insert it into the hash table
   unsigned long const Hash = Cache.Hash(Name);
   map_pointer_t *insertAt = &Cache.HeaderP->GrpHashTableP()[Hash];
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
   map_pointer_t const Package = AllocateInMap(sizeof(pkgCache::Package));
   if (unlikely(Package == 0))
      return false;
   Pkg = pkgCache::PkgIterator(Cache,Cache.PkgP + Package);

   // Set the name, arch and the ID
   APT_IGNORE_DEPRECATED(Pkg->Name = Grp->Name;)
   Pkg->Group = Grp.Index();
   // all is mapped to the native architecture
   map_stringitem_t const idxArch = (Arch == "all") ? Cache.HeaderP->Architecture : StoreString(MIXED, Arch);
   if (unlikely(idxArch == 0))
      return false;
   Pkg->Arch = idxArch;
   Pkg->ID = Cache.HeaderP->PackageCount++;

   // Insert the package into our package list
   if (Grp->FirstPackage == 0) // the group is new
   {
      Grp->FirstPackage = Package;
      // Insert it into the hash table
      map_id_t const Hash = Cache.Hash(Name);
      map_pointer_t *insertAt = &Cache.HeaderP->PkgHashTableP()[Hash];
      while (*insertAt != 0 && strcasecmp(Name.c_str(), Cache.StrP + (Cache.GrpP + (Cache.PkgP + *insertAt)->Group)->Name) > 0)
	 insertAt = &(Cache.PkgP + *insertAt)->NextPackage;
      Pkg->NextPackage = *insertAt;
      *insertAt = Package;
   }
   else // Group the Packages together
   {
      // but first get implicit provides done
      if (APT::Configuration::checkArchitecture(Pkg.Arch()) == true)
      {
	 pkgCache::PkgIterator const M = Grp.FindPreferredPkg(false); // native or any foreign pkg will do
	 if (M.end() == false)
	    for (pkgCache::PrvIterator Prv = M.ProvidesList(); Prv.end() == false; ++Prv)
	    {
	       if ((Prv->Flags & pkgCache::Flag::ArchSpecific) != 0)
		  continue;
	       pkgCache::VerIterator Ver = Prv.OwnerVer();
	       if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed ||
	           ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign &&
			(Prv->Flags & pkgCache::Flag::MultiArchImplicit) == 0))
		  if (NewProvides(Ver, Pkg, Prv->ProvideVersion, Prv->Flags) == false)
		     return false;
	    }

	 for (pkgCache::PkgIterator P = Grp.PackageList(); P.end() == false;  P = Grp.NextPkg(P))
	    for (pkgCache::VerIterator Ver = P.VersionList(); Ver.end() == false; ++Ver)
	       if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
		  if (NewProvides(Ver, Pkg, Ver->VerStr, pkgCache::Flag::MultiArchImplicit) == false)
		     return false;
      }
      // and negative dependencies, don't forget negative dependencies
      {
	 pkgCache::PkgIterator const M = Grp.FindPreferredPkg(false);
	 if (M.end() == false)
	    for (pkgCache::DepIterator Dep = M.RevDependsList(); Dep.end() == false; ++Dep)
	    {
	       if ((Dep->CompareOp & (pkgCache::Dep::ArchSpecific | pkgCache::Dep::MultiArchImplicit)) != 0)
		  continue;
	       if (Dep->Type != pkgCache::Dep::DpkgBreaks && Dep->Type != pkgCache::Dep::Conflicts &&
		     Dep->Type != pkgCache::Dep::Replaces)
		  continue;
	       pkgCache::VerIterator Ver = Dep.ParentVer();
	       map_pointer_t * unused = NULL;
	       if (NewDepends(Pkg, Ver, Dep->Version, Dep->CompareOp, Dep->Type, unused) == false)
		  return false;
	    }
      }

      // this package is the new last package
      pkgCache::PkgIterator LastPkg(Cache, Cache.PkgP + Grp->LastPackage);
      Pkg->NextPackage = LastPkg->NextPackage;
      LastPkg->NextPackage = Package;
   }
   Grp->LastPackage = Package;
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
   map_pointer_t *OldDepLast = NULL;
   /* MultiArch handling introduces a lot of implicit Dependencies:
      - MultiArch: same → Co-Installable if they have the same version
      - All others conflict with all other group members */
   bool const coInstall = ((V->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same);
   pkgCache::PkgIterator D = G.PackageList();
   Dynamic<pkgCache::PkgIterator> DynD(D);
   map_stringitem_t const VerStrIdx = V->VerStr;
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
		    pkgCache::Dep::Less | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::Replaces,
		    OldDepLast);
	 // Breaks: ${self}:other (!= ${binary:Version})
	 NewDepends(D, V, VerStrIdx,
		    pkgCache::Dep::NotEquals | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::DpkgBreaks,
		    OldDepLast);
      } else {
	 // Conflicts: ${self}:other
	 NewDepends(D, V, 0,
		    pkgCache::Dep::NoOp | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::Conflicts,
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
   map_pointer_t *OldDepLast = NULL;
   bool const coInstall = ((V->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same);
   if (coInstall == true)
   {
      map_stringitem_t const VerStrIdx = V->VerStr;
      // Replaces: ${self}:other ( << ${binary:Version})
      NewDepends(D, V, VerStrIdx,
		 pkgCache::Dep::Less | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::Replaces,
		 OldDepLast);
      // Breaks: ${self}:other (!= ${binary:Version})
      NewDepends(D, V, VerStrIdx,
		 pkgCache::Dep::NotEquals | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::DpkgBreaks,
		 OldDepLast);
   } else {
      // Conflicts: ${self}:other
      NewDepends(D, V, 0,
		 pkgCache::Dep::NoOp | pkgCache::Dep::MultiArchImplicit, pkgCache::Dep::Conflicts,
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
   map_pointer_t const VerFile = AllocateInMap(sizeof(pkgCache::VerFile));
   if (VerFile == 0)
      return false;
   
   pkgCache::VerFileIterator VF(Cache,Cache.VerFileP + VerFile);
   VF->File = CurrentFile - Cache.PkgFileP;
   
   // Link it to the end of the list
   map_pointer_t *Last = &Ver->FileList;
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
map_pointer_t pkgCacheGenerator::NewVersion(pkgCache::VerIterator &Ver,
					    const string &VerStr,
					    map_pointer_t const ParentPkg,
					    unsigned short const Hash,
					    map_pointer_t const Next)
{
   // Get a structure
   map_pointer_t const Version = AllocateInMap(sizeof(pkgCache::Version));
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
   map_stringitem_t const idxVerStr = StoreString(VERSIONNUMBER, VerStr);
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
   map_pointer_t const DescFile = AllocateInMap(sizeof(pkgCache::DescFile));
   if (DescFile == 0)
      return false;

   pkgCache::DescFileIterator DF(Cache,Cache.DescFileP + DescFile);
   DF->File = CurrentFile - Cache.PkgFileP;

   // Link it to the end of the list
   map_pointer_t *Last = &Desc->FileList;
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
map_pointer_t pkgCacheGenerator::NewDescription(pkgCache::DescIterator &Desc,
					    const string &Lang,
					    const MD5SumValue &md5sum,
					    map_stringitem_t const idxmd5str)
{
   // Get a structure
   map_pointer_t const Description = AllocateInMap(sizeof(pkgCache::Description));
   if (Description == 0)
      return 0;

   // Fill it in
   Desc = pkgCache::DescIterator(Cache,Cache.DescP + Description);
   Desc->ID = Cache.HeaderP->DescriptionCount++;
   map_stringitem_t const idxlanguage_code = StoreString(MIXED, Lang);
   if (unlikely(idxlanguage_code == 0))
      return 0;
   Desc->language_code = idxlanguage_code;

   if (idxmd5str != 0)
      Desc->md5sum = idxmd5str;
   else
   {
      map_stringitem_t const idxmd5sum = WriteStringInMap(md5sum.Value());
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
				   map_pointer_t const Version,
				   uint8_t const Op,
				   uint8_t const Type,
				   map_pointer_t* &OldDepLast)
{
   void const * const oldMap = Map.Data();
   // Get a structure
   map_pointer_t const Dependency = AllocateInMap(sizeof(pkgCache::Dependency));
   if (unlikely(Dependency == 0))
      return false;

   bool isDuplicate = false;
   map_pointer_t DependencyData = 0;
   map_pointer_t PreviousData = 0;
   if (Pkg->RevDepends != 0)
   {
      pkgCache::Dependency const * const L = Cache.DepP + Pkg->RevDepends;
      DependencyData = L->DependencyData;
      do {
	 pkgCache::DependencyData const * const D = Cache.DepDataP + DependencyData;
	 if (Version > D->Version)
	    break;
	 if (D->Version == Version && D->Type == Type && D->CompareOp == Op)
	 {
	    isDuplicate = true;
	    break;
	 }
	 PreviousData = DependencyData;
	 DependencyData = D->NextData;
      } while (DependencyData != 0);
   }

   if (isDuplicate == false)
   {
      DependencyData = AllocateInMap(sizeof(pkgCache::DependencyData));
      if (unlikely(DependencyData == 0))
        return false;
   }

   pkgCache::Dependency * Link = Cache.DepP + Dependency;
   Link->ParentVer = Ver.Index();
   Link->DependencyData = DependencyData;
   Link->ID = Cache.HeaderP->DependsCount++;

   pkgCache::DepIterator Dep(Cache, Link);
   if (isDuplicate == false)
   {
      Dep->Type = Type;
      Dep->CompareOp = Op;
      Dep->Version = Version;
      Dep->Package = Pkg.Index();
      ++Cache.HeaderP->DependsDataCount;
      if (PreviousData != 0)
      {
	 pkgCache::DependencyData * const D = Cache.DepDataP + PreviousData;
	 Dep->NextData = D->NextData;
	 D->NextData = DependencyData;
      }
      else if (Pkg->RevDepends != 0)
      {
	 pkgCache::Dependency const * const D = Cache.DepP + Pkg->RevDepends;
	 Dep->NextData = D->DependencyData;
      }
   }

   if (isDuplicate == true || PreviousData != 0)
   {
      pkgCache::Dependency * const L = Cache.DepP + Pkg->RevDepends;
      Link->NextRevDepends = L->NextRevDepends;
      L->NextRevDepends = Dependency;
   }
   else
   {
      Link->NextRevDepends = Pkg->RevDepends;
      Pkg->RevDepends = Dependency;
   }


   // Do we know where to link the Dependency to?
   if (OldDepLast == NULL)
   {
      OldDepLast = &Ver->DependsList;
      for (pkgCache::DepIterator D = Ver.DependsList(); D.end() == false; ++D)
	 OldDepLast = &D->NextDepends;
   } else if (oldMap != Map.Data())
      OldDepLast += (map_pointer_t const * const) Map.Data() - (map_pointer_t const * const) oldMap;

   Dep->NextDepends = *OldDepLast;
   *OldDepLast = Dependency;
   OldDepLast = &Dep->NextDepends;
   return true;
}
									/*}}}*/
// ListParser::NewDepends - Create the environment for a new dependency	/*{{{*/
// ---------------------------------------------------------------------
/* This creates a Group and the Package to link this dependency to if
   needed and handles also the caching of the old endpoint */
bool pkgCacheListParser::NewDepends(pkgCache::VerIterator &Ver,
					       const string &PackageName,
					       const string &Arch,
					       const string &Version,
					       uint8_t const Op,
					       uint8_t const Type)
{
   pkgCache::GrpIterator Grp;
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);
   if (unlikely(Owner->NewGroup(Grp, PackageName) == false))
      return false;

   // Is it a file dependency?
   if (unlikely(PackageName[0] == '/'))
      FoundFileDeps = true;

   map_stringitem_t idxVersion = 0;
   if (Version.empty() == false)
   {
      int const CmpOp = Op & 0x0F;
      // =-deps are used (79:1) for lockstep on same-source packages (e.g. data-packages)
      if (CmpOp == pkgCache::Dep::Equals && strcmp(Version.c_str(), Ver.VerStr()) == 0)
	 idxVersion = Ver->VerStr;

      if (idxVersion == 0)
      {
	 idxVersion = StoreString(pkgCacheGenerator::VERSIONNUMBER, Version);
	 if (unlikely(idxVersion == 0))
	    return false;
      }
   }

   bool const isNegative = (Type == pkgCache::Dep::DpkgBreaks ||
	 Type == pkgCache::Dep::Conflicts ||
	 Type == pkgCache::Dep::Replaces);

   pkgCache::PkgIterator Pkg;
   Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
   if (isNegative == false || (Op & pkgCache::Dep::ArchSpecific) == pkgCache::Dep::ArchSpecific || Grp->FirstPackage == 0)
   {
      // Locate the target package
      Pkg = Grp.FindPkg(Arch);
      if (Pkg.end() == true) {
	 if (unlikely(Owner->NewPackage(Pkg, PackageName, Arch) == false))
	    return false;
      }

      /* Caching the old end point speeds up generation substantially */
      if (OldDepVer != Ver) {
	 OldDepLast = NULL;
	 OldDepVer = Ver;
      }

      return Owner->NewDepends(Pkg, Ver, idxVersion, Op, Type, OldDepLast);
   }
   else
   {
      /* Caching the old end point speeds up generation substantially */
      if (OldDepVer != Ver) {
	 OldDepLast = NULL;
	 OldDepVer = Ver;
      }

      for (Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
      {
	 if (Owner->NewDepends(Pkg, Ver, idxVersion, Op, Type, OldDepLast) == false)
	       return false;
      }
   }
   return true;
}
									/*}}}*/
// ListParser::NewProvides - Create a Provides element			/*{{{*/
bool pkgCacheListParser::NewProvides(pkgCache::VerIterator &Ver,
						const string &PkgName,
						const string &PkgArch,
						const string &Version,
						uint8_t const Flags)
{
   pkgCache const &Cache = Owner->Cache;

   // We do not add self referencing provides
   if (Ver.ParentPkg().Name() == PkgName && (PkgArch == Ver.ParentPkg().Arch() ||
	(PkgArch == "all" && strcmp((Cache.StrP + Cache.HeaderP->Architecture), Ver.ParentPkg().Arch()) == 0)))
      return true;

   // Locate the target package
   pkgCache::PkgIterator Pkg;
   Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
   if (unlikely(Owner->NewPackage(Pkg,PkgName, PkgArch) == false))
      return false;

   map_stringitem_t idxProvideVersion = 0;
   if (Version.empty() == false) {
      idxProvideVersion = StoreString(pkgCacheGenerator::VERSIONNUMBER, Version);
      if (unlikely(idxProvideVersion == 0))
	 return false;
   }
   return Owner->NewProvides(Ver, Pkg, idxProvideVersion, Flags);
}
bool pkgCacheGenerator::NewProvides(pkgCache::VerIterator &Ver,
				    pkgCache::PkgIterator &Pkg,
				    map_pointer_t const ProvideVersion,
				    uint8_t const Flags)
{
   // Get a structure
   map_pointer_t const Provides = AllocateInMap(sizeof(pkgCache::Provides));
   if (unlikely(Provides == 0))
      return false;
   ++Cache.HeaderP->ProvidesCount;

   // Fill it in
   pkgCache::PrvIterator Prv(Cache,Cache.ProvideP + Provides,Cache.PkgP);
   Prv->Version = Ver.Index();
   Prv->ProvideVersion = ProvideVersion;
   Prv->Flags = Flags;
   Prv->NextPkgProv = Ver->ProvidesList;
   Ver->ProvidesList = Prv.Index();

   // Link it to the package
   Prv->ParentPkg = Pkg.Index();
   Prv->NextProvides = Pkg->ProvidesList;
   Pkg->ProvidesList = Prv.Index();
   return true;
}
									/*}}}*/
// ListParser::NewProvidesAllArch - add provides for all architectures	/*{{{*/
bool pkgCacheListParser::NewProvidesAllArch(pkgCache::VerIterator &Ver, string const &Package,
				string const &Version, uint8_t const Flags) {
   pkgCache &Cache = Owner->Cache;
   pkgCache::GrpIterator const Grp = Cache.FindGrp(Package);
   if (Grp.end() == true)
      return NewProvides(Ver, Package, Cache.NativeArch(), Version, Flags);
   else
   {
      map_stringitem_t idxProvideVersion = 0;
      if (Version.empty() == false) {
	 idxProvideVersion = StoreString(pkgCacheGenerator::VERSIONNUMBER, Version);
	 if (unlikely(idxProvideVersion == 0))
	    return false;
      }

      bool const isImplicit = (Flags & pkgCache::Flag::MultiArchImplicit) == pkgCache::Flag::MultiArchImplicit;
      bool const isArchSpecific = (Flags & pkgCache::Flag::ArchSpecific) == pkgCache::Flag::ArchSpecific;
      pkgCache::PkgIterator const OwnerPkg = Ver.ParentPkg();
      for (pkgCache::PkgIterator Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
      {
	 if (isImplicit && OwnerPkg == Pkg)
	    continue;
	 if (isArchSpecific == false && APT::Configuration::checkArchitecture(OwnerPkg.Arch()) == false)
	    continue;
	 if (Owner->NewProvides(Ver, Pkg, idxProvideVersion, Flags) == false)
	    return false;
      }
   }
   return true;
}
									/*}}}*/
bool pkgCacheListParser::SameVersion(unsigned short const Hash,		/*{{{*/
      pkgCache::VerIterator const &Ver)
{
   return Hash == Ver->Hash;
}
									/*}}}*/
// CacheGenerator::SelectReleaseFile - Select the current release file the indexes belong to	/*{{{*/
bool pkgCacheGenerator::SelectReleaseFile(const string &File,const string &Site,
				   unsigned long Flags)
{
   if (File.empty() && Site.empty())
   {
      CurrentRlsFile = NULL;
      return true;
   }

   // Get some space for the structure
   map_pointer_t const idxFile = AllocateInMap(sizeof(*CurrentRlsFile));
   if (unlikely(idxFile == 0))
      return false;
   CurrentRlsFile = Cache.RlsFileP + idxFile;

   // Fill it in
   map_stringitem_t const idxFileName = WriteStringInMap(File);
   map_stringitem_t const idxSite = StoreString(MIXED, Site);
   if (unlikely(idxFileName == 0 || idxSite == 0))
      return false;
   CurrentRlsFile->FileName = idxFileName;
   CurrentRlsFile->Site = idxSite;
   CurrentRlsFile->NextFile = Cache.HeaderP->RlsFileList;
   CurrentRlsFile->Flags = Flags;
   CurrentRlsFile->ID = Cache.HeaderP->ReleaseFileCount;
   RlsFileName = File;
   Cache.HeaderP->RlsFileList = CurrentRlsFile - Cache.RlsFileP;
   Cache.HeaderP->ReleaseFileCount++;

   return true;
}
									/*}}}*/
// CacheGenerator::SelectFile - Select the current file being parsed	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to select which file is to be associated with all newly
   added versions. The caller is responsible for setting the IMS fields. */
bool pkgCacheGenerator::SelectFile(std::string const &File,
				   pkgIndexFile const &Index,
				   std::string const &Architecture,
				   std::string const &Component,
				   unsigned long const Flags)
{
   // Get some space for the structure
   map_pointer_t const idxFile = AllocateInMap(sizeof(*CurrentFile));
   if (unlikely(idxFile == 0))
      return false;
   CurrentFile = Cache.PkgFileP + idxFile;

   // Fill it in
   map_stringitem_t const idxFileName = WriteStringInMap(File);
   if (unlikely(idxFileName == 0))
      return false;
   CurrentFile->FileName = idxFileName;
   CurrentFile->NextFile = Cache.HeaderP->FileList;
   CurrentFile->ID = Cache.HeaderP->PackageFileCount;
   map_stringitem_t const idxIndexType = StoreString(MIXED, Index.GetType()->Label);
   if (unlikely(idxIndexType == 0))
      return false;
   CurrentFile->IndexType = idxIndexType;
   if (Architecture.empty())
      CurrentFile->Architecture = 0;
   else
   {
      map_stringitem_t const arch = StoreString(pkgCacheGenerator::MIXED, Architecture);
      if (unlikely(arch == 0))
	 return false;
      CurrentFile->Architecture = arch;
   }
   map_stringitem_t const component = StoreString(pkgCacheGenerator::MIXED, Component);
   if (unlikely(component == 0))
      return false;
   CurrentFile->Component = component;
   CurrentFile->Flags = Flags;
   if (CurrentRlsFile != NULL)
      CurrentFile->Release = CurrentRlsFile - Cache.RlsFileP;
   else
      CurrentFile->Release = 0;
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
map_stringitem_t pkgCacheGenerator::StoreString(enum StringType const type, const char *S,
						 unsigned int Size)
{
   std::string const key(S, Size);

   std::map<std::string,map_stringitem_t> * strings;
   switch(type) {
      case MIXED: strings = &strMixed; break;
      case PKGNAME: strings = &strPkgNames; break;
      case VERSIONNUMBER: strings = &strVersions; break;
      case SECTION: strings = &strSections; break;
      default: _error->Fatal("Unknown enum type used for string storage of '%s'", key.c_str()); return 0;
   }

   std::map<std::string,map_stringitem_t>::const_iterator const item = strings->find(key);
   if (item != strings->end())
      return item->second;

   map_stringitem_t const idxString = WriteStringInMap(S,Size);
   strings->insert(std::make_pair(key, idxString));
   return idxString;
}
									/*}}}*/
// CheckValidity - Check that a cache is up-to-date			/*{{{*/
// ---------------------------------------------------------------------
/* This just verifies that each file in the list of index files exists,
   has matching attributes with the cache and the cache does not have
   any extra files. */
static bool CheckValidity(const string &CacheFile,
                          pkgSourceList &List,
                          FileIterator const Start,
                          FileIterator const End,
                          MMap **OutMap = 0)
{
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);
   // No file, certainly invalid
   if (CacheFile.empty() == true || FileExists(CacheFile) == false)
   {
      if (Debug == true)
	 std::clog << "CacheFile " << CacheFile << " doesn't exist" << std::endl;
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
	 std::clog << "Errors are pending or Map is empty() for " << CacheFile << std::endl;
      _error->Discard();
      return false;
   }

   SPtrArray<bool> RlsVisited = new bool[Cache.HeaderP->ReleaseFileCount];
   memset(RlsVisited,0,sizeof(*RlsVisited)*Cache.HeaderP->ReleaseFileCount);
   std::vector<pkgIndexFile *> Files;
   for (pkgSourceList::const_iterator i = List.begin(); i != List.end(); ++i)
   {
      if (Debug == true)
	 std::clog << "Checking RlsFile " << (*i)->Describe() << ": ";
      pkgCache::RlsFileIterator const RlsFile = (*i)->FindInCache(Cache, true);
      if (RlsFile.end() == true)
      {
	 if (Debug == true)
	    std::clog << "FindInCache returned end-Pointer" << std::endl;
	 return false;
      }

      RlsVisited[RlsFile->ID] = true;
      if (Debug == true)
	 std::clog << "with ID " << RlsFile->ID << " is valid" << std::endl;

      std::vector <pkgIndexFile *> const * const Indexes = (*i)->GetIndexFiles();
      std::copy_if(Indexes->begin(), Indexes->end(), std::back_inserter(Files),
	    [](pkgIndexFile const * const I) { return I->HasPackages(); });
   }
   for (unsigned I = 0; I != Cache.HeaderP->ReleaseFileCount; ++I)
      if (RlsVisited[I] == false)
      {
	 if (Debug == true)
	    std::clog << "RlsFile with ID" << I << " wasn't visited" << std::endl;
	 return false;
      }

   std::copy(Start, End, std::back_inserter(Files));

   /* Now we check every index file, see if it is in the cache,
      verify the IMS data and check that it is on the disk too.. */
   SPtrArray<bool> Visited = new bool[Cache.HeaderP->PackageFileCount];
   memset(Visited,0,sizeof(*Visited)*Cache.HeaderP->PackageFileCount);
   for (std::vector<pkgIndexFile *>::const_reverse_iterator PkgFile = Files.rbegin(); PkgFile != Files.rend(); ++PkgFile)
   {
      if (Debug == true)
	 std::clog << "Checking PkgFile " << (*PkgFile)->Describe() << ": ";
      if ((*PkgFile)->Exists() == false)
      {
         if (Debug == true)
	    std::clog << "file doesn't exist" << std::endl;
	 continue;
      }

      // FindInCache is also expected to do an IMS check.
      pkgCache::PkgFileIterator File = (*PkgFile)->FindInCache(Cache);
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
	    std::clog << "PkgFile with ID" << I << " wasn't visited" << std::endl;
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
static map_filesize_t ComputeSize(pkgSourceList const * const List, FileIterator Start,FileIterator End)
{
   map_filesize_t TotalSize = 0;
   if (List !=  NULL)
   {
      for (pkgSourceList::const_iterator i = List->begin(); i != List->end(); ++i)
      {
	 std::vector <pkgIndexFile *> *Indexes = (*i)->GetIndexFiles();
	 for (std::vector<pkgIndexFile *>::const_iterator j = Indexes->begin(); j != Indexes->end(); ++j)
	    if ((*j)->HasPackages() == true)
	       TotalSize += (*j)->Size();
      }
   }

   for (; Start < End; ++Start)
   {
      if ((*Start)->HasPackages() == false)
	 continue;
      TotalSize += (*Start)->Size();
   }
   return TotalSize;
}
									/*}}}*/
// BuildCache - Merge the list of index files into the cache		/*{{{*/
static bool BuildCache(pkgCacheGenerator &Gen,
		       OpProgress * const Progress,
		       map_filesize_t &CurrentSize,map_filesize_t TotalSize,
		       pkgSourceList const * const List,
		       FileIterator const Start, FileIterator const End)
{
   std::vector<pkgIndexFile *> Files;
   bool const HasFileDeps = Gen.HasFileDeps();
   bool mergeFailure = false;

   auto const indexFileMerge = [&](pkgIndexFile * const I) {
      if (HasFileDeps)
	 Files.push_back(I);

      if (I->HasPackages() == false || mergeFailure)
	 return;

      if (I->Exists() == false)
	 return;

      if (I->FindInCache(Gen.GetCache()).end() == false)
      {
	 _error->Warning("Duplicate sources.list entry %s",
	       I->Describe().c_str());
	 return;
      }

      map_filesize_t const Size = I->Size();
      if (Progress != NULL)
	 Progress->OverallProgress(CurrentSize, TotalSize, Size, _("Reading package lists"));
      CurrentSize += Size;

      if (I->Merge(Gen,Progress) == false)
	 mergeFailure = true;
   };

   if (List !=  NULL)
   {
      for (pkgSourceList::const_iterator i = List->begin(); i != List->end(); ++i)
      {
	 if ((*i)->FindInCache(Gen.GetCache(), false).end() == false)
	 {
	    _error->Warning("Duplicate sources.list entry %s",
		  (*i)->Describe().c_str());
	    continue;
	 }

	 if ((*i)->Merge(Gen, Progress) == false)
	    return false;

	 std::vector <pkgIndexFile *> *Indexes = (*i)->GetIndexFiles();
	 if (Indexes != NULL)
	    std::for_each(Indexes->begin(), Indexes->end(), indexFileMerge);
	 if (mergeFailure)
	    return false;
      }
   }

   if (Start != End)
   {
      Gen.SelectReleaseFile("", "");
      std::for_each(Start, End, indexFileMerge);
      if (mergeFailure)
	 return false;
   }

   if (HasFileDeps == true)
   {
      if (Progress != NULL)
	 Progress->Done();
      TotalSize = ComputeSize(List, Start, End);
      CurrentSize = 0;
      for (std::vector<pkgIndexFile *>::const_iterator I = Files.begin(); I != Files.end(); ++I)
      {
	 map_filesize_t Size = (*I)->Size();
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
// CacheGenerator::MakeStatusCache - Construct the status cache		/*{{{*/
// ---------------------------------------------------------------------
/* This makes sure that the status cache (the cache that has all 
   index files from the sources list and all local ones) is ready
   to be mmaped. If OutMap is not zero then a MMap object representing
   the cache will be stored there. This is pretty much mandetory if you
   are using AllowMem. AllowMem lets the function be run as non-root
   where it builds the cache 'fast' into a memory buffer. */
static DynamicMMap* CreateDynamicMMap(FileFd * const CacheF, unsigned long Flags)
{
   map_filesize_t const MapStart = _config->FindI("APT::Cache-Start", 24*1024*1024);
   map_filesize_t const MapGrow = _config->FindI("APT::Cache-Grow", 1*1024*1024);
   map_filesize_t const MapLimit = _config->FindI("APT::Cache-Limit", 0);
   Flags |= MMap::Moveable;
   if (_config->FindB("APT::Cache-Fallback", false) == true)
      Flags |= MMap::Fallback;
   if (CacheF != NULL)
      return new DynamicMMap(*CacheF, Flags, MapStart, MapGrow, MapLimit);
   else
      return new DynamicMMap(Flags, MapStart, MapGrow, MapLimit);
}
static bool writeBackMMapToFile(pkgCacheGenerator * const Gen, DynamicMMap * const Map,
      std::string const &FileName)
{
   FileFd SCacheF(FileName, FileFd::WriteAtomic);
   if (_error->PendingError() == true)
      return false;

   fchmod(SCacheF.Fd(),0644);

   // Write out the main data
   if (SCacheF.Write(Map->Data(),Map->Size()) == false)
      return _error->Error(_("IO Error saving source cache"));
   SCacheF.Sync();

   // Write out the proper header
   Gen->GetCache().HeaderP->Dirty = false;
   if (SCacheF.Seek(0) == false ||
	 SCacheF.Write(Map->Data(),sizeof(*Gen->GetCache().HeaderP)) == false)
      return _error->Error(_("IO Error saving source cache"));
   Gen->GetCache().HeaderP->Dirty = true;
   SCacheF.Sync();
   return true;
}
static bool loadBackMMapFromFile(std::unique_ptr<pkgCacheGenerator> &Gen,
      SPtr<DynamicMMap> &Map, OpProgress * const Progress, std::string const &FileName)
{
   Map = CreateDynamicMMap(NULL, 0);
   FileFd CacheF(FileName, FileFd::ReadOnly);
   map_pointer_t const alloc = Map->RawAllocate(CacheF.Size());
   if ((alloc == 0 && _error->PendingError())
	 || CacheF.Read((unsigned char *)Map->Data() + alloc,
	    CacheF.Size()) == false)
      return false;
   Gen.reset(new pkgCacheGenerator(Map.Get(),Progress));
   return true;
}
APT_DEPRECATED bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap, bool AllowMem)
   { return pkgCacheGenerator::MakeStatusCache(List, &Progress, OutMap, AllowMem); }
bool pkgCacheGenerator::MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap,bool AllowMem)
{
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);

   std::vector<pkgIndexFile *> Files;
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

   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));

   bool pkgcache_fine = false;
   bool srcpkgcache_fine = false;
   bool volatile_fine = List.GetVolatileFiles().empty();

   if (CheckValidity(CacheFile, List, Files.begin(), Files.end(), volatile_fine ? OutMap : NULL) == true)
   {
      if (Debug == true)
	 std::clog << "pkgcache.bin is valid - no need to build any cache" << std::endl;
      pkgcache_fine = true;
      srcpkgcache_fine = true;
   }
   if (pkgcache_fine == false)
   {
      if (CheckValidity(SrcCacheFile, List, Files.end(), Files.end()) == true)
      {
	 if (Debug == true)
	    std::clog << "srcpkgcache.bin is valid - it can be reused" << std::endl;
	 srcpkgcache_fine = true;
      }
   }

   if (volatile_fine == true && srcpkgcache_fine == true && pkgcache_fine == true)
   {
      if (Progress != NULL)
	 Progress->OverallProgress(1,1,1,_("Reading package lists"));
      return true;
   }

   bool Writeable = false;
   if (srcpkgcache_fine == false || pkgcache_fine == false)
   {
      if (CacheFile.empty() == false)
	 Writeable = access(flNotFile(CacheFile).c_str(),W_OK) == 0;
      else if (SrcCacheFile.empty() == false)
	 Writeable = access(flNotFile(SrcCacheFile).c_str(),W_OK) == 0;

      if (Debug == true)
	 std::clog << "Do we have write-access to the cache files? " << (Writeable ? "YES" : "NO") << std::endl;

      if (Writeable == false && AllowMem == false)
      {
	 if (CacheFile.empty() == false)
	    return _error->Error(_("Unable to write to %s"),flNotFile(CacheFile).c_str());
	 else if (SrcCacheFile.empty() == false)
	    return _error->Error(_("Unable to write to %s"),flNotFile(SrcCacheFile).c_str());
	 else
	    return _error->Error("Unable to create caches as file usage is disabled, but memory not allowed either!");
      }
   }

   // At this point we know we need to construct something, so get storage ready
   SPtr<DynamicMMap> Map = CreateDynamicMMap(NULL, 0);
   if (Debug == true)
      std::clog << "Open memory Map (not filebased)" << std::endl;

   std::unique_ptr<pkgCacheGenerator> Gen{nullptr};
   map_filesize_t CurrentSize = 0;
   std::vector<pkgIndexFile*> VolatileFiles = List.GetVolatileFiles();
   map_filesize_t TotalSize = ComputeSize(NULL, VolatileFiles.begin(), VolatileFiles.end());
   if (srcpkgcache_fine == true && pkgcache_fine == false)
   {
      if (Debug == true)
	 std::clog << "srcpkgcache.bin was valid - populate MMap with it" << std::endl;
      if (loadBackMMapFromFile(Gen, Map, Progress, SrcCacheFile) == false)
	 return false;
      srcpkgcache_fine = true;
      TotalSize += ComputeSize(NULL, Files.begin(), Files.end());
   }
   else if (srcpkgcache_fine == false)
   {
      if (Debug == true)
	 std::clog << "srcpkgcache.bin is NOT valid - rebuild" << std::endl;
      Gen.reset(new pkgCacheGenerator(Map.Get(),Progress));

      TotalSize += ComputeSize(&List, Files.begin(),Files.end());
      if (BuildCache(*Gen, Progress, CurrentSize, TotalSize, &List,
	       Files.end(),Files.end()) == false)
	 return false;

      if (Writeable == true && SrcCacheFile.empty() == false)
	 if (writeBackMMapToFile(Gen.get(), Map.Get(), SrcCacheFile) == false)
	    return false;
   }

   if (pkgcache_fine == false)
   {
      if (Debug == true)
	 std::clog << "Building status cache in pkgcache.bin now" << std::endl;
      if (BuildCache(*Gen, Progress, CurrentSize, TotalSize, NULL,
	       Files.begin(), Files.end()) == false)
	 return false;

      if (Writeable == true && CacheFile.empty() == false)
	 if (writeBackMMapToFile(Gen.get(), Map.Get(), CacheFile) == false)
	    return false;
   }

   if (Debug == true)
      std::clog << "Caches done. Now bring in the volatile files (if any)" << std::endl;

   if (volatile_fine == false)
   {
      if (Gen == nullptr)
      {
	 if (Debug == true)
	    std::clog << "Populate new MMap with cachefile contents" << std::endl;
	 if (loadBackMMapFromFile(Gen, Map, Progress, CacheFile) == false)
	    return false;
      }

      Files = List.GetVolatileFiles();
      if (BuildCache(*Gen, Progress, CurrentSize, TotalSize, NULL,
	       Files.begin(), Files.end()) == false)
	 return false;
   }

   if (OutMap != nullptr)
      *OutMap = Map.UnGuard();

   if (Debug == true)
      std::clog << "Everything is ready for shipping" << std::endl;
   return true;
}
									/*}}}*/
// CacheGenerator::MakeOnlyStatusCache - Build only a status files cache/*{{{*/
// ---------------------------------------------------------------------
/* */
APT_DEPRECATED bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap)
   { return pkgCacheGenerator::MakeOnlyStatusCache(&Progress, OutMap); }
bool pkgCacheGenerator::MakeOnlyStatusCache(OpProgress *Progress,DynamicMMap **OutMap)
{
   std::vector<pkgIndexFile *> Files;
   if (_system->AddStatusFiles(Files) == false)
      return false;

   SPtr<DynamicMMap> Map = CreateDynamicMMap(NULL, 0);
   map_filesize_t CurrentSize = 0;
   map_filesize_t TotalSize = 0;
   
   TotalSize = ComputeSize(NULL, Files.begin(), Files.end());
   
   // Build the status cache
   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));
   pkgCacheGenerator Gen(Map.Get(),Progress);
   if (_error->PendingError() == true)
      return false;
   if (BuildCache(Gen,Progress,CurrentSize,TotalSize, NULL,
		  Files.begin(), Files.end()) == false)
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
bool pkgCacheGenerator::FinishCache(OpProgress * /*Progress*/)
{
   return true;
}
									/*}}}*/

pkgCacheListParser::pkgCacheListParser() : Owner(NULL), OldDepLast(NULL), FoundFileDeps(false), d(NULL) {}
pkgCacheListParser::~pkgCacheListParser() {}

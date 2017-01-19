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
template<class T> using Dynamic = pkgCacheGenerator::Dynamic<T>;
typedef std::vector<pkgIndexFile *>::iterator FileIterator;
template <typename Iter> std::vector<Iter*> pkgCacheGenerator::Dynamic<Iter>::toReMap;

static bool IsDuplicateDescription(pkgCache &Cache, pkgCache::DescIterator Desc,
			    APT::StringView CurMd5, std::string const &CurLang);

using std::string;
using APT::StringView;

// CacheGenerator::pkgCacheGenerator - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* We set the dirty flag and make sure that is written to the disk */
pkgCacheGenerator::pkgCacheGenerator(DynamicMMap *pMap,OpProgress *Prog) :
		    Map(*pMap), Cache(pMap,false), Progress(Prog),
		     CurrentRlsFile(NULL), CurrentFile(NULL), d(NULL)
{
}
bool pkgCacheGenerator::Start()
{
   if (Map.Size() == 0)
   {
      // Setup the map interface..
      Cache.HeaderP = (pkgCache::Header *)Map.Data();
      _error->PushToStack();
      Map.RawAllocate(sizeof(pkgCache::Header));
      bool const newError = _error->PendingError();
      _error->MergeWithStack();
      if (newError)
	 return false;
      if (Map.Size() <= 0)
	 return false;

      Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));

      // Starting header
      *Cache.HeaderP = pkgCache::Header();

      // make room for the hashtables for packages and groups
      if (Map.RawAllocate(2 * (Cache.HeaderP->GetHashTableSize() * sizeof(map_pointer_t))) == 0)
	 return false;

      map_stringitem_t const idxVerSysName = WriteStringInMap(_system->VS->Label);
      if (unlikely(idxVerSysName == 0))
	 return false;
      map_stringitem_t const idxArchitecture = StoreString(MIXED, _config->Find("APT::Architecture"));
      if (unlikely(idxArchitecture == 0))
	 return false;
      map_stringitem_t idxArchitectures;

      std::vector<std::string> archs = APT::Configuration::getArchitectures();
      if (archs.size() > 1)
      {
	 std::vector<std::string>::const_iterator a = archs.begin();
	 std::string list = *a;
	 for (++a; a != archs.end(); ++a)
	    list.append(",").append(*a);
	 idxArchitectures = WriteStringInMap(list);
	 if (unlikely(idxArchitectures == 0))
	    return false;
      }
      else
	 idxArchitectures = idxArchitecture;

      Cache.HeaderP = (pkgCache::Header *)Map.Data();
      Cache.HeaderP->VerSysName = idxVerSysName;
      Cache.HeaderP->Architecture = idxArchitecture;
      Cache.HeaderP->SetArchitectures(idxArchitectures);

      // Calculate the hash for the empty map, so ReMap does not fail
      Cache.HeaderP->CacheFileSize = Cache.CacheHash();
      Cache.ReMap();
   }
   else
   {
      // Map directly from the existing file
      Cache.ReMap(); 
      Map.UsePools(*Cache.HeaderP->Pools,sizeof(Cache.HeaderP->Pools)/sizeof(Cache.HeaderP->Pools[0]));
      if (Cache.VS != _system->VS)
	 return _error->Error(_("Cache has an incompatible versioning system"));
   }

   Cache.HeaderP->Dirty = true;
   Map.Sync(0,sizeof(pkgCache::Header));
   return true;
}
									/*}}}*/
// CacheGenerator::~pkgCacheGenerator - Destructor 			/*{{{*/
// ---------------------------------------------------------------------
/* We sync the data then unset the dirty flag in two steps so as to
   advoid a problem during a crash */
pkgCacheGenerator::~pkgCacheGenerator()
{
   if (_error->PendingError() == true || Map.validData() == false)
      return;
   if (Map.Sync() == false)
      return;
   
   Cache.HeaderP->Dirty = false;
   Cache.HeaderP->CacheFileSize = Cache.CacheHash();

   if (_config->FindB("Debug::pkgCacheGen", false))
      std::clog << "Produced cache with hash " << Cache.HeaderP->CacheFileSize << std::endl;
   Map.Sync(0,sizeof(pkgCache::Header));
}
									/*}}}*/
void pkgCacheGenerator::ReMap(void const * const oldMap, void const * const newMap, size_t oldSize) {/*{{{*/
   // Prevent multiple remaps of the same iterator. If seen.insert(iterator)
   // returns (something, true) the iterator was not yet seen and we can
   // remap it.
   std::unordered_set<void *> seen;
   if (oldMap == newMap)
      return;

   if (_config->FindB("Debug::pkgCacheGen", false))
      std::clog << "Remaping from " << oldMap << " to " << newMap << std::endl;

   Cache.ReMap(false);

   CurrentFile += (pkgCache::PackageFile const * const) newMap - (pkgCache::PackageFile const * const) oldMap;
   CurrentRlsFile += (pkgCache::ReleaseFile const * const) newMap - (pkgCache::ReleaseFile const * const) oldMap;

   for (std::vector<pkgCache::GrpIterator*>::const_iterator i = Dynamic<pkgCache::GrpIterator>::toReMap.begin();
	i != Dynamic<pkgCache::GrpIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PkgIterator*>::const_iterator i = Dynamic<pkgCache::PkgIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PkgIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::VerIterator*>::const_iterator i = Dynamic<pkgCache::VerIterator>::toReMap.begin();
	i != Dynamic<pkgCache::VerIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::DepIterator*>::const_iterator i = Dynamic<pkgCache::DepIterator>::toReMap.begin();
	i != Dynamic<pkgCache::DepIterator>::toReMap.end(); ++i)
	if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::DescIterator*>::const_iterator i = Dynamic<pkgCache::DescIterator>::toReMap.begin();
	i != Dynamic<pkgCache::DescIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PrvIterator*>::const_iterator i = Dynamic<pkgCache::PrvIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PrvIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::PkgFileIterator*>::const_iterator i = Dynamic<pkgCache::PkgFileIterator>::toReMap.begin();
	i != Dynamic<pkgCache::PkgFileIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (std::vector<pkgCache::RlsFileIterator*>::const_iterator i = Dynamic<pkgCache::RlsFileIterator>::toReMap.begin();
	i != Dynamic<pkgCache::RlsFileIterator>::toReMap.end(); ++i)
      if (std::get<1>(seen.insert(*i)) == true)
	 (*i)->ReMap(oldMap, newMap);
   for (APT::StringView* ViewP : Dynamic<APT::StringView>::toReMap) {
      if (std::get<1>(seen.insert(ViewP)) == false)
	 continue;
      // Ignore views outside of the cache.
      if (ViewP->data() < static_cast<const char*>(oldMap)
	 || ViewP->data() > static_cast<const char*>(oldMap) + oldSize)
	 continue;
      const char *data = ViewP->data() + (static_cast<const char*>(newMap) - static_cast<const char*>(oldMap));
      *ViewP = StringView(data , ViewP->size());
   }
}									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_stringitem_t pkgCacheGenerator::WriteStringInMap(const char *String,
					const unsigned long &Len) {
   size_t oldSize = Map.Size();
   void const * const oldMap = Map.Data();
   map_stringitem_t const index = Map.WriteString(String, Len);
   if (index != 0)
      ReMap(oldMap, Map.Data(), oldSize);
   return index;
}
									/*}}}*/
// CacheGenerator::WriteStringInMap					/*{{{*/
map_stringitem_t pkgCacheGenerator::WriteStringInMap(const char *String) {
   size_t oldSize = Map.Size();
   void const * const oldMap = Map.Data();
   map_stringitem_t const index = Map.WriteString(String);
   if (index != 0)
      ReMap(oldMap, Map.Data(), oldSize);
   return index;
}
									/*}}}*/
map_pointer_t pkgCacheGenerator::AllocateInMap(const unsigned long &size) {/*{{{*/
   size_t oldSize = Map.Size();
   void const * const oldMap = Map.Data();
   map_pointer_t const index = Map.Allocate(size);
   if (index != 0)
      ReMap(oldMap, Map.Data(), oldSize);
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

      APT::StringView Arch = List.Architecture();
      Dynamic<APT::StringView> DynArch(Arch);
      APT::StringView Version = List.Version();
      Dynamic<APT::StringView> DynVersion(Version);
      if (Version.empty() == true && Arch.empty() == true)
      {
	 // package descriptions
	 if (MergeListGroup(List, PackageName) == false)
	    return false;
	 continue;
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
	 return true;
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
   StringView CurMd5 = List.Description_md5();
   std::vector<std::string> availDesc = List.AvailableDescriptionLanguages();
   for (Ver = Pkg.VersionList(); Ver.end() == false; ++Ver)
   {
      pkgCache::DescIterator VerDesc = Ver.DescriptionList();

      // a version can only have one md5 describing it
      if (VerDesc.end() == true || Cache.ViewString(VerDesc->md5sum) != CurMd5)
	 continue;

      map_stringitem_t md5idx = VerDesc->md5sum;
      for (std::vector<std::string>::const_iterator CurLang = availDesc.begin(); CurLang != availDesc.end(); ++CurLang)
      {
	 // don't add a new description if we have one for the given
	 // md5 && language
	 if (IsDuplicateDescription(Cache, VerDesc, CurMd5, *CurLang) == true)
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
					 APT::StringView const &Version, pkgCache::VerIterator* &OutVer)
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
	 char const * const VerStr = Ver.VerStr();
	 Res = Cache.VS->DoCmpVersion(Version.data(), Version.data() + Version.length(),
	       VerStr, VerStr + strlen(VerStr));
	 // Version is higher as current version - insert here
	 if (Res > 0)
	    break;
	 // Versionstrings are equal - is hash also equal?
	 if (Res == 0)
	 {
	    if (List.SameVersion(Hash, Ver) == true)
	       break;
	    // sort (volatile) sources above not-sources like the status file
	    if ((CurrentFile->Flags & pkgCache::Flag::NotSource) == 0)
	    {
	       auto VF = Ver.FileList();
	       for (; VF.end() == false; ++VF)
		  if (VF.File().Flagged(pkgCache::Flag::NotSource) == false)
		     break;
	       if (VF.end() == true)
		  break;
	    }
	 }
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
   if (unlikely(verindex == 0))
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
   StringView CurMd5 = List.Description_md5();

   /* Before we add a new description we first search in the group for
      a version with a description of the same MD5 - if so we reuse this
      description group instead of creating our own for this version */
   for (pkgCache::PkgIterator P = Grp.PackageList();
	P.end() == false; P = Grp.NextPkg(P))
   {
      for (pkgCache::VerIterator V = P.VersionList();
	   V.end() == false; ++V)
      {
	 if (V->DescriptionList == 0 || Cache.ViewString(V.DescriptionList()->md5sum) != CurMd5)
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
bool pkgCacheGenerator::AddNewDescription(ListParser &List, pkgCache::VerIterator &Ver, std::string const &lang, APT::StringView CurMd5, map_stringitem_t &md5idx) /*{{{*/
{
   pkgCache::DescIterator Desc;
   Dynamic<pkgCache::DescIterator> DynDesc(Desc);

   map_pointer_t const descindex = NewDescription(Desc, lang, CurMd5, md5idx);
   if (unlikely(descindex == 0))
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
// CacheGenerator::NewGroup - Add a new group				/*{{{*/
// ---------------------------------------------------------------------
/* This creates a new group structure and adds it to the hash table */
bool pkgCacheGenerator::NewGroup(pkgCache::GrpIterator &Grp, StringView Name)
{
   Dynamic<StringView> DName(Name);
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

   while (*insertAt != 0 && StringViewCompareFast(Name, Cache.ViewString((Cache.GrpP + *insertAt)->Name)) > 0)
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
bool pkgCacheGenerator::NewPackage(pkgCache::PkgIterator &Pkg, StringView Name,
					StringView Arch) {
   pkgCache::GrpIterator Grp;
   Dynamic<StringView> DName(Name);
   Dynamic<StringView> DArch(Arch);
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
      while (*insertAt != 0 && StringViewCompareFast(Name, Cache.ViewString((Cache.GrpP + (Cache.PkgP + *insertAt)->Group)->Name)) > 0)
	 insertAt = &(Cache.PkgP + *insertAt)->NextPackage;
      Pkg->NextPackage = *insertAt;
      *insertAt = Package;
   }
   else // Group the Packages together
   {
      // if sibling is provided by another package, this one is too
      {
	 pkgCache::PkgIterator const M = Grp.FindPreferredPkg(false); // native or any foreign pkg will do
	 if (M.end() == false) {
	    pkgCache::PrvIterator Prv;
	    Dynamic<pkgCache::PrvIterator> DynPrv(Prv);
	    for (Prv = M.ProvidesList(); Prv.end() == false; ++Prv)
	    {
	       if ((Prv->Flags & pkgCache::Flag::ArchSpecific) != 0)
		  continue;
	       pkgCache::VerIterator Ver = Prv.OwnerVer();
	       Dynamic<pkgCache::VerIterator> DynVer(Ver);
	       if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed ||
	           ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign &&
			(Prv->Flags & pkgCache::Flag::MultiArchImplicit) == 0))
	       {
		  if (APT::Configuration::checkArchitecture(Ver.ParentPkg().Arch()) == false)
		     continue;
		  if (NewProvides(Ver, Pkg, Prv->ProvideVersion, Prv->Flags) == false)
		     return false;
	       }
	    }
	 }
      }
      // let M-A:foreign package siblings provide this package
      {
	 pkgCache::PkgIterator P;
	 pkgCache::VerIterator Ver;
	 Dynamic<pkgCache::PkgIterator> DynP(P);
	 Dynamic<pkgCache::VerIterator> DynVer(Ver);

	 for (P = Grp.PackageList(); P.end() == false;  P = Grp.NextPkg(P))
	 {
	    if (APT::Configuration::checkArchitecture(P.Arch()) == false)
	       continue;
	    for (Ver = P.VersionList(); Ver.end() == false; ++Ver)
	       if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
		  if (NewProvides(Ver, Pkg, Ver->VerStr, pkgCache::Flag::MultiArchImplicit) == false)
		     return false;
	 }
      }
      // and negative dependencies, don't forget negative dependencies
      {
	 pkgCache::PkgIterator const M = Grp.FindPreferredPkg(false);
	 if (M.end() == false) {
	    pkgCache::DepIterator Dep;
	    Dynamic<pkgCache::DepIterator> DynDep(Dep);
	    for (Dep = M.RevDependsList(); Dep.end() == false; ++Dep)
	    {
	       if ((Dep->CompareOp & (pkgCache::Dep::ArchSpecific | pkgCache::Dep::MultiArchImplicit)) != 0)
		  continue;
	       if (Dep->Type != pkgCache::Dep::DpkgBreaks && Dep->Type != pkgCache::Dep::Conflicts &&
		     Dep->Type != pkgCache::Dep::Replaces)
		  continue;
	       pkgCache::VerIterator Ver = Dep.ParentVer();
	       Dynamic<pkgCache::VerIterator> DynVer(Ver);
	       map_pointer_t * unused = NULL;
	       if (NewDepends(Pkg, Ver, Dep->Version, Dep->CompareOp, Dep->Type, unused) == false)
		  return false;
	    }
	 }
      }

      // this package is the new last package
      pkgCache::PkgIterator LastPkg(Cache, Cache.PkgP + Grp->LastPackage);
      Pkg->NextPackage = LastPkg->NextPackage;
      LastPkg->NextPackage = Package;
   }
   Grp->LastPackage = Package;

   // lazy-create foo (of amd64) provides foo:amd64 at the time we first need it
   if (Arch == "any")
   {
      size_t const found = Name.rfind(':');
      StringView ArchA = Name.substr(found + 1);
      if (ArchA != "any")
      {
	 // ArchA is used inside the loop which might remap (NameA is not used)
	 Dynamic<StringView> DynArchA(ArchA);
	 StringView NameA = Name.substr(0, found);
	 pkgCache::PkgIterator PkgA = Cache.FindPkg(NameA, ArchA);
	 Dynamic<pkgCache::PkgIterator> DynPkgA(PkgA);
	 if (PkgA.end())
	 {
	    Dynamic<StringView> DynNameA(NameA);
	    if (NewPackage(PkgA, NameA, ArchA) == false)
	       return false;
	 }
	 if (unlikely(PkgA.end()))
	    return _error->Fatal("NewPackage was successful for %s:%s,"
		  "but the package doesn't exist anyhow!",
		  NameA.to_string().c_str(), ArchA.to_string().c_str());
	 else
	 {
	    pkgCache::PrvIterator Prv = PkgA.ProvidesList();
	    for (; Prv.end() == false; ++Prv)
	    {
	       if (Prv.IsMultiArchImplicit())
		  continue;
	       pkgCache::VerIterator V = Prv.OwnerVer();
	       if (ArchA != V.ParentPkg().Arch())
		  continue;
	       if (NewProvides(V, Pkg, V->VerStr, pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific) == false)
		  return false;
	    }
	    pkgCache::VerIterator V = PkgA.VersionList();
	    Dynamic<pkgCache::VerIterator> DynV(V);
	    for (; V.end() == false; ++V)
	    {
	       if (NewProvides(V, Pkg, V->VerStr, pkgCache::Flag::MultiArchImplicit | pkgCache::Flag::ArchSpecific) == false)
		  return false;
	    }
	 }
      }
   }
   return true;
}
									/*}}}*/
// CacheGenerator::AddImplicitDepends					/*{{{*/
bool pkgCacheGenerator::AddImplicitDepends(pkgCache::GrpIterator &G,
					   pkgCache::PkgIterator &P,
					   pkgCache::VerIterator &V)
{
   APT::StringView Arch = P.Arch() == NULL ? "" : P.Arch();
   Dynamic<APT::StringView> DynArch(Arch);
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
					    APT::StringView const &VerStr,
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
	    int const cmp = strncmp(V.VerStr(), VerStr.data(), VerStr.length());
	    if (cmp == 0 && V.VerStr()[VerStr.length()] == '\0')
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
					    APT::StringView md5sum,
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
      map_stringitem_t const idxmd5sum = WriteStringInMap(md5sum);
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
					       StringView PackageName,
					       StringView Arch,
					       StringView Version,
					       uint8_t const Op,
					       uint8_t const Type)
{
   pkgCache::GrpIterator Grp;
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);
   Dynamic<StringView> DynPackageName(PackageName);
   Dynamic<StringView> DynArch(Arch);
   Dynamic<StringView> DynVersion(Version);
   if (unlikely(Owner->NewGroup(Grp, PackageName) == false))
      return false;

   map_stringitem_t idxVersion = 0;
   if (Version.empty() == false)
   {
      int const CmpOp = Op & 0x0F;
      // =-deps are used (79:1) for lockstep on same-source packages (e.g. data-packages)
      if (CmpOp == pkgCache::Dep::Equals && Version == Ver.VerStr())
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
						StringView PkgName,
						StringView PkgArch,
						StringView Version,
						uint8_t const Flags)
{
   pkgCache const &Cache = Owner->Cache;
   Dynamic<StringView> DynPkgName(PkgName);
   Dynamic<StringView> DynArch(PkgArch);
   Dynamic<StringView> DynVersion(Version);

   // We do not add self referencing provides
   if (Ver.ParentPkg().Name() == PkgName && (PkgArch == Ver.ParentPkg().Arch() ||
	(PkgArch == "all" && strcmp((Cache.StrP + Cache.HeaderP->Architecture), Ver.ParentPkg().Arch()) == 0)) &&
	 (Version.empty() || Version == Ver.VerStr()))
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
bool pkgCacheListParser::NewProvidesAllArch(pkgCache::VerIterator &Ver, StringView Package,
				StringView Version, uint8_t const Flags) {
   pkgCache &Cache = Owner->Cache;
   pkgCache::GrpIterator Grp = Cache.FindGrp(Package);
   Dynamic<pkgCache::GrpIterator> DynGrp(Grp);
   Dynamic<StringView> DynPackage(Package);
   Dynamic<StringView> DynVersion(Version);

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
      pkgCache::PkgIterator OwnerPkg = Ver.ParentPkg();
      Dynamic<pkgCache::PkgIterator> DynOwnerPkg(OwnerPkg);
      pkgCache::PkgIterator Pkg;
      Dynamic<pkgCache::PkgIterator> DynPkg(Pkg);
      for (Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
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
   auto strings = &strMixed;
   switch(type) {
      case MIXED: strings = &strMixed; break;
      case PKGNAME: strings = &strPkgNames; break;
      case VERSIONNUMBER: strings = &strVersions; break;
      case SECTION: strings = &strSections; break;
      default: _error->Fatal("Unknown enum type used for string storage of '%.*s'", Size, S); return 0;
   }

   auto const item = strings->find({S, Size, nullptr, 0});
   if (item != strings->end())
      return item->item;

   map_stringitem_t const idxString = WriteStringInMap(S,Size);
   strings->insert({nullptr, Size, this, idxString});
   return idxString;
}
									/*}}}*/
// CheckValidity - Check that a cache is up-to-date			/*{{{*/
// ---------------------------------------------------------------------
/* This just verifies that each file in the list of index files exists,
   has matching attributes with the cache and the cache does not have
   any extra files. */
class APT_HIDDEN ScopedErrorRevert {
public:
   ScopedErrorRevert() { _error->PushToStack(); }
   ~ScopedErrorRevert() { _error->RevertToStack(); }
};
static bool CheckValidity(FileFd &CacheFile, std::string const &CacheFileName,
                          pkgSourceList &List,
                          FileIterator const Start,
                          FileIterator const End,
                          MMap **OutMap = 0,
			  pkgCache **OutCache = 0)
{
   if (CacheFileName.empty())
      return false;
   ScopedErrorRevert ser;

   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);
   // No file, certainly invalid
   if (CacheFile.Open(CacheFileName, FileFd::ReadOnly, FileFd::None) == false)
   {
      if (Debug == true)
	 std::clog << "CacheFile " << CacheFileName << " doesn't exist" << std::endl;
      return false;
   }

   if (List.GetLastModifiedTime() > CacheFile.ModificationTime())
   {
      if (Debug == true)
	 std::clog << "sources.list is newer than the cache" << std::endl;
      return false;
   }

   // Map it
   std::unique_ptr<MMap> Map(new MMap(CacheFile,0));
   if (unlikely(Map->validData()) == false)
      return false;
   std::unique_ptr<pkgCache> CacheP(new pkgCache(Map.get()));
   pkgCache &Cache = *CacheP.get();
   if (_error->PendingError() || Map->Size() == 0)
   {
      if (Debug == true)
	 std::clog << "Errors are pending or Map is empty() for " << CacheFileName << std::endl;
      return false;
   }

   std::unique_ptr<bool[]> RlsVisited(new bool[Cache.HeaderP->ReleaseFileCount]);
   memset(RlsVisited.get(),0,sizeof(RlsVisited[0])*Cache.HeaderP->ReleaseFileCount);
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
   std::unique_ptr<bool[]> Visited(new bool[Cache.HeaderP->PackageFileCount]);
   memset(Visited.get(),0,sizeof(Visited[0])*Cache.HeaderP->PackageFileCount);
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
	 _error->DumpErrors(std::clog, GlobalError::DEBUG, false);
      }
      return false;
   }

   if (OutMap != 0)
      *OutMap = Map.release();
   if (OutCache != 0)
      *OutCache = CacheP.release();
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
   bool mergeFailure = false;

   auto const indexFileMerge = [&](pkgIndexFile * const I) {
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
   if (SCacheF.IsOpen() == false || SCacheF.Failed())
      return false;

   fchmod(SCacheF.Fd(),0644);

   // Write out the main data
   if (SCacheF.Write(Map->Data(),Map->Size()) == false)
      return _error->Error(_("IO Error saving source cache"));

   // Write out the proper header
   Gen->GetCache().HeaderP->Dirty = false;
   Gen->GetCache().HeaderP->CacheFileSize = Gen->GetCache().CacheHash();
   if (SCacheF.Seek(0) == false ||
	 SCacheF.Write(Map->Data(),sizeof(*Gen->GetCache().HeaderP)) == false)
      return _error->Error(_("IO Error saving source cache"));
   Gen->GetCache().HeaderP->Dirty = true;
   return true;
}
static bool loadBackMMapFromFile(std::unique_ptr<pkgCacheGenerator> &Gen,
      std::unique_ptr<DynamicMMap> &Map, OpProgress * const Progress, FileFd &CacheF)
{
   Map.reset(CreateDynamicMMap(NULL, 0));
   if (unlikely(Map->validData()) == false)
      return false;
   if (CacheF.IsOpen() == false || CacheF.Seek(0) == false || CacheF.Failed())
      return false;
   _error->PushToStack();
   map_pointer_t const alloc = Map->RawAllocate(CacheF.Size());
   bool const newError = _error->PendingError();
   _error->MergeWithStack();
   if (alloc == 0 && newError)
      return false;
   if (CacheF.Read((unsigned char *)Map->Data() + alloc, CacheF.Size()) == false)
      return false;
   Gen.reset(new pkgCacheGenerator(Map.get(),Progress));
   return Gen->Start();
}
bool pkgMakeStatusCache(pkgSourceList &List,OpProgress &Progress,
			MMap **OutMap, bool AllowMem)
   { return pkgCacheGenerator::MakeStatusCache(List, &Progress, OutMap, AllowMem); }
bool pkgCacheGenerator::MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap,bool)
{
   return pkgCacheGenerator::MakeStatusCache(List, Progress, OutMap, nullptr, true);
}
bool pkgCacheGenerator::MakeStatusCache(pkgSourceList &List,OpProgress *Progress,
			MMap **OutMap,pkgCache **OutCache, bool)
{
   // FIXME: deprecate the ignored AllowMem parameter
   bool const Debug = _config->FindB("Debug::pkgCacheGen", false);

   std::vector<pkgIndexFile *> Files;
   if (_system->AddStatusFiles(Files) == false)
      return false;

   // Decide if we can write to the files..
   string const CacheFileName = _config->FindFile("Dir::Cache::pkgcache");
   string const SrcCacheFileName = _config->FindFile("Dir::Cache::srcpkgcache");

   // ensure the cache directory exists
   if (CacheFileName.empty() == false || SrcCacheFileName.empty() == false)
   {
      string dir = _config->FindDir("Dir::Cache");
      size_t const len = dir.size();
      if (len > 5 && dir.find("/apt/", len - 6, 5) == len - 5)
	 dir = dir.substr(0, len - 5);
      if (CacheFileName.empty() == false)
	 CreateDirectory(dir, flNotFile(CacheFileName));
      if (SrcCacheFileName.empty() == false)
	 CreateDirectory(dir, flNotFile(SrcCacheFileName));
   }

   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));

   bool pkgcache_fine = false;
   bool srcpkgcache_fine = false;
   bool volatile_fine = List.GetVolatileFiles().empty();
   FileFd CacheFile;
   if (CheckValidity(CacheFile, CacheFileName, List, Files.begin(), Files.end(), volatile_fine ? OutMap : NULL,
		     volatile_fine ? OutCache : NULL) == true)
   {
      if (Debug == true)
	 std::clog << "pkgcache.bin is valid - no need to build any cache" << std::endl;
      pkgcache_fine = true;
      srcpkgcache_fine = true;
   }

   FileFd SrcCacheFile;
   if (pkgcache_fine == false)
   {
      if (CheckValidity(SrcCacheFile, SrcCacheFileName, List, Files.end(), Files.end()) == true)
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
      if (CacheFileName.empty() == false)
	 Writeable = access(flNotFile(CacheFileName).c_str(),W_OK) == 0;
      else if (SrcCacheFileName.empty() == false)
	 Writeable = access(flNotFile(SrcCacheFileName).c_str(),W_OK) == 0;

      if (Debug == true)
	 std::clog << "Do we have write-access to the cache files? " << (Writeable ? "YES" : "NO") << std::endl;
   }

   // At this point we know we need to construct something, so get storage ready
   std::unique_ptr<DynamicMMap> Map(CreateDynamicMMap(NULL, 0));
   if (unlikely(Map->validData()) == false)
      return false;
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
      Gen.reset(new pkgCacheGenerator(Map.get(),Progress));
      if (Gen->Start() == false)
	 return false;

      TotalSize += ComputeSize(&List, Files.begin(),Files.end());
      if (BuildCache(*Gen, Progress, CurrentSize, TotalSize, &List,
	       Files.end(),Files.end()) == false)
	 return false;

      if (Writeable == true && SrcCacheFileName.empty() == false)
	 if (writeBackMMapToFile(Gen.get(), Map.get(), SrcCacheFileName) == false)
	    return false;
   }

   if (pkgcache_fine == false)
   {
      if (Debug == true)
	 std::clog << "Building status cache in pkgcache.bin now" << std::endl;
      if (BuildCache(*Gen, Progress, CurrentSize, TotalSize, NULL,
	       Files.begin(), Files.end()) == false)
	 return false;

      if (Writeable == true && CacheFileName.empty() == false)
	 if (writeBackMMapToFile(Gen.get(), Map.get(), CacheFileName) == false)
	    return false;
   }

   if (Debug == true)
      std::clog << "Caches done. " << (volatile_fine ? "No volatile files, so we are done here." : "Now bring in the volatile files") << std::endl;

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
      *OutMap = Map.release();

   if (Debug == true)
      std::clog << "Everything is ready for shipping" << std::endl;
   return true;
}
									/*}}}*/
// CacheGenerator::MakeOnlyStatusCache - Build only a status files cache/*{{{*/
class APT_HIDDEN ScopedErrorMerge {
public:
   ScopedErrorMerge() { _error->PushToStack(); }
   ~ScopedErrorMerge() { _error->MergeWithStack(); }
};
bool pkgMakeOnlyStatusCache(OpProgress &Progress,DynamicMMap **OutMap)
   { return pkgCacheGenerator::MakeOnlyStatusCache(&Progress, OutMap); }
bool pkgCacheGenerator::MakeOnlyStatusCache(OpProgress *Progress,DynamicMMap **OutMap)
{
   std::vector<pkgIndexFile *> Files;
   if (_system->AddStatusFiles(Files) == false)
      return false;

   ScopedErrorMerge sem;
   std::unique_ptr<DynamicMMap> Map(CreateDynamicMMap(NULL, 0));
   if (unlikely(Map->validData()) == false)
      return false;
   map_filesize_t CurrentSize = 0;
   map_filesize_t TotalSize = 0;
   TotalSize = ComputeSize(NULL, Files.begin(), Files.end());

   // Build the status cache
   if (Progress != NULL)
      Progress->OverallProgress(0,1,1,_("Reading package lists"));
   pkgCacheGenerator Gen(Map.get(),Progress);
   if (Gen.Start() == false || _error->PendingError() == true)
      return false;
   if (BuildCache(Gen,Progress,CurrentSize,TotalSize, NULL,
		  Files.begin(), Files.end()) == false)
      return false;

   if (_error->PendingError() == true)
      return false;
   *OutMap = Map.release();
   
   return true;
}
									/*}}}*/
// IsDuplicateDescription						/*{{{*/
static bool IsDuplicateDescription(pkgCache &Cache, pkgCache::DescIterator Desc,
			    APT::StringView CurMd5, std::string const &CurLang)
{
   // Descriptions in the same link-list have all the same md5
   if (Desc.end() == true || Cache.ViewString(Desc->md5sum) != CurMd5)
      return false;
   for (; Desc.end() == false; ++Desc)
      if (Desc.LanguageCode() == CurLang)
	 return true;
   return false;
}
									/*}}}*/

pkgCacheListParser::pkgCacheListParser() : Owner(NULL), OldDepLast(NULL), d(NULL) {}
pkgCacheListParser::~pkgCacheListParser() {}

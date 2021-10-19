// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Package Cache - Accessor code for the cache
   
   Please see doc/apt-pkg/cache.sgml for a more detailed description of 
   this format. Also be sure to keep that file up-to-date!!
   
   This is the general utility functions for cache management. They provide
   a complete set of accessor functions for the cache. The cacheiterators
   header contains the STL-like iterators that can be used to easially
   navigate the cache as well as seamlessly dereference the mmap'd
   indexes. Use these always.
   
   The main class provides for ways to get package indexes and some
   general lookup functions to start the iterators.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <xxhash.h>

#include <apti18n.h>
									/*}}}*/

using std::string;
using APT::StringView;


// Cache::Header::Header - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Simply initialize the header */
pkgCache::Header::Header()
{
#define APT_HEADER_SET(X,Y) X = Y; static_assert(std::numeric_limits<decltype(X)>::max() > Y, "Size violation detected in pkgCache::Header")
   APT_HEADER_SET(Signature, 0x98FE76DC);

   /* Whenever the structures change the major version should be bumped,
      whenever the generator changes the minor version should be bumped. */
   APT_HEADER_SET(MajorVersion, 16);
   APT_HEADER_SET(MinorVersion, 0);
   APT_HEADER_SET(Dirty, false);

   APT_HEADER_SET(HeaderSz, sizeof(pkgCache::Header));
   APT_HEADER_SET(GroupSz, sizeof(pkgCache::Group));
   APT_HEADER_SET(PackageSz, sizeof(pkgCache::Package));
   APT_HEADER_SET(ReleaseFileSz, sizeof(pkgCache::ReleaseFile));
   APT_HEADER_SET(PackageFileSz, sizeof(pkgCache::PackageFile));
   APT_HEADER_SET(VersionSz, sizeof(pkgCache::Version));
   APT_HEADER_SET(DescriptionSz, sizeof(pkgCache::Description));
   APT_HEADER_SET(DependencySz, sizeof(pkgCache::Dependency));
   APT_HEADER_SET(DependencyDataSz, sizeof(pkgCache::DependencyData));
   APT_HEADER_SET(ProvidesSz, sizeof(pkgCache::Provides));
   APT_HEADER_SET(VerFileSz, sizeof(pkgCache::VerFile));
   APT_HEADER_SET(DescFileSz, sizeof(pkgCache::DescFile));
#undef APT_HEADER_SET

   GroupCount = 0;
   PackageCount = 0;
   VersionCount = 0;
   DescriptionCount = 0;
   DependsCount = 0;
   DependsDataCount = 0;
   ReleaseFileCount = 0;
   PackageFileCount = 0;
   VerFileCount = 0;
   DescFileCount = 0;
   ProvidesCount = 0;
   MaxVerFileSize = 0;
   MaxDescFileSize = 0;

   FileList = 0;
   RlsFileList = 0;
   VerSysName = 0;
   Architecture = 0;
   SetArchitectures(0);
   SetHashTableSize(_config->FindI("APT::Cache-HashTableSize", 196613));
   memset(Pools,0,sizeof(Pools));

   CacheFileSize = 0;
}
									/*}}}*/
// Cache::Header::CheckSizes - Check if the two headers have same *sz	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCache::Header::CheckSizes(Header &Against) const
{
   if (HeaderSz == Against.HeaderSz &&
       GroupSz == Against.GroupSz &&
       PackageSz == Against.PackageSz &&
       ReleaseFileSz == Against.ReleaseFileSz &&
       PackageFileSz == Against.PackageFileSz &&
       VersionSz == Against.VersionSz &&
       DescriptionSz == Against.DescriptionSz &&
       DependencySz == Against.DependencySz &&
       DependencyDataSz == Against.DependencyDataSz &&
       VerFileSz == Against.VerFileSz &&
       DescFileSz == Against.DescFileSz &&
       ProvidesSz == Against.ProvidesSz)
      return true;
   return false;
}
									/*}}}*/

// Cache::pkgCache - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::pkgCache(MMap *Map, bool DoMap) : Map(*Map), VS(nullptr), d(NULL)
{
   // call getArchitectures() with cached=false to ensure that the 
   // architectures cache is re-evaluated. this is needed in cases
   // when the APT::Architecture field changes between two cache creations
   APT::Configuration::getArchitectures(false);
   MultiArchEnabled = true;
   if (DoMap == true)
      ReMap();
}
									/*}}}*/
// Cache::ReMap - Reopen the cache file					/*{{{*/
// ---------------------------------------------------------------------
/* If the file is already closed then this will open it open it. */
bool pkgCache::ReMap(bool const &Errorchecks)
{
   // Apply the typecasts.
   HeaderP = (Header *)Map.Data();
   GrpP = (Group *)Map.Data();
   PkgP = (Package *)Map.Data();
   VerFileP = (VerFile *)Map.Data();
   DescFileP = (DescFile *)Map.Data();
   RlsFileP = (ReleaseFile *)Map.Data();
   PkgFileP = (PackageFile *)Map.Data();
   VerP = (Version *)Map.Data();
   DescP = (Description *)Map.Data();
   ProvideP = (Provides *)Map.Data();
   DepP = (Dependency *)Map.Data();
   DepDataP = (DependencyData *)Map.Data();
   StrP = (char *)Map.Data();

   if (Errorchecks == false)
      return true;

   if (Map.Size() == 0 || HeaderP == 0)
      return _error->Error(_("Empty package cache"));
   
   // Check the header
   Header DefHeader;
   if (HeaderP->Signature != DefHeader.Signature ||
       HeaderP->Dirty == true)
      return _error->Error(_("The package cache file is corrupted"));
   
   if (HeaderP->MajorVersion != DefHeader.MajorVersion ||
       HeaderP->MinorVersion != DefHeader.MinorVersion ||
       HeaderP->CheckSizes(DefHeader) == false)
      return _error->Error(_("The package cache file is an incompatible version"));

   if (HeaderP->VerSysName == 0 || HeaderP->Architecture == 0 || HeaderP->GetArchitectures() == 0)
      return _error->Error(_("The package cache file is corrupted"));

   // Locate our VS..
   if ((VS = pkgVersioningSystem::GetVS(StrP + HeaderP->VerSysName)) == 0)
      return _error->Error(_("This APT does not support the versioning system '%s'"),StrP + HeaderP->VerSysName);

   // Check the architecture
   std::vector<std::string> archs = APT::Configuration::getArchitectures();
   std::string list = "";
   for (auto const & arch : archs) {
      if (!list.empty())
         list.append(",");
      list.append(arch);
   }
   if (_config->Find("APT::Architecture") != StrP + HeaderP->Architecture ||
	 list != StrP + HeaderP->GetArchitectures())
      return _error->Error(_("The package cache was built for different architectures: %s vs %s"), StrP + HeaderP->GetArchitectures(), list.c_str());


   auto hash = CacheHash();
   if (_config->FindB("Debug::pkgCacheGen", false))
      std::clog << "Opened cache with hash " << hash << ", expecting " <<  HeaderP->CacheFileSize << "\n";
   if (hash != HeaderP->CacheFileSize)
      return _error->Error(_("The package cache file is corrupted, it has the wrong hash"));

   return true;
}
									/*}}}*/
// Cache::Hash - Hash a string						/*{{{*/
// ---------------------------------------------------------------------
/* This is used to generate the hash entries for the HashTable. With my
   package list from bo this function gets 94% table usage on a 512 item
   table (480 used items) */
map_id_t pkgCache::sHash(StringView Str) const
{
   uint32_t Hash = 5381;
   auto I = Str.begin();
   auto End = Str.end();
   for (; I + 7 < End; I += 8)
   {
      Hash = (33u * 33u * 33u * 33u * 33u * 33u * 33u * 33u * Hash +
	      33u * 33u * 33u * 33u * 33u * 33u * 33u * tolower_ascii_unsafe(I[0]) +
	      33u * 33u * 33u * 33u * 33u * 33u * tolower_ascii_unsafe(I[1]) +
	      33u * 33u * 33u * 33u * 33u * tolower_ascii_unsafe(I[2]) +
	      33u * 33u * 33u * 33u * tolower_ascii_unsafe(I[3]) +
	      33u * 33u * 33u * tolower_ascii_unsafe(I[4]) +
	      33u * 33u * tolower_ascii_unsafe(I[5]) +
	      33u * tolower_ascii_unsafe(I[6]) +
	      tolower_ascii_unsafe(I[7]));
   }
   for (; I != End; ++I)
      Hash = 33u * Hash + tolower_ascii_unsafe(*I);
   return Hash % HeaderP->GetHashTableSize();
}
uint32_t pkgCache::CacheHash()
{
   pkgCache::Header header = {};
   XXH3_state_t *state = XXH3_createState();

   if (Map.Size() < sizeof(header))
      return 0;

   XXH3_64bits_reset(state);
   memcpy(&header, GetMap().Data(), sizeof(header));

   header.Dirty = false;
   header.CacheFileSize = 0;

   XXH3_64bits_update(state,
		      reinterpret_cast<const unsigned char *>(PACKAGE_VERSION),
		      APT_ARRAY_SIZE(PACKAGE_VERSION));

   XXH3_64bits_update(state,
		      reinterpret_cast<const unsigned char *>(&header),
		      sizeof(header));

   if (Map.Size() > sizeof(header)) {
      XXH3_64bits_update(state,
			 static_cast<const unsigned char *>(GetMap().Data()) + sizeof(header),
			 GetMap().Size() - sizeof(header));
   }

   auto const digest = XXH3_64bits_digest(state);
   XXH3_freeState(state);
   return digest & 0xFFFFFFFF;
}
									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(StringView Name) {
	auto const found = Name.rfind(':');
	if (found == string::npos)
	   return FindPkg(Name, "native");
	auto const Arch = Name.substr(found+1);
	/* Beware: This is specialcased to handle pkg:any in dependencies
	   as these are linked to virtual pkg:any named packages.
	   If you want any arch from a pkg, use FindPkg(pkg,"any") */
	if (Arch == "any")
		return FindPkg(Name, "any");
	return FindPkg(Name.substr(0, found), Arch);
}
									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(StringView Name, StringView Arch) {
	/* We make a detour via the GrpIterator here as
	   on a multi-arch environment a group is easier to
	   find than a package (less entries in the buckets) */
	pkgCache::GrpIterator Grp = FindGrp(Name);
	if (Grp.end() == true)
		return PkgIterator(*this,0);

	return Grp.FindPkg(Arch);
}
									/*}}}*/
// Cache::FindGrp - Locate a group by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns End-Pointer on error, pointer to the group otherwise */
pkgCache::GrpIterator pkgCache::FindGrp(StringView Name) {
	if (unlikely(Name.empty() == true))
		return GrpIterator(*this,0);

	// Look at the hash bucket for the group
	Group *Grp = GrpP + HeaderP->GrpHashTableP()[sHash(Name)];
	for (; Grp != GrpP; Grp = GrpP + Grp->Next) {
		int const cmp = StringViewCompareFast(Name, ViewString(Grp->Name));
		if (cmp == 0)
			return GrpIterator(*this, Grp);
		else if (cmp < 0)
			break;
	}

	return GrpIterator(*this,0);
}
									/*}}}*/
// Cache::CompTypeDeb - Return a string describing the compare type	/*{{{*/
// ---------------------------------------------------------------------
/* This returns a string representation of the dependency compare 
   type in the weird debian style.. */
const char *pkgCache::CompTypeDeb(unsigned char Comp)
{
   const char * const Ops[] = {"","<=",">=","<<",">>","=","!="};
   if (unlikely((unsigned)(Comp & 0xF) >= sizeof(Ops)/sizeof(Ops[0])))
      return "";
   return Ops[Comp & 0xF];
}
									/*}}}*/
// Cache::CompType - Return a string describing the compare type	/*{{{*/
// ---------------------------------------------------------------------
/* This returns a string representation of the dependency compare
   type */
const char *pkgCache::CompType(unsigned char Comp)
{
   const char * const Ops[] = {"","<=",">=","<",">","=","!="};
   if (unlikely((unsigned)(Comp & 0xF) >= sizeof(Ops)/sizeof(Ops[0])))
      return "";
   return Ops[Comp & 0xF];
}
									/*}}}*/
// Cache::DepType - Return a string describing the dep type		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgCache::DepType(unsigned char Type)
{
   const char *Types[] = {"",_("Depends"),_("PreDepends"),_("Suggests"),
                          _("Recommends"),_("Conflicts"),_("Replaces"),
                          _("Obsoletes"),_("Breaks"), _("Enhances")};
   if (Type < sizeof(Types)/sizeof(*Types))
      return Types[Type];
   return "";
}
									/*}}}*/
// Cache::Priority - Convert a priority value to a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgCache::Priority(unsigned char Prio)
{
   const char *Mapping[] = {0,_("required"),_("important"),_("standard"),
                            _("optional"),_("extra")};
   if (Prio < APT_ARRAY_SIZE(Mapping))
      return Mapping[Prio];
   return 0;
}
									/*}}}*/
// GrpIterator::FindPkg - Locate a package by arch			/*{{{*/
// ---------------------------------------------------------------------
/* Returns an End-Pointer on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::GrpIterator::FindPkg(StringView Arch) const {
	if (unlikely(IsGood() == false || S->FirstPackage == 0))
		return PkgIterator(*Owner, 0);

	/* If we accept any package we simply return the "first"
	   package in this group */
	if (Arch == "any")
		return PkgIterator(*Owner, Owner->PkgP + S->FirstPackage);
	if (Arch == "native" || Arch == "all")
               Arch = Owner->NativeArch();

	// Iterate over the list to find the matching arch
	for (pkgCache::Package *Pkg = PackageList(); Pkg != Owner->PkgP;
	     Pkg = Owner->PkgP + Pkg->NextPackage) {
		if (Arch == Owner->ViewString(Pkg->Arch))
			return PkgIterator(*Owner, Pkg);
		if ((Owner->PkgP + S->LastPackage) == Pkg)
			break;
	}

	return PkgIterator(*Owner, 0);
}
									/*}}}*/
// GrpIterator::FindPreferredPkg - Locate the "best" package		/*{{{*/
// ---------------------------------------------------------------------
/* Returns an End-Pointer on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::GrpIterator::FindPreferredPkg(bool const &PreferNonVirtual) const {
	pkgCache::PkgIterator Pkg = FindPkg(StringView("native", 6));
	if (Pkg.end() == false && (PreferNonVirtual == false || Pkg->VersionList != 0))
		return Pkg;

	std::vector<std::string> const archs = APT::Configuration::getArchitectures();
	for (std::vector<std::string>::const_iterator a = archs.begin();
	     a != archs.end(); ++a) {
		Pkg = FindPkg(*a);
		if (Pkg.end() == false && (PreferNonVirtual == false || Pkg->VersionList != 0))
			return Pkg;
	}
	// packages without an architecture
	Pkg = FindPkg(StringView("none", 4));
	if (Pkg.end() == false && (PreferNonVirtual == false || Pkg->VersionList != 0))
		return Pkg;

	if (PreferNonVirtual == true)
		return FindPreferredPkg(false);
	return PkgIterator(*Owner, 0);
}
									/*}}}*/
// GrpIterator::NextPkg - Locate the next package in the group		/*{{{*/
// ---------------------------------------------------------------------
/* Returns an End-Pointer on error, pointer to the package otherwise.
   We can't simply ++ to the next as the next package of the last will
   be from a different group (with the same hash value) */
pkgCache::PkgIterator pkgCache::GrpIterator::NextPkg(pkgCache::PkgIterator const &LastPkg) const {
	if (unlikely(IsGood() == false || S->FirstPackage == 0 ||
	    LastPkg.end() == true))
		return PkgIterator(*Owner, 0);

	if (S->LastPackage == LastPkg.MapPointer())
		return PkgIterator(*Owner, 0);

	return PkgIterator(*Owner, Owner->PkgP + LastPkg->NextPackage);
}
									/*}}}*/
// GrpIterator::operator++ - Prefix incr				/*{{{*/
// ---------------------------------------------------------------------
/* This will advance to the next logical group in the hash table. */
pkgCache::GrpIterator& pkgCache::GrpIterator::operator++()
{
   // Follow the current links
   if (S != Owner->GrpP)
      S = Owner->GrpP + S->Next;

   // Follow the hash table
   while (S == Owner->GrpP && (HashIndex+1) < (signed)Owner->HeaderP->GetHashTableSize())
   {
      ++HashIndex;
      S = Owner->GrpP + Owner->HeaderP->GrpHashTableP()[HashIndex];
   }
   return *this;
}
									/*}}}*/
// PkgIterator::operator++ - Prefix incr				/*{{{*/
// ---------------------------------------------------------------------
/* This will advance to the next logical package in the hash table. */
pkgCache::PkgIterator& pkgCache::PkgIterator::operator++()
{
   // Follow the current links
   if (S != Owner->PkgP)
      S = Owner->PkgP + S->NextPackage;

   // Follow the hash table
   while (S == Owner->PkgP && (HashIndex+1) < (signed)Owner->HeaderP->GetHashTableSize())
   {
      ++HashIndex;
      S = Owner->PkgP + Owner->HeaderP->PkgHashTableP()[HashIndex];
   }
   return *this;
}
									/*}}}*/
pkgCache::DepIterator& pkgCache::DepIterator::operator++()		/*{{{*/
{
   if (S == Owner->DepP)
      return *this;
   S = Owner->DepP + (Type == DepVer ? S->NextDepends : S->NextRevDepends);
   if (S == Owner->DepP)
      S2 = Owner->DepDataP;
   else
      S2 = Owner->DepDataP + S->DependencyData;
   return *this;
}
									/*}}}*/
// PkgIterator::State - Check the State of the package			/*{{{*/
// ---------------------------------------------------------------------
/* By this we mean if it is either cleanly installed or cleanly removed. */
pkgCache::PkgIterator::OkState pkgCache::PkgIterator::State() const
{  
   if (S->InstState == pkgCache::State::ReInstReq ||
       S->InstState == pkgCache::State::HoldReInstReq)
      return NeedsUnpack;
   
   if (S->CurrentState == pkgCache::State::UnPacked ||
       S->CurrentState == pkgCache::State::HalfConfigured)
      // we leave triggers alone completely. dpkg deals with
      // them in a hard-to-predict manner and if they get 
      // resolved by dpkg before apt run dpkg --configure on 
      // the TriggersPending package dpkg returns a error
      //Pkg->CurrentState == pkgCache::State::TriggersAwaited
      //Pkg->CurrentState == pkgCache::State::TriggersPending)
      return NeedsConfigure;
   
   if (S->CurrentState == pkgCache::State::HalfInstalled ||
       S->InstState != pkgCache::State::Ok)
      return NeedsUnpack;
      
   return NeedsNothing;
}
									/*}}}*/
// PkgIterator::CurVersion - Returns the current version string		/*{{{*/
// ---------------------------------------------------------------------
/* Return string representing of the current version. */
const char *
pkgCache::PkgIterator::CurVersion() const
{
  VerIterator version = CurrentVer();
  if (version.IsGood())
    return CurrentVer().VerStr();
  return 0;
}
									/*}}}*/
// ostream operator to handle string representation of a package	/*{{{*/
// ---------------------------------------------------------------------
/* Output name < cur.rent.version -> candid.ate.version | new.est.version > (section)
   Note that the characters <|>() are all literal above. Versions will be omitted
   if they provide no new information (e.g. there is no newer version than candidate)
   If no version and/or section can be found "none" is used. */
std::ostream& 
operator<<(std::ostream& out, pkgCache::PkgIterator Pkg) 
{
   if (Pkg.end() == true)
      return out << "invalid package";

   string current = string(Pkg.CurVersion() == 0 ? "none" : Pkg.CurVersion());
   string newest = string(Pkg.VersionList().end() ? "none" : Pkg.VersionList().VerStr());

   out << Pkg.Name() << " [ " << Pkg.Arch() << " ] < " << current;
   if ( newest != "none")
      out << " | " << newest;
   if (Pkg->VersionList == 0)
      out << " > ( none )";
   else
      out << " > ( " << string(Pkg.VersionList().Section()==0?"unknown":Pkg.VersionList().Section()) << " )";
   return out;
}
									/*}}}*/
// PkgIterator::FullName - Returns Name and (maybe) Architecture	/*{{{*/
// ---------------------------------------------------------------------
/* Returns a name:arch string */
std::string pkgCache::PkgIterator::FullName(bool const &Pretty) const
{
   string fullname = Name();
   if (Pretty == false ||
       (strcmp(Arch(), "all") != 0 && strcmp(Arch(), "any") != 0 &&
	strcmp(Owner->NativeArch(), Arch()) != 0))
      return fullname.append(":").append(Arch());
   return fullname;
}
									/*}}}*/
// DepIterator::IsCritical - Returns true if the dep is important	/*{{{*/
// ---------------------------------------------------------------------
/* Currently critical deps are defined as depends, predepends and
   conflicts (including dpkg's Breaks fields). */
bool pkgCache::DepIterator::IsCritical() const
{
   if (IsNegative() == true ||
       S2->Type == pkgCache::Dep::Depends ||
       S2->Type == pkgCache::Dep::PreDepends)
      return true;
   return false;
}
									/*}}}*/
// DepIterator::IsNegative - Returns true if the dep is a negative one	/*{{{*/
// ---------------------------------------------------------------------
/* Some dependencies are positive like Depends and Recommends, others
   are negative like Conflicts which can and should be handled differently */
bool pkgCache::DepIterator::IsNegative() const
{
   return S2->Type == Dep::DpkgBreaks ||
	  S2->Type == Dep::Conflicts ||
	  S2->Type == Dep::Obsoletes;
}
									/*}}}*/
// DepIterator::SmartTargetPkg - Resolve dep target pointers w/provides	/*{{{*/
// ---------------------------------------------------------------------
/* This intellegently looks at dep target packages and tries to figure
   out which package should be used. This is needed to nicely handle
   provide mapping. If the target package has no other providing packages
   then it returned. Otherwise the providing list is looked at to
   see if there is one unique providing package if so it is returned.
   Otherwise true is returned and the target package is set. The return
   result indicates whether the node should be expandable 
 
   In Conjunction with the DepCache the value of Result may not be 
   super-good since the policy may have made it uninstallable. Using
   AllTargets is better in this case. */
bool pkgCache::DepIterator::SmartTargetPkg(PkgIterator &Result) const
{
   Result = TargetPkg();
   
   // No provides at all
   if (Result->ProvidesList == 0)
      return false;
   
   // There is the Base package and the providing ones which is at least 2
   if (Result->VersionList != 0)
      return true;
      
   /* We have to skip over indirect provisions of the package that
      owns the dependency. For instance, if libc5-dev depends on the
      virtual package libc-dev which is provided by libc5-dev and libc6-dev
      we must ignore libc5-dev when considering the provides list. */ 
   PrvIterator PStart = Result.ProvidesList();
   for (; PStart.end() != true && PStart.OwnerPkg() == ParentPkg(); ++PStart);

   // Nothing but indirect self provides
   if (PStart.end() == true)
      return false;
   
   // Check for single packages in the provides list
   PrvIterator P = PStart;
   for (; P.end() != true; ++P)
   {
      // Skip over self provides
      if (P.OwnerPkg() == ParentPkg())
	 continue;
      if (PStart.OwnerPkg() != P.OwnerPkg())
	 break;
   }

   Result = PStart.OwnerPkg();
   
   // Check for non dups
   if (P.end() != true)
      return true;
   
   return false;
}
									/*}}}*/
// DepIterator::AllTargets - Returns the set of all possible targets	/*{{{*/
// ---------------------------------------------------------------------
/* This is a more useful version of TargetPkg() that follows versioned
   provides. It includes every possible package-version that could satisfy
   the dependency. The last item in the list has a 0. The resulting pointer
   must be delete [] 'd */
pkgCache::Version **pkgCache::DepIterator::AllTargets() const
{
   Version **Res = 0;
   unsigned long Size =0;
   while (1)
   {
      Version **End = Res;
      PkgIterator DPkg = TargetPkg();

      // Walk along the actual package providing versions
      for (VerIterator I = DPkg.VersionList(); I.end() == false; ++I)
      {
	 if (IsIgnorable(I.ParentPkg()) == true)
	    continue;
	 if (IsSatisfied(I) == false)
	    continue;

	 Size++;
	 if (Res != 0)
	    *End++ = I;
      }
      
      // Follow all provides
      for (PrvIterator I = DPkg.ProvidesList(); I.end() == false; ++I)
      {
	 if (IsIgnorable(I) == true)
	    continue;
	 if (IsSatisfied(I) == false)
	    continue;

	 Size++;
	 if (Res != 0)
	    *End++ = I.OwnerVer();
      }
      
      // Do it again and write it into the array
      if (Res == 0)
      {
	 Res = new Version *[Size+1];
	 Size = 0;
      }
      else
      {
	 *End = 0;
	 break;
      }      
   }
   
   return Res;
}
									/*}}}*/
// DepIterator::GlobOr - Compute an OR group				/*{{{*/
// ---------------------------------------------------------------------
/* This Takes an iterator, iterates past the current dependency grouping
   and returns Start and End so that so End is the final element
   in the group, if End == Start then D is End++ and End is the
   dependency D was pointing to. Use in loops to iterate sensibly. */
void pkgCache::DepIterator::GlobOr(DepIterator &Start,DepIterator &End)
{
   // Compute a single dependency element (glob or)
   Start = *this;
   End = *this;
   for (bool LastOR = true; end() == false && LastOR == true;)
   {
      LastOR = (S2->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
      ++(*this);
      if (LastOR == true)
	 End = (*this);
   }
}
									/*}}}*/
// DepIterator::IsIgnorable - should this packag/providr be ignored?	/*{{{*/
// ---------------------------------------------------------------------
/* Deps like self-conflicts should be ignored as well as implicit conflicts
   on virtual packages. */
bool pkgCache::DepIterator::IsIgnorable(PkgIterator const &PT) const
{
   if (IsNegative() == false)
      return false;

   pkgCache::PkgIterator const PP = ParentPkg();
   if (PP->Group != PT->Group)
      return false;
   // self-conflict
   if (PP == PT)
      return true;
   pkgCache::VerIterator const PV = ParentVer();
   // ignore group-conflict on a M-A:same package - but not our implicit dependencies
   // so that we can have M-A:same packages conflicting with their own real name
   if ((PV->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
      return IsMultiArchImplicit() == false;

   return false;
}
bool pkgCache::DepIterator::IsIgnorable(PrvIterator const &Prv) const
{
   if (IsNegative() == false)
      return false;

   PkgIterator const Pkg = ParentPkg();
   /* Provides may never be applied against the same package (or group)
      if it is a conflicts. See the comment above. */
   if (Prv.OwnerPkg()->Group == Pkg->Group)
      return true;
   // Implicit group-conflicts should not be applied on providers of other groups
   if (IsMultiArchImplicit() && Prv.OwnerPkg()->Group != Pkg->Group)
      return true;

   return false;
}
									/*}}}*/
// DepIterator::IsSatisfied - check if a version satisfied the dependency /*{{{*/
bool pkgCache::DepIterator::IsSatisfied(VerIterator const &Ver) const
{
   return Owner->VS->CheckDep(Ver.VerStr(),S2->CompareOp,TargetVer());
}
bool pkgCache::DepIterator::IsSatisfied(PrvIterator const &Prv) const
{
   return Owner->VS->CheckDep(Prv.ProvideVersion(),S2->CompareOp,TargetVer());
}
									/*}}}*/
// DepIterator::IsImplicit - added by the cache generation		/*{{{*/
bool pkgCache::DepIterator::IsImplicit() const
{
   if (IsMultiArchImplicit() == true)
      return true;
   if (IsNegative() || S2->Type == pkgCache::Dep::Replaces)
   {
      if ((S2->CompareOp & pkgCache::Dep::ArchSpecific) != pkgCache::Dep::ArchSpecific &&
	    strcmp(ParentPkg().Arch(), TargetPkg().Arch()) != 0)
	 return true;
   }
   return false;
}
									/*}}}*/
// ostream operator to handle string representation of a dependency	/*{{{*/
// ---------------------------------------------------------------------
/* */
std::ostream& operator<<(std::ostream& out, pkgCache::DepIterator D)
{
   if (D.end() == true)
      return out << "invalid dependency";

   pkgCache::PkgIterator P = D.ParentPkg();
   pkgCache::PkgIterator T = D.TargetPkg();

   out << (P.end() ? "invalid pkg" : P.FullName(false)) << " " << D.DepType()
	<< " on ";
APT_IGNORE_DEPRECATED_PUSH
   if (T.end() == true)
      out << "invalid pkg";
   else
      out << T;
APT_IGNORE_DEPRECATED_POP

   if (D->Version != 0)
      out << " (" << D.CompType() << " " << D.TargetVer() << ")";

   return out;
}
									/*}}}*/
// VerIterator::CompareVer - Fast version compare for same pkgs		/*{{{*/
// ---------------------------------------------------------------------
/* This just looks over the version list to see if B is listed before A. In
   most cases this will return in under 4 checks, ver lists are short. */
int pkgCache::VerIterator::CompareVer(const VerIterator &B) const
{
   // Check if they are equal
   if (*this == B)
      return 0;
   if (end() == true)
      return -1;
   if (B.end() == true)
      return 1;
       
   /* Start at A and look for B. If B is found then A > B otherwise
      B was before A so A < B */
   VerIterator I = *this;
   for (;I.end() == false; ++I)
      if (I == B)
	 return 1;
   return -1;
}
									/*}}}*/
// VerIterator::Downloadable - Checks if the version is downloadable	/*{{{*/
// ---------------------------------------------------------------------
/* */
APT_PURE bool pkgCache::VerIterator::Downloadable() const
{
   VerFileIterator Files = FileList();
   for (; Files.end() == false; ++Files)
      if (Files.File().Flagged(pkgCache::Flag::NotSource) == false)
	 return true;
   return false;
}
									/*}}}*/
// VerIterator::Automatic - Check if this version is 'automatic'	/*{{{*/
// ---------------------------------------------------------------------
/* This checks to see if any of the versions files are not NotAutomatic. 
   True if this version is selectable for automatic installation. */
APT_PURE bool pkgCache::VerIterator::Automatic() const
{
   VerFileIterator Files = FileList();
   for (; Files.end() == false; ++Files)
      // Do not check ButAutomaticUpgrades here as it is kind of automatic…
      if (Files.File().Flagged(pkgCache::Flag::NotAutomatic) == false)
	 return true;
   return false;
}
									/*}}}*/
// VerIterator::NewestFile - Return the newest file version relation	/*{{{*/
// ---------------------------------------------------------------------
/* This looks at the version numbers associated with all of the sources
   this version is in and returns the highest.*/
pkgCache::VerFileIterator pkgCache::VerIterator::NewestFile() const
{
   VerFileIterator Files = FileList();
   VerFileIterator Highest = Files;
   for (; Files.end() == false; ++Files)
   {
      if (Owner->VS->CmpReleaseVer(Files.File().Version(),Highest.File().Version()) > 0)
	 Highest = Files;
   }
   
   return Highest;
}
									/*}}}*/
// VerIterator::RelStr - Release description string			/*{{{*/
// ---------------------------------------------------------------------
/* This describes the version from a release-centric manner. The output is a 
   list of Label:Version/Archive */
static std::string PkgFileIteratorToRelString(pkgCache::PkgFileIterator const &File)
{
   std::string Res;
   if (File.Label() != 0)
      Res = Res + File.Label() + ':';

   if (File.Archive() != 0)
   {
      if (File.Version() == 0)
	 Res += File.Archive();
      else
	 Res = Res + File.Version() + '/' +  File.Archive();
   }
   else
   {
      // No release file, print the host name that this came from
      if (File.Site() == 0 || File.Site()[0] == 0)
	 Res += "localhost";
      else
	 Res += File.Site();
   }
   return Res;
}
string pkgCache::VerIterator::RelStr() const
{
   std::vector<std::string> RelStrs;
   for (pkgCache::VerFileIterator I = this->FileList(); I.end() == false; ++I)
   {
      // Do not print 'not source' entries'
      pkgCache::PkgFileIterator const File = I.File();
      if (File.Flagged(pkgCache::Flag::NotSource))
	 continue;

      std::string const RS = PkgFileIteratorToRelString(File);
      if (std::find(RelStrs.begin(), RelStrs.end(), RS) != RelStrs.end())
	 continue;

      RelStrs.push_back(RS);
   }
   std::ostringstream os;
   if (likely(RelStrs.empty() == false))
   {
      std::copy(RelStrs.begin(), RelStrs.end()-1, std::ostream_iterator<std::string>(os, ", "));
      os << *RelStrs.rbegin();
   }
   if (S->ParentPkg != 0)
      os << " [" << Arch() << "]";
   return os.str();
}
									/*}}}*/
// VerIterator::MultiArchType - string representing MultiArch flag	/*{{{*/
const char * pkgCache::VerIterator::MultiArchType() const
{
   if ((S->MultiArch & pkgCache::Version::Same) == pkgCache::Version::Same)
      return "same";
   else if ((S->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
      return "foreign";
   else if ((S->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
      return "allowed";
   return "none";
}
									/*}}}*/
// RlsFileIterator::RelStr - Return the release string			/*{{{*/
string pkgCache::RlsFileIterator::RelStr()
{
   string Res;
   if (Version() != 0)
      Res = Res + (Res.empty() == true?"v=":",v=") + Version();
   if (Origin() != 0)
      Res = Res + (Res.empty() == true?"o=":",o=")  + Origin();
   if (Archive() != 0)
      Res = Res + (Res.empty() == true?"a=":",a=")  + Archive();
   if (Codename() != 0)
      Res = Res + (Res.empty() == true?"n=":",n=")  + Codename();
   if (Label() != 0)
      Res = Res + (Res.empty() == true?"l=":",l=")  + Label();
   return Res;
}
									/*}}}*/
string pkgCache::PkgFileIterator::RelStr()				/*{{{*/
{
   std::string Res;
   if (ReleaseFile() == 0)
   {
      if (Component() != 0)
	 Res = Res + (Res.empty() == true?"a=":",a=")  + Component();
   }
   else
   {
      Res = ReleaseFile().RelStr();
      if (Component() != 0)
	 Res = Res + (Res.empty() == true?"c=":",c=")  + Component();
   }
   if (Architecture() != 0)
      Res = Res + (Res.empty() == true?"b=":",b=")  + Architecture();
   return Res;
}
									/*}}}*/
// VerIterator::TranslatedDescriptionForLanguage - Return a DescIter for language/*{{{*/
// ---------------------------------------------------------------------
/* return a DescIter for the specified language
 */
pkgCache::DescIterator pkgCache::VerIterator::TranslatedDescriptionForLanguage(StringView lang) const
{
   for (pkgCache::DescIterator Desc = DescriptionList(); Desc.end() == false; ++Desc)
      if (lang == Desc.LanguageCode())
         return Desc;

   if (lang == "en")
      return TranslatedDescriptionForLanguage("");

   return DescIterator();
}

									/*}}}*/
// VerIterator::TranslatedDescription - Return the a DescIter for locale/*{{{*/
// ---------------------------------------------------------------------
/* return a DescIter for the current locale or the default if none is
 * found
 */
pkgCache::DescIterator pkgCache::VerIterator::TranslatedDescription() const
{
   std::vector<string> const lang = APT::Configuration::getLanguages();
   for (std::vector<string>::const_iterator l = lang.begin();
	l != lang.end(); ++l)
   {
      pkgCache::DescIterator Desc = TranslatedDescriptionForLanguage(*l);
      if (Desc.IsGood())
         return Desc;
   }

   pkgCache::DescIterator Desc = TranslatedDescriptionForLanguage("");
   if (Desc.IsGood())
      return Desc;

   return DescriptionList();
}

									/*}}}*/

pkgCache::~pkgCache() {}

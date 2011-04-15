// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcache.cc,v 1.37 2003/02/10 01:40:58 doogie Exp $
/* ######################################################################
   
   Package Cache - Accessor code for the cache
   
   Please see doc/apt-pkg/cache.sgml for a more detailed description of 
   this format. Also be sure to keep that file up-to-date!!
   
   This is the general utility functions for cache managment. They provide
   a complete set of accessor functions for the cache. The cacheiterators
   header contains the STL-like iterators that can be used to easially
   navigate the cache as well as seemlessly dereference the mmap'd 
   indexes. Use these always.
   
   The main class provides for ways to get package indexes and some
   general lookup functions to start the iterators.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/macros.h>

#include <apti18n.h>
    
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
									/*}}}*/

using std::string;


// Cache::Header::Header - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Simply initialize the header */
pkgCache::Header::Header()
{
   Signature = 0x98FE76DC;
   
   /* Whenever the structures change the major version should be bumped,
      whenever the generator changes the minor version should be bumped. */
   MajorVersion = 8;
   MinorVersion = 0;
   Dirty = false;
   
   HeaderSz = sizeof(pkgCache::Header);
   GroupSz = sizeof(pkgCache::Group);
   PackageSz = sizeof(pkgCache::Package);
   PackageFileSz = sizeof(pkgCache::PackageFile);
   VersionSz = sizeof(pkgCache::Version);
   DescriptionSz = sizeof(pkgCache::Description);
   DependencySz = sizeof(pkgCache::Dependency);
   ProvidesSz = sizeof(pkgCache::Provides);
   VerFileSz = sizeof(pkgCache::VerFile);
   DescFileSz = sizeof(pkgCache::DescFile);
   
   GroupCount = 0;
   PackageCount = 0;
   VersionCount = 0;
   DescriptionCount = 0;
   DependsCount = 0;
   PackageFileCount = 0;
   VerFileCount = 0;
   DescFileCount = 0;
   ProvidesCount = 0;
   MaxVerFileSize = 0;
   MaxDescFileSize = 0;
   
   FileList = 0;
   StringList = 0;
   VerSysName = 0;
   Architecture = 0;
   memset(PkgHashTable,0,sizeof(PkgHashTable));
   memset(GrpHashTable,0,sizeof(GrpHashTable));
   memset(Pools,0,sizeof(Pools));
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
       PackageFileSz == Against.PackageFileSz &&
       VersionSz == Against.VersionSz &&
       DescriptionSz == Against.DescriptionSz &&
       DependencySz == Against.DependencySz &&
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
pkgCache::pkgCache(MMap *Map, bool DoMap) : Map(*Map)
{
   // call getArchitectures() with cached=false to ensure that the 
   // architectures cache is re-evaulated. this is needed in cases
   // when the APT::Architecture field changes between two cache creations
   MultiArchEnabled = APT::Configuration::getArchitectures(false).size() > 1;
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
   PkgFileP = (PackageFile *)Map.Data();
   VerP = (Version *)Map.Data();
   DescP = (Description *)Map.Data();
   ProvideP = (Provides *)Map.Data();
   DepP = (Dependency *)Map.Data();
   StringItemP = (StringItem *)Map.Data();
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

   // Locate our VS..
   if (HeaderP->VerSysName == 0 ||
       (VS = pkgVersioningSystem::GetVS(StrP + HeaderP->VerSysName)) == 0)
      return _error->Error(_("This APT does not support the versioning system '%s'"),StrP + HeaderP->VerSysName);

   // Chcek the arhcitecture
   if (HeaderP->Architecture == 0 ||
       _config->Find("APT::Architecture") != StrP + HeaderP->Architecture)
      return _error->Error(_("The package cache was built for a different architecture"));
   return true;
}
									/*}}}*/
// Cache::Hash - Hash a string						/*{{{*/
// ---------------------------------------------------------------------
/* This is used to generate the hash entries for the HashTable. With my
   package list from bo this function gets 94% table usage on a 512 item
   table (480 used items) */
unsigned long pkgCache::sHash(const string &Str) const
{
   unsigned long Hash = 0;
   for (string::const_iterator I = Str.begin(); I != Str.end(); I++)
      Hash = 5*Hash + tolower_ascii(*I);
   return Hash % _count(HeaderP->PkgHashTable);
}

unsigned long pkgCache::sHash(const char *Str) const
{
   unsigned long Hash = 0;
   for (const char *I = Str; *I != 0; I++)
      Hash = 5*Hash + tolower_ascii(*I);
   return Hash % _count(HeaderP->PkgHashTable);
}

									/*}}}*/
// Cache::SingleArchFindPkg - Locate a package by name			/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise
   The multiArch enabled methods will fallback to this one as it is (a bit)
   faster for single arch environments and realworld is mostly singlearch… */
pkgCache::PkgIterator pkgCache::SingleArchFindPkg(const string &Name)
{
   // Look at the hash bucket
   Package *Pkg = PkgP + HeaderP->PkgHashTable[Hash(Name)];
   for (; Pkg != PkgP; Pkg = PkgP + Pkg->NextPackage)
   {
      if (Pkg->Name != 0 && StrP[Pkg->Name] == Name[0] &&
          stringcasecmp(Name,StrP + Pkg->Name) == 0)
         return PkgIterator(*this,Pkg);
   }
   return PkgIterator(*this,0);
}
									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(const string &Name) {
	if (MultiArchCache() == false)
		return SingleArchFindPkg(Name);
	size_t const found = Name.find(':');
	if (found == string::npos)
		return FindPkg(Name, "native");
	string const Arch = Name.substr(found+1);
	/* Beware: This is specialcased to handle pkg:any in dependencies as
	   these are linked to virtual pkg:any named packages with all archs.
	   If you want any arch from a given pkg, use FindPkg(pkg,arch) */
	if (Arch == "any")
		return FindPkg(Name, "any");
	return FindPkg(Name.substr(0, found), Arch);
}
									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(const string &Name, string const &Arch) {
	if (MultiArchCache() == false) {
		if (Arch == "native" || Arch == "all" || Arch == "any" ||
		    Arch == NativeArch())
			return SingleArchFindPkg(Name);
		else
			return PkgIterator(*this,0);
	}
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
pkgCache::GrpIterator pkgCache::FindGrp(const string &Name) {
	if (unlikely(Name.empty() == true))
		return GrpIterator(*this,0);

	// Look at the hash bucket for the group
	Group *Grp = GrpP + HeaderP->GrpHashTable[sHash(Name)];
	for (; Grp != GrpP; Grp = GrpP + Grp->Next) {
		if (Grp->Name != 0 && StrP[Grp->Name] == Name[0] &&
		    stringcasecmp(Name, StrP + Grp->Name) == 0)
			return GrpIterator(*this, Grp);
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
   const char *Ops[] = {"","<=",">=","<<",">>","=","!="};
   if ((unsigned)(Comp & 0xF) < 7)
      return Ops[Comp & 0xF];
   return "";	 
}
									/*}}}*/
// Cache::CompType - Return a string describing the compare type	/*{{{*/
// ---------------------------------------------------------------------
/* This returns a string representation of the dependency compare 
   type */
const char *pkgCache::CompType(unsigned char Comp)
{
   const char *Ops[] = {"","<=",">=","<",">","=","!="};
   if ((unsigned)(Comp & 0xF) < 7)
      return Ops[Comp & 0xF];
   return "";	 
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
   const char *Mapping[] = {0,_("important"),_("required"),_("standard"),
                            _("optional"),_("extra")};
   if (Prio < _count(Mapping))
      return Mapping[Prio];
   return 0;
}
									/*}}}*/
// GrpIterator::FindPkg - Locate a package by arch			/*{{{*/
// ---------------------------------------------------------------------
/* Returns an End-Pointer on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::GrpIterator::FindPkg(string Arch) const {
	if (unlikely(IsGood() == false || S->FirstPackage == 0))
		return PkgIterator(*Owner, 0);

	/* If we accept any package we simply return the "first"
	   package in this group (the last one added). */
	if (Arch == "any")
		return PkgIterator(*Owner, Owner->PkgP + S->FirstPackage);

	char const* const myArch = Owner->NativeArch();
	/* Most of the time the package for our native architecture is
	   the one we add at first to the cache, but this would be the
	   last one we check, so we do it now. */
	if (Arch == "native" || Arch == myArch || Arch == "all") {
		pkgCache::Package *Pkg = Owner->PkgP + S->LastPackage;
		if (strcasecmp(myArch, Owner->StrP + Pkg->Arch) == 0)
			return PkgIterator(*Owner, Pkg);
		Arch = myArch;
	}

	/* Iterate over the list to find the matching arch
	   unfortunately this list includes "package noise"
	   (= different packages with same calculated hash),
	   so we need to check the name also */
	for (pkgCache::Package *Pkg = PackageList(); Pkg != Owner->PkgP;
	     Pkg = Owner->PkgP + Pkg->NextPackage) {
		if (S->Name == Pkg->Name &&
		    stringcasecmp(Arch, Owner->StrP + Pkg->Arch) == 0)
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
	pkgCache::PkgIterator Pkg = FindPkg("native");
	if (Pkg.end() == false && (PreferNonVirtual == false || Pkg->VersionList != 0))
		return Pkg;

	std::vector<std::string> const archs = APT::Configuration::getArchitectures();
	for (std::vector<std::string>::const_iterator a = archs.begin();
	     a != archs.end(); ++a) {
		Pkg = FindPkg(*a);
		if (Pkg.end() == false && (PreferNonVirtual == false || Pkg->VersionList != 0))
			return Pkg;
	}

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

	if (S->LastPackage == LastPkg.Index())
		return PkgIterator(*Owner, 0);

	return PkgIterator(*Owner, Owner->PkgP + LastPkg->NextPackage);
}
									/*}}}*/
// GrpIterator::operator ++ - Postfix incr				/*{{{*/
// ---------------------------------------------------------------------
/* This will advance to the next logical group in the hash table. */
void pkgCache::GrpIterator::operator ++(int) 
{
   // Follow the current links
   if (S != Owner->GrpP)
      S = Owner->GrpP + S->Next;

   // Follow the hash table
   while (S == Owner->GrpP && (HashIndex+1) < (signed)_count(Owner->HeaderP->GrpHashTable))
   {
      HashIndex++;
      S = Owner->GrpP + Owner->HeaderP->GrpHashTable[HashIndex];
   }
};
									/*}}}*/
// PkgIterator::operator ++ - Postfix incr				/*{{{*/
// ---------------------------------------------------------------------
/* This will advance to the next logical package in the hash table. */
void pkgCache::PkgIterator::operator ++(int) 
{
   // Follow the current links
   if (S != Owner->PkgP)
      S = Owner->PkgP + S->NextPackage;

   // Follow the hash table
   while (S == Owner->PkgP && (HashIndex+1) < (signed)_count(Owner->HeaderP->PkgHashTable))
   {
      HashIndex++;
      S = Owner->PkgP + Owner->HeaderP->PkgHashTable[HashIndex];
   }
};
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
      // we leave triggers alone complettely. dpkg deals with
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
// PkgIterator::CandVersion - Returns the candidate version string	/*{{{*/
// ---------------------------------------------------------------------
/* Return string representing of the candidate version. */
const char *
pkgCache::PkgIterator::CandVersion() const 
{
  //TargetVer is empty, so don't use it.
  VerIterator version = pkgPolicy(Owner).GetCandidateVer(*this);
  if (version.IsGood())
    return version.VerStr();
  return 0;
};
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
};
									/*}}}*/
// ostream operator to handle string representation of a package	/*{{{*/
// ---------------------------------------------------------------------
/* Output name < cur.rent.version -> candid.ate.version | new.est.version > (section)
   Note that the characters <|>() are all literal above. Versions will be ommited
   if they provide no new information (e.g. there is no newer version than candidate)
   If no version and/or section can be found "none" is used. */
std::ostream& 
operator<<(ostream& out, pkgCache::PkgIterator Pkg) 
{
   if (Pkg.end() == true)
      return out << "invalid package";

   string current = string(Pkg.CurVersion() == 0 ? "none" : Pkg.CurVersion());
   string candidate = string(Pkg.CandVersion() == 0 ? "none" : Pkg.CandVersion());
   string newest = string(Pkg.VersionList().end() ? "none" : Pkg.VersionList().VerStr());

   out << Pkg.Name() << " [ " << Pkg.Arch() << " ] < " << current;
   if (current != candidate)
      out << " -> " << candidate;
   if ( newest != "none" && candidate != newest)
      out << " | " << newest;
   out << " > ( " << string(Pkg.Section()==0?"none":Pkg.Section()) << " )";
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
       (strcmp(Arch(), "all") != 0 &&
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
   if (S->Type == pkgCache::Dep::Conflicts ||
       S->Type == pkgCache::Dep::DpkgBreaks ||
       S->Type == pkgCache::Dep::Obsoletes ||
       S->Type == pkgCache::Dep::Depends ||
       S->Type == pkgCache::Dep::PreDepends)
      return true;
   return false;
}
									/*}}}*/
// DepIterator::SmartTargetPkg - Resolve dep target pointers w/provides	/*{{{*/
// ---------------------------------------------------------------------
/* This intellegently looks at dep target packages and tries to figure
   out which package should be used. This is needed to nicely handle
   provide mapping. If the target package has no other providing packages
   then it returned. Otherwise the providing list is looked at to 
   see if there is one one unique providing package if so it is returned.
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
   for (; PStart.end() != true && PStart.OwnerPkg() == ParentPkg(); PStart++);

   // Nothing but indirect self provides
   if (PStart.end() == true)
      return false;
   
   // Check for single packages in the provides list
   PrvIterator P = PStart;
   for (; P.end() != true; P++)
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
      for (VerIterator I = DPkg.VersionList(); I.end() == false; I++)
      {
	 if (Owner->VS->CheckDep(I.VerStr(),S->CompareOp,TargetVer()) == false)
	    continue;

	 if ((S->Type == pkgCache::Dep::Conflicts ||
	      S->Type == pkgCache::Dep::DpkgBreaks ||
	      S->Type == pkgCache::Dep::Obsoletes) &&
	     ParentPkg() == I.ParentPkg())
	    continue;
	 
	 Size++;
	 if (Res != 0)
	    *End++ = I;
      }
      
      // Follow all provides
      for (PrvIterator I = DPkg.ProvidesList(); I.end() == false; I++)
      {
	 if (Owner->VS->CheckDep(I.ProvideVersion(),S->CompareOp,TargetVer()) == false)
	    continue;
	 
	 if ((S->Type == pkgCache::Dep::Conflicts ||
	      S->Type == pkgCache::Dep::DpkgBreaks ||
	      S->Type == pkgCache::Dep::Obsoletes) &&
	     ParentPkg() == I.OwnerPkg())
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
      LastOR = (S->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
      (*this)++;
      if (LastOR == true)
	 End = (*this);
   }
}
									/*}}}*/
// ostream operator to handle string representation of a dependecy	/*{{{*/
// ---------------------------------------------------------------------
/* */
std::ostream& operator<<(ostream& out, pkgCache::DepIterator D)
{
   if (D.end() == true)
      return out << "invalid dependency";

   pkgCache::PkgIterator P = D.ParentPkg();
   pkgCache::PkgIterator T = D.TargetPkg();

   out << (P.end() ? "invalid pkg" : P.FullName(false)) << " " << D.DepType()
	<< " on ";
   if (T.end() == true)
      out << "invalid pkg";
   else
      out << T;

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
   for (;I.end() == false; I++)
      if (I == B)
	 return 1;
   return -1;
}
									/*}}}*/
// VerIterator::Downloadable - Checks if the version is downloadable	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCache::VerIterator::Downloadable() const
{
   VerFileIterator Files = FileList();
   for (; Files.end() == false; Files++)
      if ((Files.File()->Flags & pkgCache::Flag::NotSource) != pkgCache::Flag::NotSource)
	 return true;
   return false;
}
									/*}}}*/
// VerIterator::Automatic - Check if this version is 'automatic'	/*{{{*/
// ---------------------------------------------------------------------
/* This checks to see if any of the versions files are not NotAutomatic. 
   True if this version is selectable for automatic installation. */
bool pkgCache::VerIterator::Automatic() const
{
   VerFileIterator Files = FileList();
   for (; Files.end() == false; Files++)
      // Do not check ButAutomaticUpgrades here as it is kind of automatic…
      if ((Files.File()->Flags & pkgCache::Flag::NotAutomatic) != pkgCache::Flag::NotAutomatic)
	 return true;
   return false;
}
									/*}}}*/
// VerIterator::Pseudo - deprecated no-op method			/*{{{*/
bool pkgCache::VerIterator::Pseudo() const { return false; }
									/*}}}*/
// VerIterator::NewestFile - Return the newest file version relation	/*{{{*/
// ---------------------------------------------------------------------
/* This looks at the version numbers associated with all of the sources
   this version is in and returns the highest.*/
pkgCache::VerFileIterator pkgCache::VerIterator::NewestFile() const
{
   VerFileIterator Files = FileList();
   VerFileIterator Highest = Files;
   for (; Files.end() == false; Files++)
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
string pkgCache::VerIterator::RelStr() const
{
   bool First = true;
   string Res;
   for (pkgCache::VerFileIterator I = this->FileList(); I.end() == false; I++)
   {
      // Do not print 'not source' entries'
      pkgCache::PkgFileIterator File = I.File();
      if ((File->Flags & pkgCache::Flag::NotSource) == pkgCache::Flag::NotSource)
	 continue;

      // See if we have already printed this out..
      bool Seen = false;
      for (pkgCache::VerFileIterator J = this->FileList(); I != J; J++)
      {
	 pkgCache::PkgFileIterator File2 = J.File();
	 if (File2->Label == 0 || File->Label == 0)
	    continue;

	 if (strcmp(File.Label(),File2.Label()) != 0)
	    continue;
	 
	 if (File2->Version == File->Version)
	 {
	    Seen = true;
	    break;
	 }
	 if (File2->Version == 0 || File->Version == 0)
	    break;
	 if (strcmp(File.Version(),File2.Version()) == 0)
	    Seen = true;
      }
      
      if (Seen == true)
	 continue;
      
      if (First == false)
	 Res += ", ";
      else
	 First = false;
      
      if (File->Label != 0)
	 Res = Res + File.Label() + ':';

      if (File->Archive != 0)
      {
	 if (File->Version == 0)
	    Res += File.Archive();
	 else
	    Res = Res + File.Version() + '/' +  File.Archive();
      }
      else
      {
	 // No release file, print the host name that this came from
	 if (File->Site == 0 || File.Site()[0] == 0)
	    Res += "localhost";
	 else
	    Res += File.Site();
      }      
   }
   if (S->ParentPkg != 0)
      Res.append(" [").append(Arch()).append("]");
   return Res;
}
									/*}}}*/
// PkgFileIterator::IsOk - Checks if the cache is in sync with the file	/*{{{*/
// ---------------------------------------------------------------------
/* This stats the file and compares its stats with the ones that were
   stored during generation. Date checks should probably also be 
   included here. */
bool pkgCache::PkgFileIterator::IsOk()
{
   struct stat Buf;
   if (stat(FileName(),&Buf) != 0)
      return false;

   if (Buf.st_size != (signed)S->Size || Buf.st_mtime != S->mtime)
      return false;

   return true;
}
									/*}}}*/
// PkgFileIterator::RelStr - Return the release string			/*{{{*/
// ---------------------------------------------------------------------
/* */
string pkgCache::PkgFileIterator::RelStr()
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
   if (Component() != 0)
      Res = Res + (Res.empty() == true?"c=":",c=")  + Component();
   if (Architecture() != 0)
      Res = Res + (Res.empty() == true?"b=":",b=")  + Architecture();
   return Res;
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
	l != lang.end(); l++)
   {
      pkgCache::DescIterator Desc = DescriptionList();
      for (; Desc.end() == false; ++Desc)
	 if (*l == Desc.LanguageCode() ||
	     (*l == "en" && strcmp(Desc.LanguageCode(),"") == 0))
	    break;
      if (Desc.end() == true)
	 continue;
      return Desc;
   }
   for (pkgCache::DescIterator Desc = DescriptionList();
	Desc.end() == false; ++Desc)
      if (strcmp(Desc.LanguageCode(), "") == 0)
	 return Desc;
   return DescriptionList();
};

									/*}}}*/

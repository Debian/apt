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
#include <apt-pkg/indexfile.h>
#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/configuration.h>

#include <apti18n.h>
    
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
#include <system.h>
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
   MajorVersion = 7;
   MinorVersion = 0;
   Dirty = false;
   
   HeaderSz = sizeof(pkgCache::Header);
   PackageSz = sizeof(pkgCache::Package);
   PackageFileSz = sizeof(pkgCache::PackageFile);
   VersionSz = sizeof(pkgCache::Version);
   DescriptionSz = sizeof(pkgCache::Description);
   DependencySz = sizeof(pkgCache::Dependency);
   ProvidesSz = sizeof(pkgCache::Provides);
   VerFileSz = sizeof(pkgCache::VerFile);
   DescFileSz = sizeof(pkgCache::DescFile);
   
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
   memset(HashTable,0,sizeof(HashTable));
   memset(Pools,0,sizeof(Pools));
}
									/*}}}*/
// Cache::Header::CheckSizes - Check if the two headers have same *sz	/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgCache::Header::CheckSizes(Header &Against) const
{
   if (HeaderSz == Against.HeaderSz &&
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
   if (DoMap == true)
      ReMap();
}
									/*}}}*/
// Cache::ReMap - Reopen the cache file					/*{{{*/
// ---------------------------------------------------------------------
/* If the file is already closed then this will open it open it. */
bool pkgCache::ReMap()
{
   // Apply the typecasts.
   HeaderP = (Header *)Map.Data();
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
      Hash = 5*Hash + tolower(*I);
   return Hash % _count(HeaderP->HashTable);
}

unsigned long pkgCache::sHash(const char *Str) const
{
   unsigned long Hash = 0;
   for (const char *I = Str; *I != 0; I++)
      Hash = 5*Hash + tolower(*I);
   return Hash % _count(HeaderP->HashTable);
}

									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(const string &Name)
{
   // Look at the hash bucket
   Package *Pkg = PkgP + HeaderP->HashTable[Hash(Name)];
   for (; Pkg != PkgP; Pkg = PkgP + Pkg->NextPackage)
   {
      if (Pkg->Name != 0 && StrP[Pkg->Name] == Name[0] &&
	  stringcasecmp(Name,StrP + Pkg->Name) == 0)
	 return PkgIterator(*this,Pkg);
   }
   return PkgIterator(*this,0);
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
                          _("Obsoletes"),_("Breaks")};
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
// Bases for iterator classes						/*{{{*/
void pkgCache::VerIterator::_dummy() {}
void pkgCache::DepIterator::_dummy() {}
void pkgCache::PrvIterator::_dummy() {}
void pkgCache::DescIterator::_dummy() {}
									/*}}}*/
// PkgIterator::operator ++ - Postfix incr				/*{{{*/
// ---------------------------------------------------------------------
/* This will advance to the next logical package in the hash table. */
void pkgCache::PkgIterator::operator ++(int) 
{
   // Follow the current links
   if (Pkg != Owner->PkgP)
      Pkg = Owner->PkgP + Pkg->NextPackage;

   // Follow the hash table
   while (Pkg == Owner->PkgP && (HashIndex+1) < (signed)_count(Owner->HeaderP->HashTable))
   {
      HashIndex++;
      Pkg = Owner->PkgP + Owner->HeaderP->HashTable[HashIndex];
   }
};
									/*}}}*/
// PkgIterator::State - Check the State of the package			/*{{{*/
// ---------------------------------------------------------------------
/* By this we mean if it is either cleanly installed or cleanly removed. */
pkgCache::PkgIterator::OkState pkgCache::PkgIterator::State() const
{  
   if (Pkg->InstState == pkgCache::State::ReInstReq ||
       Pkg->InstState == pkgCache::State::HoldReInstReq)
      return NeedsUnpack;
   
   if (Pkg->CurrentState == pkgCache::State::UnPacked ||
       Pkg->CurrentState == pkgCache::State::HalfConfigured ||
       Pkg->CurrentState == pkgCache::State::TriggersPending ||
       Pkg->CurrentState == pkgCache::State::TriggersAwaited)
      return NeedsConfigure;
   
   if (Pkg->CurrentState == pkgCache::State::HalfInstalled ||
       Pkg->InstState != pkgCache::State::Ok)
      return NeedsUnpack;
      
   return NeedsNothing;
}
									/*}}}*/
// DepIterator::IsCritical - Returns true if the dep is important	/*{{{*/
// ---------------------------------------------------------------------
/* Currently critical deps are defined as depends, predepends and
   conflicts (including dpkg's Breaks fields). */
bool pkgCache::DepIterator::IsCritical()
{
   if (Dep->Type == pkgCache::Dep::Conflicts ||
       Dep->Type == pkgCache::Dep::DpkgBreaks ||
       Dep->Type == pkgCache::Dep::Obsoletes ||
       Dep->Type == pkgCache::Dep::Depends ||
       Dep->Type == pkgCache::Dep::PreDepends)
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
bool pkgCache::DepIterator::SmartTargetPkg(PkgIterator &Result)
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
pkgCache::Version **pkgCache::DepIterator::AllTargets()
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
	 if (Owner->VS->CheckDep(I.VerStr(),Dep->CompareOp,TargetVer()) == false)
	    continue;

	 if ((Dep->Type == pkgCache::Dep::Conflicts ||
	      Dep->Type == pkgCache::Dep::DpkgBreaks ||
	      Dep->Type == pkgCache::Dep::Obsoletes) &&
	     ParentPkg() == I.ParentPkg())
	    continue;
	 
	 Size++;
	 if (Res != 0)
	    *End++ = I;
      }
      
      // Follow all provides
      for (PrvIterator I = DPkg.ProvidesList(); I.end() == false; I++)
      {
	 if (Owner->VS->CheckDep(I.ProvideVersion(),Dep->CompareOp,TargetVer()) == false)
	    continue;
	 
	 if ((Dep->Type == pkgCache::Dep::Conflicts ||
	      Dep->Type == pkgCache::Dep::DpkgBreaks ||
	      Dep->Type == pkgCache::Dep::Obsoletes) &&
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
      LastOR = (Dep->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
      (*this)++;
      if (LastOR == true)
	 End = (*this);
   }
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
      if ((Files.File()->Flags & pkgCache::Flag::NotAutomatic) != pkgCache::Flag::NotAutomatic)
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
string pkgCache::VerIterator::RelStr()
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

   if (Buf.st_size != (signed)File->Size || Buf.st_mtime != File->mtime)
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
   if (Label() != 0)
      Res = Res + (Res.empty() == true?"l=":",l=")  + Label();
   if (Component() != 0)
      Res = Res + (Res.empty() == true?"c=":",c=")  + Component();
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
   pkgCache::DescIterator DescDefault = DescriptionList();
   pkgCache::DescIterator Desc = DescDefault;
   for (; Desc.end() == false; Desc++)
      if (pkgIndexFile::LanguageCode() == Desc.LanguageCode())
	 break;
   if (Desc.end() == true) 
      Desc = DescDefault;
   return Desc;
};

									/*}}}*/

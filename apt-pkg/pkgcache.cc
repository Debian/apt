// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcache.cc,v 1.7 1998/07/12 23:58:32 jgg Exp $
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
#ifdef __GNUG__
#pragma implementation "apt-pkg/pkgcache.h"
#pragma implementation "apt-pkg/cacheiterators.h"
#endif 
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
#include <system.h>

#include <string>
#include <sys/stat.h>
#include <unistd.h>
									/*}}}*/

// Cache::Header::Header - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Simply initialize the header */
pkgCache::Header::Header()
{
   Signature = 0x98FE76DC;
   
   /* Whenever the structures change the major version should be bumped,
      whenever the generator changes the minor version should be bumped. */
   MajorVersion = 2;
   MinorVersion = 0;
   Dirty = true;
   
   HeaderSz = sizeof(pkgCache::Header);
   PackageSz = sizeof(pkgCache::Package);
   PackageFileSz = sizeof(pkgCache::PackageFile);
   VersionSz = sizeof(pkgCache::Version);
   DependencySz = sizeof(pkgCache::Dependency);
   ProvidesSz = sizeof(pkgCache::Provides);
   VerFileSz = sizeof(pkgCache::VerFile);
   
   PackageCount = 0;
   VersionCount = 0;
   DependsCount = 0;
   PackageFileCount = 0;
   
   FileList = 0;
   StringList = 0;
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
       DependencySz == Against.DependencySz &&
       VerFileSz == Against.VerFileSz &&
       ProvidesSz == Against.ProvidesSz)
      return true;
   return false;
}
									/*}}}*/

// Cache::pkgCache - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache::pkgCache(MMap &Map) : Map(Map)
{
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
   PkgFileP = (PackageFile *)Map.Data();
   VerP = (Version *)Map.Data();
   ProvideP = (Provides *)Map.Data();
   DepP = (Dependency *)Map.Data();
   StringItemP = (StringItem *)Map.Data();
   StrP = (char *)Map.Data();

   if (Map.Size() == 0)
      return false;
   
   // Check the header
   Header DefHeader;
   if (HeaderP->Signature != DefHeader.Signature ||
       HeaderP->Dirty == true)
      return _error->Error("The package cache file is corrupted");
   
   if (HeaderP->MajorVersion != DefHeader.MajorVersion ||
       HeaderP->MinorVersion != DefHeader.MinorVersion ||
       HeaderP->CheckSizes(DefHeader) == false)
      return _error->Error("The package cache file is an incompatible version");
   
   return true;
}
									/*}}}*/
// Cache::Hash - Hash a string						/*{{{*/
// ---------------------------------------------------------------------
/* This is used to generate the hash entries for the HashTable. With my
   package list from bo this function gets 94% table usage on a 512 item
   table (480 used items) */
unsigned long pkgCache::sHash(string Str)
{
   unsigned long Hash = 0;
   for (const char *I = Str.begin(); I != Str.end(); I++)
      Hash += *I * ((Str.end() - I + 1));
   Header H;
   return Hash % _count(H.HashTable);
}

unsigned long pkgCache::sHash(const char *Str)
{
   unsigned long Hash = 0;
   const char *End = Str + strlen(Str);
   for (const char *I = Str; I != End; I++)
      Hash += *I * ((End - I + 1));
   Header H;
   return Hash % _count(H.HashTable);
}

									/*}}}*/
// Cache::FindPkg - Locate a package by name				/*{{{*/
// ---------------------------------------------------------------------
/* Returns 0 on error, pointer to the package otherwise */
pkgCache::PkgIterator pkgCache::FindPkg(string Name)
{
   // Look at the hash bucket
   Package *Pkg = PkgP + HeaderP->HashTable[Hash(Name)];
   for (; Pkg != PkgP; Pkg = PkgP + Pkg->NextPackage)
   {
      if (Pkg->Name != 0 && StrP + Pkg->Name == Name)
	 return PkgIterator(*this,Pkg);
   }
   return PkgIterator(*this,0);
}
									/*}}}*/
// Cache::Priority - Convert a priority value to a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgCache::Priority(unsigned char Prio)
{
   const char *Mapping[] = {0,"important","required","standard","optional","extra"};
   if (Prio < _count(Mapping))
      return Mapping[Prio];
   return 0;
}
									/*}}}*/

// Bases for iterator classes						/*{{{*/
void pkgCache::VerIterator::_dummy() {}
void pkgCache::DepIterator::_dummy() {}
void pkgCache::PrvIterator::_dummy() {}
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
   while (Pkg == Owner->PkgP && HashIndex < (signed)_count(Owner->HeaderP->HashTable))
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
   if (Pkg->CurrentState == State::UnPacked ||
       Pkg->CurrentState == State::HalfConfigured)
      return NeedsConfigure;
   
   if (Pkg->CurrentState == State::UnInstalled ||
       Pkg->CurrentState == State::HalfInstalled ||
       Pkg->InstState != State::Ok)
      return NeedsUnpack;
      
   return NeedsNothing;
}
									/*}}}*/
// DepIterator::IsCritical - Returns true if the dep is important	/*{{{*/
// ---------------------------------------------------------------------
/* Currently critical deps are defined as depends, predepends and
   conflicts. */
bool pkgCache::DepIterator::IsCritical()
{
   if (Dep->Type == Dep::Conflicts || Dep->Type == Dep::Depends ||
       Dep->Type == Dep::PreDepends)
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
   result indicates whether the node should be expandable */
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
   
   // Check for non dups
   if (P.end() != true)
      return true;
   Result = PStart.OwnerPkg();
   return false;
}
									/*}}}*/
// DepIterator::AllTargets - Returns the set of all possible targets	/*{{{*/
// ---------------------------------------------------------------------
/* This is a more usefull version of TargetPkg() that follows versioned
   provides. It includes every possible package-version that could satisfy
   the dependency. The last item in the list has a 0. */
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
	 if (pkgCheckDep(TargetVer(),I.VerStr(),Dep->CompareOp) == false)
	    continue;

	 if (Dep->Type == Dep::Conflicts && ParentPkg() == I.ParentPkg())
	    continue;
	 
	 Size++;
	 if (Res != 0)
	    *End++ = I;
      }
      
      // Follow all provides
      for (PrvIterator I = DPkg.ProvidesList(); I.end() == false; I++)
      {
	 if (pkgCheckDep(TargetVer(),I.ProvideVersion(),Dep->CompareOp) == false)
	    continue;
	 
	 if (Dep->Type == Dep::Conflicts && ParentPkg() == I.OwnerPkg())
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
      if ((Files.File()->Flags & Flag::NotSource) != Flag::NotSource)
	 return true;
   return false;
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

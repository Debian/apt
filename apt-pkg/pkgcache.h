// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcache.h,v 1.21 1999/07/15 03:15:48 jgg Exp $
/* ######################################################################
   
   Cache - Structure definitions for the cache file
   
   Please see doc/apt-pkg/cache.sgml for a more detailed description of 
   this format. Also be sure to keep that file up-to-date!!
   
   Clients should always use the CacheIterators classes for access to the
   cache. They provide a simple STL-like method for traversing the links
   of the datastructure.
   
   See pkgcachegen.h for information about generating cache structures.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_PKGCACHE_H
#define PKGLIB_PKGCACHE_H

#ifdef __GNUG__
#pragma interface "apt-pkg/pkgcache.h"
#endif 

#include <string>
#include <time.h>
#include <apt-pkg/mmap.h>

class pkgCache
{
   public:
   // Cache element predeclarations
   struct Header;
   struct Package;
   struct PackageFile;
   struct Version;
   struct Provides;
   struct Dependency;
   struct StringItem;
   struct VerFile;
   
   // Iterators
   class PkgIterator;
   class VerIterator;
   class DepIterator;
   class PrvIterator;
   class PkgFileIterator;
   class VerFileIterator;
   friend PkgIterator;
   friend VerIterator;
   friend DepIterator;
   friend PrvIterator;
   friend PkgFileIterator;
   friend VerFileIterator;
   
   // These are all the constants used in the cache structures
   struct Dep
   {
      enum DepType {Depends=1,PreDepends=2,Suggests=3,Recommends=4,
	 Conflicts=5,Replaces=6};
      enum DepCompareOp {Or=0x10,NoOp=0,LessEq=0x1,GreaterEq=0x2,Less=0x3,
	 Greater=0x4,Equals=0x5,NotEquals=0x6};
   };
   
   struct State
   {
      enum VerPriority {Important=1,Required=2,Standard=3,Optional=4,Extra=5};
      enum PkgSelectedState {Unknown=0,Install=1,Hold=2,DeInstall=3,Purge=4};
      enum PkgInstState {Ok=0,ReInstReq=1,HoldInst=2,HoldReInstReq=3};
      enum PkgCurrentState {NotInstalled=0,UnPacked=1,HalfConfigured=2,
	 UnInstalled=3,HalfInstalled=4,ConfigFiles=5,Installed=6};
   };
   
   struct Flag
   {
      enum PkgFlags {Auto=(1<<0),Essential=(1<<3),Important=(1<<4)};
      enum PkgFFlags {NotSource=(1<<0),NotAutomatic=(1<<1)};
   };
   
   protected:
   
   // Memory mapped cache file
   string CacheFile;
   MMap &Map;

   unsigned long sHash(string S) const;
   unsigned long sHash(const char *S) const;
   
   public:
   
   // Pointers to the arrays of items
   Header *HeaderP;
   Package *PkgP;
   VerFile *VerFileP;
   PackageFile *PkgFileP;
   Version *VerP;
   Provides *ProvideP;
   Dependency *DepP;
   StringItem *StringItemP;
   char *StrP;

   virtual bool ReMap();
   inline bool Sync() {return Map.Sync();};
   inline MMap &GetMap() {return Map;};
   
   // String hashing function (512 range)
   inline unsigned long Hash(string S) const {return sHash(S);};
   inline unsigned long Hash(const char *S) const {return sHash(S);};

   // Usefull transformation things
   const char *Priority(unsigned char Priority);
   
   // Accessors
   PkgIterator FindPkg(string Name);
   Header &Head() {return *HeaderP;};
   inline PkgIterator PkgBegin();
   inline PkgIterator PkgEnd();
   inline PkgFileIterator FileBegin();
   inline PkgFileIterator FileEnd();
   VerIterator GetCandidateVer(PkgIterator Pkg,bool AllowCurrent = true);
   
   pkgCache(MMap &Map);
   virtual ~pkgCache() {};
};

// Header structure
struct pkgCache::Header
{
   // Signature information
   unsigned long Signature;
   short MajorVersion;
   short MinorVersion;
   bool Dirty;
   
   // Size of structure values
   unsigned short HeaderSz;
   unsigned short PackageSz;
   unsigned short PackageFileSz;
   unsigned short VersionSz;
   unsigned short DependencySz;
   unsigned short ProvidesSz;
   unsigned short VerFileSz;
   
   // Structure counts
   unsigned long PackageCount;
   unsigned long VersionCount;
   unsigned long DependsCount;
   unsigned long PackageFileCount;
   unsigned long VerFileCount;
   unsigned long ProvidesCount;
   
   // Offsets
   map_ptrloc FileList;              // struct PackageFile
   map_ptrloc StringList;            // struct StringItem
   unsigned long MaxVerFileSize;

   /* Allocation pools, there should be one of these for each structure
      excluding the header */
   DynamicMMap::Pool Pools[7];
   
   // Rapid package name lookup
   map_ptrloc HashTable[2*1048];

   bool CheckSizes(Header &Against) const;
   Header();
};

struct pkgCache::Package
{
   // Pointers
   map_ptrloc Name;              // Stringtable
   map_ptrloc VersionList;       // Version
   map_ptrloc TargetVer;         // Version
   map_ptrloc CurrentVer;        // Version
   map_ptrloc TargetDist;        // StringTable (StringItem)
   map_ptrloc Section;           // StringTable (StringItem)
      
   // Linked list 
   map_ptrloc NextPackage;       // Package
   map_ptrloc RevDepends;        // Dependency
   map_ptrloc ProvidesList;      // Provides
   
   // Install/Remove/Purge etc
   unsigned char SelectedState;     // What
   unsigned char InstState;         // Flags
   unsigned char CurrentState;      // State
   
   unsigned short ID;
   unsigned long Flags;
};

struct pkgCache::PackageFile
{
   // Names
   map_ptrloc FileName;        // Stringtable
   map_ptrloc Archive;         // Stringtable
   map_ptrloc Component;       // Stringtable
   map_ptrloc Version;         // Stringtable
   map_ptrloc Origin;          // Stringtable
   map_ptrloc Label;           // Stringtable
   map_ptrloc Architecture;    // Stringtable
   unsigned long Size;            
   unsigned long Flags;
   
   // Linked list
   map_ptrloc NextFile;        // PackageFile
   unsigned short ID;
   time_t mtime;                  // Modification time for the file
};

struct pkgCache::VerFile
{
   map_ptrloc File;           // PackageFile
   map_ptrloc NextFile;       // PkgVerFile
   map_ptrloc Offset;         // File offset
   unsigned short Size;
};

struct pkgCache::Version
{
   map_ptrloc VerStr;            // Stringtable
   map_ptrloc Section;           // StringTable (StringItem)
   map_ptrloc Arch;              // StringTable
      
   // Lists
   map_ptrloc FileList;          // VerFile
   map_ptrloc NextVer;           // Version
   map_ptrloc DependsList;       // Dependency
   map_ptrloc ParentPkg;         // Package
   map_ptrloc ProvidesList;      // Provides
   
   map_ptrloc Size;              // These are the .deb size
   map_ptrloc InstalledSize;
   unsigned short Hash;
   unsigned short ID;
   unsigned char Priority;
};

struct pkgCache::Dependency
{
   map_ptrloc Version;         // Stringtable
   map_ptrloc Package;         // Package
   map_ptrloc NextDepends;     // Dependency
   map_ptrloc NextRevDepends;  // Dependency
   map_ptrloc ParentVer;       // Version
   
   // Specific types of depends
   unsigned char Type;
   unsigned char CompareOp;
   unsigned short ID;
};

struct pkgCache::Provides
{
   map_ptrloc ParentPkg;        // Pacakge
   map_ptrloc Version;          // Version
   map_ptrloc ProvideVersion;   // Stringtable
   map_ptrloc NextProvides;     // Provides
   map_ptrloc NextPkgProv;      // Provides
};

struct pkgCache::StringItem
{
   map_ptrloc String;        // Stringtable
   map_ptrloc NextItem;      // StringItem
};

#include <apt-pkg/cacheiterators.h>

inline pkgCache::PkgIterator pkgCache::PkgBegin() 
       {return PkgIterator(*this);};
inline pkgCache::PkgIterator pkgCache::PkgEnd() 
       {return PkgIterator(*this,PkgP);};
inline pkgCache::PkgFileIterator pkgCache::FileBegin()
       {return PkgFileIterator(*this);};
inline pkgCache::PkgFileIterator pkgCache::FileEnd()
       {return PkgFileIterator(*this,PkgFileP);};

#endif

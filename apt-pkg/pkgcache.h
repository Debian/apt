// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcache.h,v 1.7 1998/07/19 04:22:01 jgg Exp $
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
      enum VerPriority {Important=1,Required=2,Standard=3,Optional=5,Extra=5};
      enum PkgSelectedState {Unknown=0,Install=1,Hold=2,DeInstall=3,Purge=4};
      enum PkgInstState {Ok=0,ReInstReq=1,HoldInst=2,HoldReInstReq=3};
      enum PkgCurrentState {NotInstalled=0,UnPacked=1,HalfConfigured=2,
	 UnInstalled=3,HalfInstalled=4,ConfigFiles=5,Installed=6};
   };
   
   struct Flag
   {
      enum PkgFlags {Auto=(1<<0),New=(1<<1),Obsolete=(1<<2),Essential=(1<<3),
	 ImmediateConf=(1<<4)};
      enum PkgFFlags {NotSource=(1<<0)};
   };
   
   protected:
   
   // Memory mapped cache file
   string CacheFile;
   MMap &Map;

   bool Public;
   bool ReadOnly;

   static unsigned long sHash(string S);
   static unsigned long sHash(const char *S);
   
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
   
   // Offsets
   unsigned long FileList;              // struct PackageFile
   unsigned long StringList;            // struct StringItem
   unsigned long MaxVerFileSize;

   /* Allocation pools, there should be one of these for each structure
      excluding the header */
   DynamicMMap::Pool Pools[7];
   
   // Rapid package name lookup
   unsigned long HashTable[512];

   bool CheckSizes(Header &Against) const;
   Header();
};

struct pkgCache::Package
{
   // Pointers
   unsigned long Name;              // Stringtable
   unsigned long VersionList;       // Version
   unsigned long TargetVer;         // Version
   unsigned long CurrentVer;        // Version
   unsigned long TargetDist;        // StringTable (StringItem)
   unsigned long Section;           // StringTable (StringItem)
      
   // Linked list 
   unsigned long NextPackage;       // Package
   unsigned long RevDepends;        // Dependency
   unsigned long ProvidesList;      // Provides
   
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
   unsigned long FileName;        // Stringtable
   unsigned long Version;         // Stringtable
   unsigned long Distribution;    // Stringtable
   unsigned long Size;
   
   // Linked list
   unsigned long NextFile;        // PackageFile
   unsigned short ID;
   unsigned long Flags;
   time_t mtime;                  // Modification time for the file
};

struct pkgCache::VerFile
{
   unsigned long File;           // PackageFile
   unsigned long NextFile;       // PkgVerFile
   unsigned long Offset;
   unsigned short Size;
};

struct pkgCache::Version
{
   unsigned long VerStr;            // Stringtable
   unsigned long Section;           // StringTable (StringItem)
   
   // Lists
   unsigned long FileList;          // VerFile
   unsigned long NextVer;           // Version
   unsigned long DependsList;       // Dependency
   unsigned long ParentPkg;         // Package
   unsigned long ProvidesList;      // Provides
   
   unsigned long Size;
   unsigned long InstalledSize;
   unsigned short ID;
   unsigned char Priority;
};

struct pkgCache::Dependency
{
   unsigned long Version;         // Stringtable
   unsigned long Package;         // Package
   unsigned long NextDepends;     // Dependency
   unsigned long NextRevDepends;  // Dependency
   unsigned long ParentVer;       // Version
   
   // Specific types of depends
   unsigned char Type;
   unsigned char CompareOp;
   unsigned short ID;
};

struct pkgCache::Provides
{
   unsigned long ParentPkg;        // Pacakge
   unsigned long Version;          // Version
   unsigned long ProvideVersion;   // Stringtable
   unsigned long NextProvides;     // Provides
   unsigned long NextPkgProv;      // Provides
};

struct pkgCache::StringItem
{
   unsigned long String;        // Stringtable
   unsigned long NextItem;      // StringItem
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: pkgcache.h,v 1.1 1998/07/02 02:58:12 jgg Exp $
/* ######################################################################
   
   Cache - Structure definitions for the cache file
   
   Please see doc/pkglib/cache.sgml for a more detailed description of 
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

#include <string>
#include <time.h>
#include <pkglib/mmap.h>

// Definitions for Depends::Type
#define pkgDEP_Depends 1
#define pkgDEP_PreDepends 2
#define pkgDEP_Suggests 3
#define pkgDEP_Recommends 4
#define pkgDEP_Conflicts 5
#define pkgDEP_Replaces 6

// Definitions for Version::Priority
#define pkgPRIO_Important 1
#define pkgPRIO_Required 2
#define pkgPRIO_Standard 3
#define pkgPRIO_Optional 4
#define pkgPRIO_Extra 5

// Definitions for Package::SelectedState
#define pkgSTATE_Unkown 0
#define pkgSTATE_Install 1
#define pkgSTATE_Hold 2
#define pkgSTATE_DeInstall 3
#define pkgSTATE_Purge 4

// Definitions for Package::Flags
#define pkgFLAG_Auto (1 << 0)
#define pkgFLAG_New (1 << 1)
#define pkgFLAG_Obsolete (1 << 2)
#define pkgFLAG_Essential (1 << 3)
#define pkgFLAG_ImmediateConf (1 << 4)

// Definitions for Package::InstState
#define pkgSTATE_Ok 0
#define pkgSTATE_ReInstReq 1
#define pkgSTATE_Hold 2
#define pkgSTATE_HoldReInstReq 3

// Definitions for Package::CurrentState
#define pkgSTATE_NotInstalled 0
#define pkgSTATE_UnPacked 1
#define pkgSTATE_HalfConfigured 2
#define pkgSTATE_UnInstalled 3
#define pkgSTATE_HalfInstalled 4
#define pkgSTATE_ConfigFiles 5
#define pkgSTATE_Installed 6

// Definitions for PackageFile::Flags
#define pkgFLAG_NotSource (1 << 0)

// Definitions for Dependency::CompareOp
#define pkgOP_OR 0x10
#define pkgOP_LESSEQ 0x1
#define pkgOP_GREATEREQ 0x2
#define pkgOP_LESS 0x3
#define pkgOP_GREATER 0x4
#define pkgOP_EQUALS 0x5
#define pkgOP_NOTEQUALS 0x6

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
   
   // Iterators
   class PkgIterator;
   class VerIterator;
   class DepIterator;
   class PrvIterator;
   class PkgFileIterator;
   friend PkgIterator;
   friend VerIterator;
   friend DepIterator;
   friend PrvIterator;
   friend PkgFileIterator;
   
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

   // Accessors
   PkgIterator FindPkg(string Name);
   Header &Head() {return *HeaderP;};
   inline PkgIterator PkgBegin();
   inline PkgIterator PkgEnd();

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

   // Structure counts
   unsigned long PackageCount;
   unsigned long VersionCount;
   unsigned long DependsCount;
   unsigned long PackageFileCount;
   
   // Offsets
   unsigned long FileList;              // struct PackageFile
   unsigned long StringList;            // struct StringItem

   /* Allocation pools, there should be one of these for each structure
      excluding the header */
   DynamicMMap::Pool Pools[6];
   
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
   unsigned short Flags;
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
   unsigned short Flags;
   time_t mtime;                  // Modification time for the file
};

struct pkgCache::Version
{
   unsigned long VerStr;            // Stringtable
   unsigned long File;              // PackageFile
   unsigned long Section;           // StringTable (StringItem)
   
   // Lists
   unsigned long NextVer;           // Version
   unsigned long DependsList;       // Dependency
   unsigned long ParentPkg;         // Package
   unsigned long ProvidesList;      // Provides
   
   unsigned long Offset;
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

#include <pkglib/cacheiterators.h>

inline pkgCache::PkgIterator pkgCache::PkgBegin() 
       {return PkgIterator(*this);};
inline pkgCache::PkgIterator pkgCache::PkgEnd() 
       {return PkgIterator(*this,PkgP);};

#endif

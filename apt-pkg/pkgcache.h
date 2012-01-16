// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/**\file pkgcache.h
   \brief pkgCache - Structure definitions for the cache file

   The goal of the cache file is two fold:
   Firstly to speed loading and processing of the package file array and
   secondly to reduce memory consumption of the package file array.

   The implementation is aimed at an environment with many primary package
   files, for instance someone that has a Package file for their CD-ROM, a
   Package file for the latest version of the distribution on the CD-ROM and a
   package file for the development version. Always present is the information
   contained in the status file which might be considered a separate package
   file.

   Please understand, this is designed as a <b>Cache file</b> it is not meant to be
   used on any system other than the one it was created for. It is not meant to
   be authoritative either, i.e. if a system crash or software failure occurs it
   must be perfectly acceptable for the cache file to be in an inconsistent
   state. Furthermore at any time the cache file may be erased without losing
   any information.

   Also the structures and storage layout is optimized for use by the APT
   and may not be suitable for all purposes. However it should be possible
   to extend it with associate cache files that contain other information.

   To keep memory use down the cache file only contains often used fields and
   fields that are inexpensive to store, the Package file has a full list of
   fields. Also the client may assume that all items are perfectly valid and
   need not perform checks against their correctness. Removal of information
   from the cache is possible, but blanks will be left in the file, and
   unused strings will also be present. The recommended implementation is to
   simply rebuild the cache each time any of the data files change. It is
   possible to add a new package file to the cache without any negative side
   effects.

   <b>Note on Pointer access</b>
   Clients should always use the CacheIterators classes for access to the
   cache and the data in it. They also provide a simple STL-like method for
   traversing the links of the datastructure.

   Every item in every structure is stored as the index to that structure.
   What this means is that once the files is mmaped every data access has to
   go through a fix up stage to get a real memory pointer. This is done
   by taking the index, multiplying it by the type size and then adding
   it to the start address of the memory block. This sounds complex, but
   in C it is a single array dereference. Because all items are aligned to
   their size and indexes are stored as multiples of the size of the structure
   the format is immediately portable to all possible architectures - BUT the
   generated files are -NOT-.

   This scheme allows code like this to be written:
   <example>
     void *Map = mmap(...);
     Package *PkgList = (Package *)Map;
     Header *Head = (Header *)Map;
     char *Strings = (char *)Map;
     cout << (Strings + PkgList[Head->HashTable[0]]->Name) << endl;
   </example>
   Notice the lack of casting or multiplication. The net result is to return
   the name of the first package in the first hash bucket, without error
   checks.

   The generator uses allocation pools to group similarly sized structures in
   large blocks to eliminate any alignment overhead. The generator also
   assures that no structures overlap and all indexes are unique. Although
   at first glance it may seem like there is the potential for two structures
   to exist at the same point the generator never allows this to happen.
   (See the discussion of free space pools)

   See \ref pkgcachegen.h for more information about generating cache structures. */
									/*}}}*/
#ifndef PKGLIB_PKGCACHE_H
#define PKGLIB_PKGCACHE_H

#include <string>
#include <time.h>
#include <apt-pkg/mmap.h>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
#endif

class pkgVersioningSystem;
class pkgCache								/*{{{*/
{
   public:
   // Cache element predeclarations
   struct Header;
   struct Group;
   struct Package;
   struct PackageFile;
   struct Version;
   struct Description;
   struct Provides;
   struct Dependency;
   struct StringItem;
   struct VerFile;
   struct DescFile;
   
   // Iterators
   template<typename Str, typename Itr> class Iterator;
   class GrpIterator;
   class PkgIterator;
   class VerIterator;
   class DescIterator;
   class DepIterator;
   class PrvIterator;
   class PkgFileIterator;
   class VerFileIterator;
   class DescFileIterator;
   
   class Namespace;
   
   // These are all the constants used in the cache structures

   // WARNING - if you change these lists you must also edit
   // the stringification in pkgcache.cc and also consider whether
   // the cache file will become incompatible.
   struct Dep
   {
      enum DepType {Depends=1,PreDepends=2,Suggests=3,Recommends=4,
	 Conflicts=5,Replaces=6,Obsoletes=7,DpkgBreaks=8,Enhances=9};
      /** \brief available compare operators

          The lower 4 bits are used to indicate what operator is being specified and
          the upper 4 bits are flags. OR indicates that the next package is
          or'd with the current package. */
      enum DepCompareOp {Or=0x10,NoOp=0,LessEq=0x1,GreaterEq=0x2,Less=0x3,
	 Greater=0x4,Equals=0x5,NotEquals=0x6};
   };
   
   struct State
   {
      /** \brief priority of a package version

          Zero is used for unparsable or absent Priority fields. */
      enum VerPriority {Important=1,Required=2,Standard=3,Optional=4,Extra=5};
      enum PkgSelectedState {Unknown=0,Install=1,Hold=2,DeInstall=3,Purge=4};
      enum PkgInstState {Ok=0,ReInstReq=1,HoldInst=2,HoldReInstReq=3};
      enum PkgCurrentState {NotInstalled=0,UnPacked=1,HalfConfigured=2,
	   HalfInstalled=4,ConfigFiles=5,Installed=6,
           TriggersAwaited=7,TriggersPending=8};
   };
   
   struct Flag
   {
      enum PkgFlags {Auto=(1<<0),Essential=(1<<3),Important=(1<<4)};
      enum PkgFFlags {NotSource=(1<<0),NotAutomatic=(1<<1),ButAutomaticUpgrades=(1<<2)};
   };
   
   protected:
   
   // Memory mapped cache file
   std::string CacheFile;
   MMap &Map;

   unsigned long sHash(const std::string &S) const;
   unsigned long sHash(const char *S) const;
   
   public:
   
   // Pointers to the arrays of items
   Header *HeaderP;
   Group *GrpP;
   Package *PkgP;
   VerFile *VerFileP;
   DescFile *DescFileP;
   PackageFile *PkgFileP;
   Version *VerP;
   Description *DescP;
   Provides *ProvideP;
   Dependency *DepP;
   StringItem *StringItemP;
   char *StrP;

   virtual bool ReMap(bool const &Errorchecks = true);
   inline bool Sync() {return Map.Sync();};
   inline MMap &GetMap() {return Map;};
   inline void *DataEnd() {return ((unsigned char *)Map.Data()) + Map.Size();};
      
   // String hashing function (512 range)
   inline unsigned long Hash(const std::string &S) const {return sHash(S);};
   inline unsigned long Hash(const char *S) const {return sHash(S);};

   // Useful transformation things
   const char *Priority(unsigned char Priority);
   
   // Accessors
   GrpIterator FindGrp(const std::string &Name);
   PkgIterator FindPkg(const std::string &Name);
   PkgIterator FindPkg(const std::string &Name, const std::string &Arch);

   Header &Head() {return *HeaderP;};
   inline GrpIterator GrpBegin();
   inline GrpIterator GrpEnd();
   inline PkgIterator PkgBegin();
   inline PkgIterator PkgEnd();
   inline PkgFileIterator FileBegin();
   inline PkgFileIterator FileEnd();

   inline bool MultiArchCache() const { return MultiArchEnabled; };
   inline char const * const NativeArch() const;

   // Make me a function
   pkgVersioningSystem *VS;
   
   // Converters
   static const char *CompTypeDeb(unsigned char Comp);
   static const char *CompType(unsigned char Comp);
   static const char *DepType(unsigned char Dep);
   
   pkgCache(MMap *Map,bool DoMap = true);
   virtual ~pkgCache() {};

private:
   bool MultiArchEnabled;
   PkgIterator SingleArchFindPkg(const std::string &Name);
};
									/*}}}*/
// Header structure							/*{{{*/
struct pkgCache::Header
{
   /** \brief Signature information

       This must contain the hex value 0x98FE76DC which is designed to
       verify that the system loading the image has the same byte order
       and byte size as the system saving the image */
   unsigned long Signature;
   /** These contain the version of the cache file */
   short MajorVersion;
   short MinorVersion;
   /** \brief indicates if the cache should be erased

       Dirty is true if the cache file was opened for reading, the client
       expects to have written things to it and have not fully synced it.
       The file should be erased and rebuilt if it is true. */
   bool Dirty;

   /** \brief Size of structure values

       All *Sz variables contains the sizeof() that particular structure.
       It is used as an extra consistency check on the structure of the file.

       If any of the size values do not exactly match what the client expects
       then the client should refuse the load the file. */
   unsigned short HeaderSz;
   unsigned short GroupSz;
   unsigned short PackageSz;
   unsigned short PackageFileSz;
   unsigned short VersionSz;
   unsigned short DescriptionSz;
   unsigned short DependencySz;
   unsigned short ProvidesSz;
   unsigned short VerFileSz;
   unsigned short DescFileSz;

   /** \brief Structure counts

       These indicate the number of each structure contained in the cache.
       PackageCount is especially useful for generating user state structures.
       See Package::Id for more info. */
   unsigned long GroupCount;
   unsigned long PackageCount;
   unsigned long VersionCount;
   unsigned long DescriptionCount;
   unsigned long DependsCount;
   unsigned long PackageFileCount;
   unsigned long VerFileCount;
   unsigned long DescFileCount;
   unsigned long ProvidesCount;

   /** \brief index of the first PackageFile structure

       The PackageFile structures are singly linked lists that represent
       all package files that have been merged into the cache. */
   map_ptrloc FileList;
   /** \brief index of the first StringItem structure

       The cache contains a list of all the unique strings (StringItems).
       The parser reads this list into memory so it can match strings
       against it.*/
   map_ptrloc StringList;
   /** \brief String representing the version system used */
   map_ptrloc VerSysName;
   /** \brief Architecture(s) the cache was built against */
   map_ptrloc Architecture;
   /** \brief The maximum size of a raw entry from the original Package file */
   unsigned long MaxVerFileSize;
   /** \brief The maximum size of a raw entry from the original Translation file */
   unsigned long MaxDescFileSize;

   /** \brief The Pool structures manage the allocation pools that the generator uses

       Start indicates the first byte of the pool, Count is the number of objects
       remaining in the pool and ItemSize is the structure size (alignment factor)
       of the pool. An ItemSize of 0 indicates the pool is empty. There should be
       the same number of pools as there are structure types. The generator
       stores this information so future additions can make use of any unused pool
       blocks. */
   DynamicMMap::Pool Pools[9];
   
   /** \brief hash tables providing rapid group/package name lookup

       Each group/package name is inserted into the hash table using pkgCache::Hash(const &string)
       By iterating over each entry in the hash table it is possible to iterate over
       the entire list of packages. Hash Collisions are handled with a singly linked
       list of packages based at the hash item. The linked list contains only
       packages that match the hashing function.
       In the PkgHashTable is it possible that multiple packages have the same name -
       these packages are stored as a sequence in the list.

       Beware: The Hashmethod assumes that the hash table sizes are equal */
   map_ptrloc PkgHashTable[2*1048];
   map_ptrloc GrpHashTable[2*1048];

   /** \brief Size of the complete cache file */
   unsigned long  CacheFileSize;

   bool CheckSizes(Header &Against) const;
   Header();
};
									/*}}}*/
// Group structure							/*{{{*/
/** \brief groups architecture depending packages together

    On or more packages with the same name form a group, so we have
    a simple way to access a package built for different architectures
    Group exists in a singly linked list of group records starting at
    the hash index of the name in the pkgCache::Header::GrpHashTable */
struct pkgCache::Group
{
   /** \brief Name of the group */
   map_ptrloc Name;		// StringItem

   // Linked List
   /** \brief Link to the first package which belongs to the group */
   map_ptrloc FirstPackage;	// Package
   /** \brief Link to the last package which belongs to the group */
   map_ptrloc LastPackage;	// Package
   /** \brief Link to the next Group */
   map_ptrloc Next;		// Group
   /** \brief unique sequel ID */
   unsigned int ID;

};
									/*}}}*/
// Package structure							/*{{{*/
/** \brief contains information for a single unique package

    There can be any number of versions of a given package.
    Package exists in a singly linked list of package records starting at
    the hash index of the name in the pkgCache::Header::PkgHashTable

    A package can be created for every architecture so package names are
    not unique, but it is garanteed that packages with the same name
    are sequencel ordered in the list. Packages with the same name can be
    accessed with the Group.
*/
struct pkgCache::Package
{
   /** \brief Name of the package */
   map_ptrloc Name;              // StringItem
   /** \brief Architecture of the package */
   map_ptrloc Arch;              // StringItem
   /** \brief Base of a singly linked list of versions

       Each structure represents a unique version of the package.
       The version structures contain links into PackageFile and the
       original text file as well as detailed information about the size
       and dependencies of the specific package. In this way multiple
       versions of a package can be cleanly handled by the system.
       Furthermore, this linked list is guaranteed to be sorted
       from Highest version to lowest version with no duplicate entries. */
   map_ptrloc VersionList;       // Version
   /** \brief index to the installed version */
   map_ptrloc CurrentVer;        // Version
   /** \brief indicates the deduced section

       Should be the index to the string "Unknown" or to the section
       of the last parsed item. */
   map_ptrloc Section;           // StringItem
   /** \brief index of the group this package belongs to */
   map_ptrloc Group;             // Group the Package belongs to

   // Linked list
   /** \brief Link to the next package in the same bucket */
   map_ptrloc NextPackage;       // Package
   /** \brief List of all dependencies on this package */
   map_ptrloc RevDepends;        // Dependency
   /** \brief List of all "packages" this package provide */
   map_ptrloc ProvidesList;      // Provides

   // Install/Remove/Purge etc
   /** \brief state that the user wishes the package to be in */
   unsigned char SelectedState;     // What
   /** \brief installation state of the package

       This should be "ok" but in case the installation failed
       it will be different.
   */
   unsigned char InstState;         // Flags
   /** \brief indicates if the package is installed */
   unsigned char CurrentState;      // State

   /** \brief unique sequel ID

       ID is a unique value from 0 to Header->PackageCount assigned by the generator.
       This allows clients to create an array of size PackageCount and use it to store
       state information for the package map. For instance the status file emitter uses
       this to track which packages have been emitted already. */
   unsigned int ID;
   /** \brief some useful indicators of the package's state */
   unsigned long Flags;
};
									/*}}}*/
// Package File structure						/*{{{*/
/** \brief stores information about the files used to generate the cache

    Package files are referenced by Version structures to be able to know
    after the generation still from which Packages file includes this Version
    as we need this information later on e.g. for pinning. */
struct pkgCache::PackageFile
{
   /** \brief physical disk file that this PackageFile represents */
   map_ptrloc FileName;        // StringItem
   /** \brief the release information

       Please see the files document for a description of what the
       release information means. */
   map_ptrloc Archive;         // StringItem
   map_ptrloc Codename;        // StringItem
   map_ptrloc Component;       // StringItem
   map_ptrloc Version;         // StringItem
   map_ptrloc Origin;          // StringItem
   map_ptrloc Label;           // StringItem
   map_ptrloc Architecture;    // StringItem
   /** \brief The site the index file was fetched from */
   map_ptrloc Site;            // StringItem
   /** \brief indicates what sort of index file this is

       @TODO enumerate at least the possible indexes */
   map_ptrloc IndexType;       // StringItem
   /** \brief Size of the file

       Used together with the modification time as a
       simple check to ensure that the Packages
       file has not been altered since Cache generation. */
   unsigned long Size;
   /** \brief Modification time for the file */
   time_t mtime;

   /* @TODO document PackageFile::Flags */
   unsigned long Flags;

   // Linked list
   /** \brief Link to the next PackageFile in the Cache */
   map_ptrloc NextFile;        // PackageFile
   /** \brief unique sequel ID */
   unsigned int ID;
};
									/*}}}*/
// VerFile structure							/*{{{*/
/** \brief associates a version with a PackageFile

    This allows a full description of all Versions in all files
    (and hence all sources) under consideration. */
struct pkgCache::VerFile
{
   /** \brief index of the package file that this version was found in */
   map_ptrloc File;           // PackageFile
   /** \brief next step in the linked list */
   map_ptrloc NextFile;       // PkgVerFile
   /** \brief position in the package file */
   map_ptrloc Offset;         // File offset
   /* @TODO document pkgCache::VerFile::Size */
   unsigned long Size;
};
									/*}}}*/
// DescFile structure							/*{{{*/
/** \brief associates a description with a Translation file */
struct pkgCache::DescFile
{
   /** \brief index of the file that this description was found in */
   map_ptrloc File;           // PackageFile
   /** \brief next step in the linked list */
   map_ptrloc NextFile;       // PkgVerFile
   /** \brief position in the file */
   map_ptrloc Offset;         // File offset
   /* @TODO document pkgCache::DescFile::Size */
   unsigned long Size;
};
									/*}}}*/
// Version structure							/*{{{*/
/** \brief information for a single version of a package

    The version list is always sorted from highest version to lowest
    version by the generator. Equal version numbers are either merged
    or handled as separate versions based on the Hash value. */
struct pkgCache::Version
{
   /** \brief complete version string */
   map_ptrloc VerStr;            // StringItem
   /** \brief section this version is filled in */
   map_ptrloc Section;           // StringItem

   /** \brief Multi-Arch capabilities of a package version */
   enum VerMultiArch { None = 0, /*!< is the default and doesn't trigger special behaviour */
		       All = (1<<0), /*!< will cause that Ver.Arch() will report "all" */
		       Foreign = (1<<1), /*!< can satisfy dependencies in another architecture */
		       Same = (1<<2), /*!< can be co-installed with itself from other architectures */
		       Allowed = (1<<3), /*!< other packages are allowed to depend on thispkg:any */
		       AllForeign = All | Foreign,
		       AllAllowed = All | Allowed };
   /** \brief stores the MultiArch capabilities of this version

       Flags used are defined in pkgCache::Version::VerMultiArch
   */
   unsigned char MultiArch;

   /** \brief references all the PackageFile's that this version came from

       FileList can be used to determine what distribution(s) the Version
       applies to. If FileList is 0 then this is a blank version.
       The structure should also have a 0 in all other fields excluding
       pkgCache::Version::VerStr and Possibly pkgCache::Version::NextVer. */
   map_ptrloc FileList;          // VerFile
   /** \brief next (lower or equal) version in the linked list */
   map_ptrloc NextVer;           // Version
   /** \brief next description in the linked list */
   map_ptrloc DescriptionList;   // Description
   /** \brief base of the dependency list */
   map_ptrloc DependsList;       // Dependency
   /** \brief links to the owning package

       This allows reverse dependencies to determine the package */
   map_ptrloc ParentPkg;         // Package
   /** \brief list of pkgCache::Provides */
   map_ptrloc ProvidesList;      // Provides

   /** \brief archive size for this version

       For Debian this is the size of the .deb file. */
   unsigned long long Size;      // These are the .deb size
   /** \brief uncompressed size for this version */
   unsigned long long InstalledSize;
   /** \brief characteristic value representing this version

       No two packages in existence should have the same VerStr
       and Hash with different contents. */
   unsigned short Hash;
   /** \brief unique sequel ID */
   unsigned int ID;
   /** \brief parsed priority value */
   unsigned char Priority;
};
									/*}}}*/
// Description structure						/*{{{*/
/** \brief datamember of a linked list of available description for a version */
struct pkgCache::Description
{
   /** \brief Language code of this description (translation)

       If the value has a 0 length then this is read using the Package
       file else the Translation-CODE file is used. */
   map_ptrloc language_code;     // StringItem
   /** \brief MD5sum of the original description

       Used to map Translations of a description to a version
       and to check that the Translation is up-to-date. */
   map_ptrloc md5sum;            // StringItem

   /* @TODO document pkgCache::Description::FileList */
   map_ptrloc FileList;          // DescFile
   /** \brief next translation for this description */
   map_ptrloc NextDesc;          // Description
   /** \brief the text is a description of this package */
   map_ptrloc ParentPkg;         // Package

   /** \brief unique sequel ID */
   unsigned int ID;
};
									/*}}}*/
// Dependency structure							/*{{{*/
/** \brief information for a single dependency record

    The records are split up like this to ease processing by the client.
    The base of the linked list is pkgCache::Version::DependsList.
    All forms of dependencies are recorded here including Depends,
    Recommends, Suggests, Enhances, Conflicts, Replaces and Breaks. */
struct pkgCache::Dependency
{
   /** \brief string of the version the dependency is applied against */
   map_ptrloc Version;         // StringItem
   /** \brief index of the package this depends applies to

       The generator will - if the package does not already exist -
       create a blank (no version records) package. */
   map_ptrloc Package;         // Package
   /** \brief next dependency of this version */
   map_ptrloc NextDepends;     // Dependency
   /** \brief next reverse dependency of this package */
   map_ptrloc NextRevDepends;  // Dependency
   /** \brief version of the package which has the reverse depends */
   map_ptrloc ParentVer;       // Version

   /** \brief unique sequel ID */
   map_ptrloc ID;
   /** \brief Dependency type - Depends, Recommends, Conflicts, etc */
   unsigned char Type;
   /** \brief comparison operator specified on the depends line

       If the high bit is set then it is a logical OR with the previous record. */
   unsigned char CompareOp;
};
									/*}}}*/
// Provides structure							/*{{{*/
/** \brief handles virtual packages

    When a Provides: line is encountered a new provides record is added
    associating the package with a virtual package name.
    The provides structures are linked off the package structures.
    This simplifies the analysis of dependencies and other aspects A provides
    refers to a specific version of a specific package, not all versions need to
    provide that provides.*/
struct pkgCache::Provides
{
   /** \brief index of the package providing this */
   map_ptrloc ParentPkg;        // Package
   /** \brief index of the version this provide line applies to */
   map_ptrloc Version;          // Version
   /** \brief version in the provides line (if any)

       This version allows dependencies to depend on specific versions of a
       Provides, as well as allowing Provides to override existing packages.
       This is experimental. Note that Debian doesn't allow versioned provides */
   map_ptrloc ProvideVersion;   // StringItem
   /** \brief next provides (based of package) */
   map_ptrloc NextProvides;     // Provides
   /** \brief next provides (based of version) */
   map_ptrloc NextPkgProv;      // Provides
};
									/*}}}*/
// StringItem structure							/*{{{*/
/** \brief used for generating single instances of strings

    Some things like Section Name are are useful to have as unique tags.
    It is part of a linked list based at pkgCache::Header::StringList

    All strings are simply inlined any place in the file that is natural
    for the writer. The client should make no assumptions about the positioning
    of strings. All StringItems should be null-terminated. */
struct pkgCache::StringItem
{
   /** \brief string this refers to */
   map_ptrloc String;        // StringItem
   /** \brief Next link in the chain */
   map_ptrloc NextItem;      // StringItem
};
									/*}}}*/


inline char const * const pkgCache::NativeArch() const
	{ return StrP + HeaderP->Architecture; };

#include <apt-pkg/cacheiterators.h>

inline pkgCache::GrpIterator pkgCache::GrpBegin() 
       {return GrpIterator(*this);};
inline pkgCache::GrpIterator pkgCache::GrpEnd() 
       {return GrpIterator(*this,GrpP);};
inline pkgCache::PkgIterator pkgCache::PkgBegin() 
       {return PkgIterator(*this);};
inline pkgCache::PkgIterator pkgCache::PkgEnd() 
       {return PkgIterator(*this,PkgP);};
inline pkgCache::PkgFileIterator pkgCache::FileBegin()
       {return PkgFileIterator(*this,PkgFileP + HeaderP->FileList);};
inline pkgCache::PkgFileIterator pkgCache::FileEnd()
       {return PkgFileIterator(*this,PkgFileP);};

// Oh I wish for Real Name Space Support
class pkgCache::Namespace						/*{{{*/
{   
   public:
   typedef pkgCache::GrpIterator GrpIterator;
   typedef pkgCache::PkgIterator PkgIterator;
   typedef pkgCache::VerIterator VerIterator;
   typedef pkgCache::DescIterator DescIterator;
   typedef pkgCache::DepIterator DepIterator;
   typedef pkgCache::PrvIterator PrvIterator;
   typedef pkgCache::PkgFileIterator PkgFileIterator;
   typedef pkgCache::VerFileIterator VerFileIterator;   
   typedef pkgCache::Version Version;
   typedef pkgCache::Description Description;
   typedef pkgCache::Package Package;
   typedef pkgCache::Header Header;
   typedef pkgCache::Dep Dep;
   typedef pkgCache::Flag Flag;
};
									/*}}}*/
#endif

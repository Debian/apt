// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: filelist.h,v 1.2 2001/02/20 07:03:16 jgg Exp $
/* ######################################################################

   File Listing - Manages a Cache of File -> Package names.

   This is identical to the Package cache, except that the generator 
   (which is much simpler) is integrated directly into the main class, 
   and it has been designed to handle live updates.
   
   The storage content of the class is maintained in a memory map and is
   written directly to the file system. Performance is traded against 
   space to give something that performs well and remains small.
   The average per file usage is 32 bytes which yeilds about a meg every
   36k files. Directory paths are collected into a binary tree and stored
   only once, this offsets the cost of the hash nodes enough to keep 
   memory usage slightly less than the sum of the filenames.
 
   The file names are stored into a fixed size chained hash table that is
   linked to the package name and to the directory component. 

   Each file node has a set of associated flags that indicate the current
   state of the file.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_FILELIST_H
#define PKGLIB_FILELIST_H


#include <cstring>
#include <apt-pkg/mmap.h>

class pkgFLCache
{
   public:
   struct Header;
   struct Node;
   struct Directory;
   struct Package;
   struct Diversion;
   struct ConfFile;
   
   class NodeIterator;
   class DirIterator;
   class PkgIterator;
   class DiverIterator;
   
   protected:
   string CacheFile;
   DynamicMMap &Map;
   map_ptrloc LastTreeLookup;
   unsigned long LastLookupSize;
   
   // Helpers for the addition algorithms
   map_ptrloc TreeLookup(map_ptrloc *Base,const char *Text,const char *TextEnd,
			 unsigned long Size,unsigned int *Count = 0,
			 bool Insert = false);
   
   public:
   
   // Pointers to the arrays of items
   Header *HeaderP;
   Node *NodeP;
   Directory *DirP;
   Package *PkgP;
   Diversion *DiverP;
   ConfFile *ConfP;
   char *StrP;
   unsigned char *AnyP;
   
   // Quick accessors
   Node *FileHash;
   
   // Accessors
   Header &Head() {return *HeaderP;};
   void PrintTree(map_ptrloc Base,unsigned long Size);

   // Add/Find things
   PkgIterator GetPkg(const char *Name,const char *End,bool Insert);
   inline PkgIterator GetPkg(const char *Name,bool Insert);
   NodeIterator GetNode(const char *Name,
			const char *NameEnd,
			map_ptrloc Loc,
			bool Insert,bool Divert);
   Node *HashNode(NodeIterator const &N);
   void DropNode(map_ptrloc Node);

   inline DiverIterator DiverBegin();
   
   // Diversion control
   void BeginDiverLoad();
   void FinishDiverLoad();
   bool AddDiversion(PkgIterator const &Owner,const char *From,
		     const char *To);
   bool AddConfFile(const char *Name,const char *NameEnd,
		    PkgIterator const &Owner,const unsigned char *Sum);
			     
   pkgFLCache(DynamicMMap &Map);
//   ~pkgFLCache();
};

struct pkgFLCache::Header
{
   // Signature information
   unsigned long Signature;
   short MajorVersion;
   short MinorVersion;
   bool Dirty;
   
   // Size of structure values
   unsigned HeaderSz;
   unsigned NodeSz;
   unsigned DirSz;
   unsigned PackageSz;
   unsigned DiversionSz;
   unsigned ConfFileSz;
   
   // Structure Counts;
   unsigned int NodeCount;
   unsigned int DirCount;
   unsigned int PackageCount;
   unsigned int DiversionCount;
   unsigned int ConfFileCount;
   unsigned int HashSize;
   unsigned long UniqNodes;
      
   // Offsets
   map_ptrloc FileHash;
   map_ptrloc DirTree;
   map_ptrloc Packages;
   map_ptrloc Diversions;
      
   /* Allocation pools, there should be one of these for each structure
      excluding the header */
   DynamicMMap::Pool Pools[5];

   bool CheckSizes(Header &Against) const;
   Header();
};

/* The bit field is used to advoid incurring an extra 4 bytes x 40000,
   Pointer is the most infrequently used member of the structure */
struct pkgFLCache::Node
{
   map_ptrloc Dir;            // Dir
   map_ptrloc File;           // String
   unsigned Pointer:24;       // Package/Diversion/ConfFile
   unsigned Flags:8;          // Package
   map_ptrloc Next;           // Node
   map_ptrloc NextPkg;        // Node

   enum Flags {Diversion = (1<<0),ConfFile = (1<<1),
               NewConfFile = (1<<2),NewFile = (1<<3),
               Unpacked = (1<<4),Replaced = (1<<5)};
};

struct pkgFLCache::Directory
{
   map_ptrloc Left;           // Directory
   map_ptrloc Right;          // Directory
   map_ptrloc Name;           // String
};

struct pkgFLCache::Package
{
   map_ptrloc Left;           // Package
   map_ptrloc Right;          // Package
   map_ptrloc Name;           // String
   map_ptrloc Files;          // Node
};

struct pkgFLCache::Diversion
{
   map_ptrloc OwnerPkg;       // Package
   map_ptrloc DivertFrom;     // Node
   map_ptrloc DivertTo;       // String
   
   map_ptrloc Next;           // Diversion
   unsigned long Flags;

   enum Flags {Touched = (1<<0)};
};

struct pkgFLCache::ConfFile
{
   map_ptrloc OwnerPkg;       // Package
   unsigned char MD5[16];
};

class pkgFLCache::PkgIterator
{
   Package *Pkg;
   pkgFLCache *Owner;
   
   public:
   
   inline bool end() const {return Owner == 0 || Pkg == Owner->PkgP?true:false;}
   
   // Accessors
   inline Package *operator ->() {return Pkg;};
   inline Package const *operator ->() const {return Pkg;};
   inline Package const &operator *() const {return *Pkg;};
   inline operator Package *() {return Pkg == Owner->PkgP?0:Pkg;};
   inline operator Package const *() const {return Pkg == Owner->PkgP?0:Pkg;};

   inline unsigned long Offset() const {return Pkg - Owner->PkgP;};
   inline const char *Name() const {return Pkg->Name == 0?0:Owner->StrP + Pkg->Name;};
   inline pkgFLCache::NodeIterator Files() const;

   PkgIterator() : Pkg(0), Owner(0) {};
   PkgIterator(pkgFLCache &Owner,Package *Trg) : Pkg(Trg), Owner(&Owner) {};
};

class pkgFLCache::DirIterator
{
   Directory *Dir;
   pkgFLCache *Owner;
   
   public:
   
   // Accessors
   inline Directory *operator ->() {return Dir;};
   inline Directory const *operator ->() const {return Dir;};
   inline Directory const &operator *() const {return *Dir;};
   inline operator Directory *() {return Dir == Owner->DirP?0:Dir;};
   inline operator Directory const *() const {return Dir == Owner->DirP?0:Dir;};

   inline const char *Name() const {return Dir->Name == 0?0:Owner->StrP + Dir->Name;};

   DirIterator() : Dir(0), Owner(0) {};
   DirIterator(pkgFLCache &Owner,Directory *Trg) : Dir(Trg), Owner(&Owner) {};
};

class pkgFLCache::DiverIterator
{
   Diversion *Diver;
   pkgFLCache *Owner;
   
   public:

   // Iteration
   void operator ++(int) {if (Diver != Owner->DiverP) Diver = Owner->DiverP + Diver->Next;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == 0 || Diver == Owner->DiverP;};

   // Accessors
   inline Diversion *operator ->() {return Diver;};
   inline Diversion const *operator ->() const {return Diver;};
   inline Diversion const &operator *() const {return *Diver;};
   inline operator Diversion *() {return Diver == Owner->DiverP?0:Diver;};
   inline operator Diversion const *() const {return Diver == Owner->DiverP?0:Diver;};

   inline PkgIterator OwnerPkg() const {return PkgIterator(*Owner,Owner->PkgP + Diver->OwnerPkg);};
   inline NodeIterator DivertFrom() const;
   inline NodeIterator DivertTo() const;

   DiverIterator() : Diver(0), Owner(0) {};
   DiverIterator(pkgFLCache &Owner,Diversion *Trg) : Diver(Trg), Owner(&Owner) {};
};

class pkgFLCache::NodeIterator
{
   Node *Nde;
   enum {NdePkg, NdeHash} Type;   
   pkgFLCache *Owner;
   
   public:
   
   // Iteration
   void operator ++(int) {if (Nde != Owner->NodeP) Nde = Owner->NodeP + 
	 (Type == NdePkg?Nde->NextPkg:Nde->Next);};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == 0 || Nde == Owner->NodeP;};

   // Accessors
   inline Node *operator ->() {return Nde;};
   inline Node const *operator ->() const {return Nde;};
   inline Node const &operator *() const {return *Nde;};
   inline operator Node *() {return Nde == Owner->NodeP?0:Nde;};
   inline operator Node const *() const {return Nde == Owner->NodeP?0:Nde;};
   inline unsigned long Offset() const {return Nde - Owner->NodeP;};
   inline DirIterator Dir() const {return DirIterator(*Owner,Owner->DirP + Nde->Dir);};
   inline DiverIterator Diversion() const {return DiverIterator(*Owner,Owner->DiverP + Nde->Pointer);};
   inline const char *File() const {return Nde->File == 0?0:Owner->StrP + Nde->File;};
   inline const char *DirN() const {return Owner->StrP + Owner->DirP[Nde->Dir].Name;};
   Package *RealPackage() const;
   
   NodeIterator() : Nde(0), Type(NdeHash), Owner(0) {};
   NodeIterator(pkgFLCache &Owner) : Nde(Owner.NodeP), Type(NdeHash), Owner(&Owner) {};
   NodeIterator(pkgFLCache &Owner,Node *Trg) : Nde(Trg), Type(NdeHash), Owner(&Owner) {};
   NodeIterator(pkgFLCache &Owner,Node *Trg,Package *) : Nde(Trg), Type(NdePkg), Owner(&Owner) {};
};

/* Inlines with forward references that cannot be included directly in their
   respsective classes */
inline pkgFLCache::NodeIterator pkgFLCache::DiverIterator::DivertFrom() const 
   {return NodeIterator(*Owner,Owner->NodeP + Diver->DivertFrom);};
inline pkgFLCache::NodeIterator pkgFLCache::DiverIterator::DivertTo() const
   {return NodeIterator(*Owner,Owner->NodeP + Diver->DivertTo);};

inline pkgFLCache::NodeIterator pkgFLCache::PkgIterator::Files() const
   {return NodeIterator(*Owner,Owner->NodeP + Pkg->Files,Pkg);};

inline pkgFLCache::DiverIterator pkgFLCache::DiverBegin()
   {return DiverIterator(*this,DiverP + HeaderP->Diversions);};

inline pkgFLCache::PkgIterator pkgFLCache::GetPkg(const char *Name,bool Insert) 
   {return GetPkg(Name,Name+strlen(Name),Insert);};

#endif

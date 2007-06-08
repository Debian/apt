// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: filelist.cc,v 1.4.2.1 2004/01/16 18:58:50 mdz Exp $
/* ######################################################################

   File Listing - Manages a Cache of File -> Package names.

   Diversions add some signficant complexity to the system. To keep 
   storage space down in the very special case of a diverted file no
   extra bytes are allocated in the Node structure. Instead a diversion
   is inserted directly into the hash table and its flag bit set. Every
   lookup for that filename will always return the diversion.
   
   The hash buckets are stored in sorted form, with diversions having 
   the higest sort order. Identical files are assigned the same file
   pointer, thus after a search all of the nodes owning that file can be
   found by iterating down the bucket.
   
   Re-updates of diversions (another extremely special case) are done by
   marking all diversions as untouched, then loading the entire diversion
   list again, touching each diversion and then finally going back and
   releasing all untouched diversions. It is assumed that the diversion
   table will always be quite small and be a very irregular case.

   Diversions that are user-installed are represented by a package with
   an empty name string.

   Conf files are handled like diversions by changing the meaning of the
   Pointer field to point to a conf file entry - again to reduce over
   head for a special case.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/filelist.h>
#include <apt-pkg/mmap.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <apti18n.h>
									/*}}}*/

using namespace std;

// FlCache::Header::Header - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Initialize the header variables. These are the defaults used when
   creating new caches */
pkgFLCache::Header::Header()
{
   Signature = 0xEA3F1295;
   
   /* Whenever the structures change the major version should be bumped,
    whenever the generator changes the minor version should be bumped. */
   MajorVersion = 1;
   MinorVersion = 0;
   Dirty = true;
   
   HeaderSz = sizeof(pkgFLCache::Header);
   NodeSz = sizeof(pkgFLCache::Node);
   DirSz = sizeof(pkgFLCache::Directory);
   PackageSz = sizeof(pkgFLCache::Package);
   DiversionSz = sizeof(pkgFLCache::Diversion);
   ConfFileSz = sizeof(pkgFLCache::ConfFile);
      
   NodeCount = 0;
   DirCount = 0;
   PackageCount = 0;
   DiversionCount = 0;
   ConfFileCount = 0;
   HashSize = 1 << 14;

   FileHash = 0;
   DirTree = 0;
   Packages = 0;
   Diversions = 0;
   UniqNodes = 0;
   memset(Pools,0,sizeof(Pools));
}
									/*}}}*/
// FLCache::Header::CheckSizes - Check if the two headers have same *sz	/*{{{*/
// ---------------------------------------------------------------------
/* Compare to make sure we are matching versions */
bool pkgFLCache::Header::CheckSizes(Header &Against) const
{
   if (HeaderSz == Against.HeaderSz &&
       NodeSz == Against.NodeSz &&
       DirSz == Against.DirSz &&
       DiversionSz == Against.DiversionSz &&
       PackageSz == Against.PackageSz &&
       ConfFileSz == Against.ConfFileSz)
            return true;
      return false;
}
									/*}}}*/

// FLCache::pkgFLCache - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* If this is a new cache then a new header and hash table are instantaited
   otherwise the existing ones are mearly attached */
pkgFLCache::pkgFLCache(DynamicMMap &Map) : Map(Map)
{
   if (_error->PendingError() == true)
      return;

   LastTreeLookup = 0;
   LastLookupSize = 0;
   
   // Apply the typecasts
   HeaderP = (Header *)Map.Data();
   NodeP = (Node *)Map.Data();
   DirP = (Directory *)Map.Data();
   DiverP = (Diversion *)Map.Data();
   PkgP = (Package *)Map.Data();
   ConfP = (ConfFile *)Map.Data();
   StrP = (char *)Map.Data();
   AnyP = (unsigned char *)Map.Data();
   
   // New mapping, create the basic cache structures
   if (Map.Size() == 0)
   {
      Map.RawAllocate(sizeof(pkgFLCache::Header));
      *HeaderP = pkgFLCache::Header();
      HeaderP->FileHash = Map.RawAllocate(sizeof(pkgFLCache::Node)*HeaderP->HashSize,
					  sizeof(pkgFLCache::Node))/sizeof(pkgFLCache::Node);
   }

   FileHash = NodeP + HeaderP->FileHash;
   
   // Setup the dynamic map manager
   HeaderP->Dirty = true;
   Map.Sync(0,sizeof(pkgFLCache::Header));
   Map.UsePools(*HeaderP->Pools,sizeof(HeaderP->Pools)/sizeof(HeaderP->Pools[0]));
}
									/*}}}*/
// FLCache::TreeLookup - Perform a lookup in a generic tree		/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple generic tree lookup. The first three entries of
   the Directory structure are used as a template, but any other similar
   structure could be used in it's place. */
map_ptrloc pkgFLCache::TreeLookup(map_ptrloc *Base,const char *Text,
				  const char *TextEnd,unsigned long Size,
				  unsigned int *Count,bool Insert)
{       
   pkgFLCache::Directory *Dir;
   
   // Check our last entry cache
   if (LastTreeLookup != 0 && LastLookupSize == Size)
   {
      Dir = (pkgFLCache::Directory *)(AnyP + LastTreeLookup*Size);
      if (stringcmp(Text,TextEnd,StrP + Dir->Name) == 0)
	 return LastTreeLookup; 
   }   
   
   while (1)
   {
      // Allocate a new one
      if (*Base == 0)
      {
	 if (Insert == false)
	    return 0;
	 
	 *Base = Map.Allocate(Size);
	 if (*Base == 0)
	    return 0;
	 
	 (*Count)++;
	 Dir = (pkgFLCache::Directory *)(AnyP + *Base*Size);
	 Dir->Name = Map.WriteString(Text,TextEnd - Text);
	 LastTreeLookup = *Base;
	 LastLookupSize = Size;
	 return *Base;
      }
      
      // Compare this node
      Dir = (pkgFLCache::Directory *)(AnyP + *Base*Size);
      int Res = stringcmp(Text,TextEnd,StrP + Dir->Name);
      if (Res == 0)
      {
	 LastTreeLookup = *Base;
	 LastLookupSize = Size;
	 return *Base;
      }
      
      if (Res > 0)
	 Base = &Dir->Left;
      if (Res < 0)
	 Base = &Dir->Right;
   }
}
									/*}}}*/
// FLCache::PrintTree - Print out a tree				/*{{{*/
// ---------------------------------------------------------------------
/* This is a simple generic tree dumper, ment for debugging. */
void pkgFLCache::PrintTree(map_ptrloc Base,unsigned long Size)
{
   if (Base == 0)
      return;
   
   pkgFLCache::Directory *Dir = (pkgFLCache::Directory *)(AnyP + Base*Size);
   PrintTree(Dir->Left,Size);
   cout << (StrP + Dir->Name) << endl;
   PrintTree(Dir->Right,Size);
}
									/*}}}*/
// FLCache::GetPkg - Get a package pointer				/*{{{*/
// ---------------------------------------------------------------------
/* Locate a package by name in it's tree, this is just a wrapper for
   TreeLookup */
pkgFLCache::PkgIterator pkgFLCache::GetPkg(const char *Name,const char *NameEnd,
					   bool Insert)
{
   if (NameEnd == 0)
      NameEnd = Name + strlen(Name);
   
   map_ptrloc Pos = TreeLookup(&HeaderP->Packages,Name,NameEnd,
			       sizeof(pkgFLCache::Package),
			       &HeaderP->PackageCount,Insert);
   if (Pos == 0)
      return pkgFLCache::PkgIterator();
   return pkgFLCache::PkgIterator(*this,PkgP + Pos);
}
									/*}}}*/
// FLCache::GetNode - Get the node associated with the filename		/*{{{*/
// ---------------------------------------------------------------------
/* Lookup a node in the hash table. If Insert is true then a new node is
   always inserted. The hash table can have multiple instances of a
   single name available. A search returns the first. It is important
   that additions for the same name insert after the first entry of
   the name group. */
pkgFLCache::NodeIterator pkgFLCache::GetNode(const char *Name,
					     const char *NameEnd,
					     map_ptrloc Loc,
					     bool Insert,bool Divert)
{
   // Split the name into file and directory, hashing as it is copied 
   const char *File = Name;
   unsigned long HashPos = 0;
   for (const char *I = Name; I < NameEnd; I++)
   {
      HashPos = 1637*HashPos + *I;
      if (*I == '/')
	 File = I;
   }
   
   // Search for it
   Node *Hash = NodeP + HeaderP->FileHash + (HashPos % HeaderP->HashSize);
   int Res = 0;
   map_ptrloc FilePtr = 0;
   while (Hash->Pointer != 0)
   {
      // Compare
      Res = stringcmp(File+1,NameEnd,StrP + Hash->File);
      if (Res == 0)
	 Res = stringcmp(Name,File,StrP + DirP[Hash->Dir].Name);
      
      // Diversion?
      if (Res == 0 && Insert == true)
      {
	 /* Dir and File match exactly, we need to reuse the file name
	    when we link it in */
	 FilePtr = Hash->File;
	 Res = Divert - ((Hash->Flags & Node::Diversion) == Node::Diversion);
      }
      
      // Is a match
      if (Res == 0)
      {
	 if (Insert == false)
	    return NodeIterator(*this,Hash);
	 
	 // Only one diversion per name!
	 if (Divert == true)
	    return NodeIterator(*this,Hash);
	 break;
      }
            
      // Out of sort order
      if (Res > 0)
	 break;
      
      if (Hash->Next != 0)
	 Hash = NodeP + Hash->Next;
      else
	 break;
   }   
   
   // Fail, not found
   if (Insert == false)
      return NodeIterator(*this);

   // Find a directory node
   map_ptrloc Dir = TreeLookup(&HeaderP->DirTree,Name,File,
			       sizeof(pkgFLCache::Directory),
			       &HeaderP->DirCount,true);
   if (Dir == 0)
      return NodeIterator(*this);

   // Allocate a new node
   if (Hash->Pointer != 0)
   {
      // Overwrite or append
      if (Res > 0)
      {
	 Node *Next = NodeP + Map.Allocate(sizeof(*Hash));
	 if (Next == NodeP)
	    return NodeIterator(*this);
	 *Next = *Hash;
	 Hash->Next = Next - NodeP;
      }
      else
      {
	 unsigned long NewNext = Map.Allocate(sizeof(*Hash));
	 if (NewNext == 0)
	    return NodeIterator(*this);
	 NodeP[NewNext].Next = Hash->Next;
	 Hash->Next = NewNext;
	 Hash = NodeP + Hash->Next;
      }      
   }      
   
   // Insert into the new item
   Hash->Dir = Dir;
   Hash->Pointer = Loc;
   Hash->Flags = 0;
   if (Divert == true)
      Hash->Flags |= Node::Diversion;
   
   if (FilePtr != 0)
      Hash->File = FilePtr;
   else
   {
      HeaderP->UniqNodes++;
      Hash->File = Map.WriteString(File+1,NameEnd - File-1);
   }
   
   // Link the node to the package list
   if (Divert == false && Loc == 0)
   {
      Hash->Next = PkgP[Loc].Files;
      PkgP[Loc].Files = Hash - NodeP;
   }
   
   HeaderP->NodeCount++;
   return NodeIterator(*this,Hash);
}
									/*}}}*/
// FLCache::HashNode - Return the hash bucket for the node		/*{{{*/
// ---------------------------------------------------------------------
/* This is one of two hashing functions. The other is inlined into the
   GetNode routine. */
pkgFLCache::Node *pkgFLCache::HashNode(NodeIterator const &Nde)
{
   // Hash the node
   unsigned long HashPos = 0;
   for (const char *I = Nde.DirN(); *I != 0; I++)
      HashPos = 1637*HashPos + *I;
   HashPos = 1637*HashPos + '/';
   for (const char *I = Nde.File(); *I != 0; I++)
      HashPos = 1637*HashPos + *I;
   return NodeP + HeaderP->FileHash + (HashPos % HeaderP->HashSize);
}
									/*}}}*/
// FLCache::DropNode - Drop a node from the hash table			/*{{{*/
// ---------------------------------------------------------------------
/* This erases a node from the hash table. Note that this does not unlink
   the node from the package linked list. */
void pkgFLCache::DropNode(map_ptrloc N)
{
   if (N == 0)
      return;
   
   NodeIterator Nde(*this,NodeP + N);
   
   if (Nde->NextPkg != 0)
      _error->Warning(_("DropNode called on still linked node"));
   
   // Locate it in the hash table
   Node *Last = 0;
   Node *Hash = HashNode(Nde);
   while (Hash->Pointer != 0)
   {
      // Got it
      if (Hash == Nde)
      {
	 // Top of the bucket..
	 if (Last == 0)
	 {
	    Hash->Pointer = 0;
	    if (Hash->Next == 0)
	       return;
	    *Hash = NodeP[Hash->Next];
	    // Release Hash->Next
	    return;
	 }
	 Last->Next = Hash->Next;
	 // Release Hash
	 return;
      }
      
      Last = Hash;
      if (Hash->Next != 0)
	 Hash = NodeP + Hash->Next;
      else
	 break;
   }   
 
   _error->Error(_("Failed to locate the hash element!"));
}
									/*}}}*/
// FLCache::BeginDiverLoad - Start reading new diversions		/*{{{*/
// ---------------------------------------------------------------------
/* Tag all the diversions as untouched */
void pkgFLCache::BeginDiverLoad()
{
   for (DiverIterator I = DiverBegin(); I.end() == false; I++)
      I->Flags = 0;
}
									/*}}}*/
// FLCache::FinishDiverLoad - Finish up a new diversion load		/*{{{*/
// ---------------------------------------------------------------------
/* This drops any untouched diversions. In effect removing any diversions
   that where not loaded (ie missing from the diversion file) */
void pkgFLCache::FinishDiverLoad()
{
   map_ptrloc *Cur = &HeaderP->Diversions;
   while (*Cur != 0) 
   {
      Diversion *Div = DiverP + *Cur;
      if ((Div->Flags & Diversion::Touched) == Diversion::Touched)
      {
	 Cur = &Div->Next;
	 continue;
      }
   
      // Purge!
      DropNode(Div->DivertTo);
      DropNode(Div->DivertFrom);
      *Cur = Div->Next;
   }
}
									/*}}}*/
// FLCache::AddDiversion - Add a new diversion				/*{{{*/
// ---------------------------------------------------------------------
/* Add a new diversion to the diverion tables and make sure that it is
   unique and non-chaining. */
bool pkgFLCache::AddDiversion(PkgIterator const &Owner,
			      const char *From,const char *To)
{   
   /* Locate the two hash nodes we are going to manipulate. If there
      are pre-existing diversions then they will be returned */
   NodeIterator FromN = GetNode(From,From+strlen(From),0,true,true);
   NodeIterator ToN = GetNode(To,To+strlen(To),0,true,true);
   if (FromN.end() == true || ToN.end() == true)
      return _error->Error(_("Failed to allocate diversion"));

   // Should never happen
   if ((FromN->Flags & Node::Diversion) != Node::Diversion ||
       (ToN->Flags & Node::Diversion) != Node::Diversion)
      return _error->Error(_("Internal error in AddDiversion"));

   // Now, try to reclaim an existing diversion..
   map_ptrloc Diver = 0;
   if (FromN->Pointer != 0)
      Diver = FromN->Pointer;
  
   /* Make sure from and to point to the same diversion, if they dont
      then we are trying to intermix diversions - very bad */
   if (ToN->Pointer != 0 && ToN->Pointer != Diver)
   {
      // It could be that the other diversion is no longer in use
      if ((DiverP[ToN->Pointer].Flags & Diversion::Touched) == Diversion::Touched)	 
	 return _error->Error(_("Trying to overwrite a diversion, %s -> %s and %s/%s"),
			      From,To,ToN.File(),ToN.Dir().Name());
      
      // We can erase it.
      Diversion *Div = DiverP + ToN->Pointer;
      ToN->Pointer = 0;
      
      if (Div->DivertTo == ToN.Offset())
	 Div->DivertTo = 0;
      if (Div->DivertFrom == ToN.Offset())
	 Div->DivertFrom = 0;
      
      // This diversion will be cleaned up by FinishDiverLoad
   }
   
   // Allocate a new diversion
   if (Diver == 0)
   {
      Diver = Map.Allocate(sizeof(Diversion));
      if (Diver == 0)
	 return false;
      DiverP[Diver].Next = HeaderP->Diversions;
      HeaderP->Diversions = Diver;
      HeaderP->DiversionCount++;
   }

   // Can only have one diversion of the same files
   Diversion *Div = DiverP + Diver;
   if ((Div->Flags & Diversion::Touched) == Diversion::Touched)
      return _error->Error(_("Double add of diversion %s -> %s"),From,To);
   
   // Setup the From/To links
   if (Div->DivertFrom != FromN.Offset() && Div->DivertFrom != ToN.Offset())
      DropNode(Div->DivertFrom);
   Div->DivertFrom = FromN.Offset();
   if (Div->DivertTo != FromN.Offset() && Div->DivertTo != ToN.Offset())
      DropNode(Div->DivertTo);
   Div->DivertTo = ToN.Offset();
   
   // Link it to the two nodes
   FromN->Pointer = Diver;
   ToN->Pointer = Diver;
   
   // And the package
   Div->OwnerPkg = Owner.Offset();
   Div->Flags |= Diversion::Touched;
   
   return true;
}
									/*}}}*/
// FLCache::AddConfFile - Add a new configuration file			/*{{{*/
// ---------------------------------------------------------------------
/* This simply adds a new conf file node to the hash table. This is only
   used by the status file reader. It associates a hash with each conf
   file entry that exists in the status file and the list file for 
   the proper package. Duplicate conf files (across packages) are left
   up to other routines to deal with. */
bool pkgFLCache::AddConfFile(const char *Name,const char *NameEnd,
			     PkgIterator const &Owner,
			     const unsigned char *Sum)
{
   NodeIterator Nde = GetNode(Name,NameEnd,0,false,false);
   if (Nde.end() == true)
      return true;
   
   unsigned long File = Nde->File;
   for (; Nde->File == File && Nde.end() == false; Nde++)
   {
      if (Nde.RealPackage() != Owner)
	 continue;

      if ((Nde->Flags & Node::ConfFile) == Node::ConfFile)
	 return _error->Error(_("Duplicate conf file %s/%s"),Nde.DirN(),Nde.File());
			      
      // Allocate a new conf file structure
      map_ptrloc Conf = Map.Allocate(sizeof(ConfFile));
      if (Conf == 0)
	 return false;
      ConfP[Conf].OwnerPkg = Owner.Offset();
      memcpy(ConfP[Conf].MD5,Sum,sizeof(ConfP[Conf].MD5));
      
      Nde->Pointer = Conf;
      Nde->Flags |= Node::ConfFile;
      return true;
   }
      
   /* This means the conf file has been replaced, but the entry in the 
      status file was not updated */
   return true;
}
									/*}}}*/

// NodeIterator::RealPackage - Return the package for this node		/*{{{*/
// ---------------------------------------------------------------------
/* Since the package pointer is indirected in all sorts of interesting ways
   this is used to get a pointer to the owning package */
pkgFLCache::Package *pkgFLCache::NodeIterator::RealPackage() const
{
   if (Nde->Pointer == 0)
      return 0;
   
   if ((Nde->Flags & Node::ConfFile) == Node::ConfFile)
      return Owner->PkgP + Owner->ConfP[Nde->Pointer].OwnerPkg;

   // Diversions are ignored
   if ((Nde->Flags & Node::Diversion) == Node::Diversion)
      return 0;
   
   return Owner->PkgP + Nde->Pointer;
}
									/*}}}*/

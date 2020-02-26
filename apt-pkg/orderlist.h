// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Order List - Represents and Manipulates an ordered list of packages.
   
   A list of packages can be ordered by a number of conflicting criteria
   each given a specific priority. Each package also has a set of flags
   indicating some useful things about it that are derived in the 
   course of sorting. The pkgPackageManager class uses this class for
   all of it's installation ordering needs.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ORDERLIST_H
#define PKGLIB_ORDERLIST_H

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <string>

class pkgDepCache;
class APT_PUBLIC pkgOrderList : protected pkgCache::Namespace
{
   void * const d;
   protected:

   pkgDepCache &Cache;   
   typedef bool (pkgOrderList::*DepFunc)(DepIterator D);

   // These are the currently selected ordering functions
   DepFunc Primary;
   DepFunc Secondary;
   DepFunc RevDepends;
   DepFunc Remove;

   // State
   Package **End;
   Package **List;
   Package **AfterEnd;
   std::string *FileList;
   DepIterator Loops[20];
   int LoopCount;
   int Depth;
   unsigned short *Flags;
   bool Debug;
   
   // Main visit function
   bool VisitNode(PkgIterator Pkg, char const* from);
   bool VisitDeps(DepFunc F,PkgIterator Pkg);
   bool VisitRDeps(DepFunc F,PkgIterator Pkg);
   bool VisitRProvides(DepFunc F,VerIterator Ver);
   bool VisitProvides(DepIterator Pkg,bool Critical);
   
   // Dependency checking functions.
   bool DepUnPackCrit(DepIterator D);
   bool DepUnPackPreD(DepIterator D);
   bool DepUnPackPre(DepIterator D);
   bool DepUnPackDep(DepIterator D);
   bool DepConfigure(DepIterator D);
   bool DepRemove(DepIterator D);
   
   // Analysis helpers
   bool AddLoop(DepIterator D);
   bool CheckDep(DepIterator D);
   bool DoRun();
   
   // For pre sorting
   int OrderCompareA(Package *a, Package *b) APT_PURE;
   int OrderCompareB(Package *a, Package *b) APT_PURE;
   int FileCmp(PkgIterator A,PkgIterator B) APT_PURE;
   
   public:

   typedef Package **iterator;
   
   /* State flags
      The Loop flag can be set on a package that is currently being processed by either SmartConfigure or
      SmartUnPack. This allows the package manager to tell when a loop has been formed as it will try to 
      SmartUnPack or SmartConfigure a package with the Loop flag set. It will then either stop (as it knows
      that the operation is unnecessary as its already in process), or in the case of the conflicts resolution
      in SmartUnPack, use EarlyRemove to resolve the situation.  */
   enum Flags {Added = (1 << 0), AddPending = (1 << 1),
               Immediate = (1 << 2), Loop = (1 << 3),
               UnPacked = (1 << 4), Configured = (1 << 5),
               Removed = (1 << 6),        // Early Remove
               InList = (1 << 7),
               After = (1 << 8),
               States = (UnPacked | Configured | Removed)};

   // Flag manipulators
   inline bool IsFlag(PkgIterator Pkg,unsigned long F) {return (Flags[Pkg->ID] & F) == F;};
   inline bool IsFlag(Package *Pkg,unsigned long F) {return (Flags[Pkg->ID] & F) == F;};
   void Flag(PkgIterator Pkg,unsigned long State, unsigned long F) {Flags[Pkg->ID] = (Flags[Pkg->ID] & (~F)) | State;};
   inline void Flag(PkgIterator Pkg,unsigned long F) {Flags[Pkg->ID] |= F;};
   inline void Flag(Package *Pkg,unsigned long F) {Flags[Pkg->ID] |= F;};
   // RmFlag removes a flag from a package 
   inline void RmFlag(Package *Pkg,unsigned long F) {Flags[Pkg->ID] &= ~F;};
   // IsNow will return true if the Pkg has been not been either configured or unpacked
   inline bool IsNow(PkgIterator Pkg) {return (Flags[Pkg->ID] & (States & (~Removed))) == 0;};
   bool IsMissing(PkgIterator Pkg) APT_PURE;
   void WipeFlags(unsigned long F);
   void SetFileList(std::string *FileList) {this->FileList = FileList;};

   // Accessors
   inline iterator begin() {return List;};
   inline iterator end() {return End;};
   inline void push_back(Package *Pkg) {*(End++) = Pkg;};
   inline void push_back(PkgIterator Pkg) {*(End++) = Pkg;};
   inline void pop_back() {End--;};
   inline bool empty() {return End == List;};
   inline unsigned int size() {return End - List;};
   
   // Ordering modes
   bool OrderCritical();
   bool OrderUnpack(std::string *FileList = 0);
   bool OrderConfigure();

   int Score(PkgIterator Pkg);

   explicit pkgOrderList(pkgDepCache *Cache);
   virtual ~pkgOrderList();
};

#endif

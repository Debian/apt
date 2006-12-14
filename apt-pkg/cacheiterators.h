// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: cacheiterators.h,v 1.18.2.1 2004/05/08 22:44:27 mdz Exp $
/* ######################################################################
   
   Cache Iterators - Iterators for navigating the cache structure
   
   The iterators all provides ++,==,!=,->,* and end for their type.
   The end function can be used to tell if the list has been fully
   traversed.
   
   Unlike STL iterators these contain helper functions to access the data
   that is being iterated over. This is because the data structures can't
   be formed in a manner that is intuitive to use and also mmapable.
   
   For each variable in the target structure that would need a translation
   to be accessed correctly a translating function of the same name is
   present in the iterator. If applicable the translating function will
   return an iterator.

   The DepIterator can iterate over two lists, a list of 'version depends'
   or a list of 'package reverse depends'. The type is determined by the
   structure passed to the constructor, which should be the structure
   that has the depends pointer as a member. The provide iterator has the
   same system.
   
   This header is not user includable, please use apt-pkg/pkgcache.h
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CACHEITERATORS_H
#define PKGLIB_CACHEITERATORS_H


// Package Iterator
class pkgCache::PkgIterator
{
   friend class pkgCache;
   Package *Pkg;
   pkgCache *Owner;
   long HashIndex;

   protected:
   
   // This constructor is the 'begin' constructor, never use it.
   inline PkgIterator(pkgCache &Owner) : Owner(&Owner), HashIndex(-1)
   {
      Pkg = Owner.PkgP;
      operator ++(0);
   };
   
   public:

   enum OkState {NeedsNothing,NeedsUnpack,NeedsConfigure};
      
   // Iteration
   void operator ++(int);
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == 0 || Pkg == Owner->PkgP?true:false;};

   // Comparison
   inline bool operator ==(const PkgIterator &B) const {return Pkg == B.Pkg;};
   inline bool operator !=(const PkgIterator &B) const {return Pkg != B.Pkg;};
			   
   // Accessors
   inline Package *operator ->() {return Pkg;};
   inline Package const *operator ->() const {return Pkg;};
   inline Package const &operator *() const {return *Pkg;};
   inline operator Package *() {return Pkg == Owner->PkgP?0:Pkg;};
   inline operator Package const *() const {return Pkg == Owner->PkgP?0:Pkg;};
   inline pkgCache *Cache() {return Owner;};
   
   inline const char *Name() const {return Pkg->Name == 0?0:Owner->StrP + Pkg->Name;};
   inline const char *Section() const {return Pkg->Section == 0?0:Owner->StrP + Pkg->Section;};
   inline bool Purge() const {return Pkg->CurrentState == pkgCache::State::Purge ||
	 (Pkg->CurrentVer == 0 && Pkg->CurrentState == pkgCache::State::NotInstalled);};
   inline VerIterator VersionList() const;
   inline VerIterator CurrentVer() const;
   inline DepIterator RevDependsList() const;
   inline PrvIterator ProvidesList() const;
   inline unsigned long Index() const {return Pkg - Owner->PkgP;};
   OkState State() const;
   
   // Constructors
   inline PkgIterator(pkgCache &Owner,Package *Trg) : Pkg(Trg), Owner(&Owner),
          HashIndex(0) 
   { 
      if (Pkg == 0)
	 Pkg = Owner.PkgP;
   };
   inline PkgIterator() : Pkg(0), Owner(0), HashIndex(0) {};
};

// Version Iterator
class pkgCache::VerIterator
{
   Version *Ver;
   pkgCache *Owner;

   void _dummy();
   
   public:

   // Iteration
   void operator ++(int) {if (Ver != Owner->VerP) Ver = Owner->VerP + Ver->NextVer;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == NULL || (Ver == Owner->VerP?true:false);};
   inline void operator =(const VerIterator &B) {Ver = B.Ver; Owner = B.Owner;};
   
   // Comparison
   inline bool operator ==(const VerIterator &B) const {return Ver == B.Ver;};
   inline bool operator !=(const VerIterator &B) const {return Ver != B.Ver;};
   int CompareVer(const VerIterator &B) const;
   
   // Accessors
   inline Version *operator ->() {return Ver;};
   inline Version const *operator ->() const {return Ver;};
   inline Version &operator *() {return *Ver;};
   inline Version const &operator *() const {return *Ver;};
   inline operator Version *() {return Ver == Owner->VerP?0:Ver;};
   inline operator Version const *() const {return Ver == Owner->VerP?0:Ver;};
   inline pkgCache *Cache() {return Owner;};
      
   inline const char *VerStr() const {return Ver->VerStr == 0?0:Owner->StrP + Ver->VerStr;};
   inline const char *Section() const {return Ver->Section == 0?0:Owner->StrP + Ver->Section;};
   inline const char *Arch() const {return Ver->Arch == 0?0:Owner->StrP + Ver->Arch;};
   inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + Ver->ParentPkg);};
   inline DescIterator DescriptionList() const;
   DescIterator TranslatedDescription() const;
   inline DepIterator DependsList() const;
   inline PrvIterator ProvidesList() const;
   inline VerFileIterator FileList() const;
   inline unsigned long Index() const {return Ver - Owner->VerP;};
   bool Downloadable() const;
   inline const char *PriorityType() {return Owner->Priority(Ver->Priority);};
   string RelStr();
   
   bool Automatic() const;
   VerFileIterator NewestFile() const;

   inline VerIterator() : Ver(0), Owner(0) {};   
   inline VerIterator(pkgCache &Owner,Version *Trg = 0) : Ver(Trg), 
              Owner(&Owner) 
   { 
      if (Ver == 0)
	 Ver = Owner.VerP;
   };
};

// Description Iterator
class pkgCache::DescIterator
{
   Description *Desc;
   pkgCache *Owner;
   
   void _dummy();
   
   public:

   // Iteration
   void operator ++(int) {if (Desc != Owner->DescP) Desc = Owner->DescP + Desc->NextDesc;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Desc == Owner->DescP?true:false;};
   inline void operator =(const DescIterator &B) {Desc = B.Desc; Owner = B.Owner;};
   
   // Comparison
   inline bool operator ==(const DescIterator &B) const {return Desc == B.Desc;};
   inline bool operator !=(const DescIterator &B) const {return Desc != B.Desc;};
   int CompareDesc(const DescIterator &B) const;
   
   // Accessors
   inline Description *operator ->() {return Desc;};
   inline Description const *operator ->() const {return Desc;};
   inline Description &operator *() {return *Desc;};
   inline Description const &operator *() const {return *Desc;};
   inline operator Description *() {return Desc == Owner->DescP?0:Desc;};
   inline operator Description const *() const {return Desc == Owner->DescP?0:Desc;};
   inline pkgCache *Cache() {return Owner;};
      
   inline const char *LanguageCode() const {return Owner->StrP + Desc->language_code;};
   inline const char *md5() const {return Owner->StrP + Desc->md5sum;};
   inline DescFileIterator FileList() const;
   inline unsigned long Index() const {return Desc - Owner->DescP;};

   inline DescIterator() : Desc(0), Owner(0) {};   
   inline DescIterator(pkgCache &Owner,Description *Trg = 0) : Desc(Trg), 
              Owner(&Owner) 
   { 
      if (Desc == 0)
	 Desc = Owner.DescP;
   };
};

// Dependency iterator
class pkgCache::DepIterator
{
   Dependency *Dep;
   enum {DepVer, DepRev} Type;
   pkgCache *Owner;
   
   void _dummy();
   
   public:

   // Iteration
   void operator ++(int) {if (Dep != Owner->DepP) Dep = Owner->DepP +
	(Type == DepVer?Dep->NextDepends:Dep->NextRevDepends);};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == 0 || Dep == Owner->DepP?true:false;};
   
   // Comparison
   inline bool operator ==(const DepIterator &B) const {return Dep == B.Dep;};
   inline bool operator !=(const DepIterator &B) const {return Dep != B.Dep;};

   // Accessors
   inline Dependency *operator ->() {return Dep;};
   inline Dependency const *operator ->() const {return Dep;};
   inline Dependency &operator *() {return *Dep;};
   inline Dependency const &operator *() const {return *Dep;};
   inline operator Dependency *() {return Dep == Owner->DepP?0:Dep;};
   inline operator Dependency const *() const {return Dep == Owner->DepP?0:Dep;};
   inline pkgCache *Cache() {return Owner;};
   
   inline const char *TargetVer() const {return Dep->Version == 0?0:Owner->StrP + Dep->Version;};
   inline PkgIterator TargetPkg() {return PkgIterator(*Owner,Owner->PkgP + Dep->Package);};
   inline PkgIterator SmartTargetPkg() {PkgIterator R(*Owner,0);SmartTargetPkg(R);return R;};
   inline VerIterator ParentVer() {return VerIterator(*Owner,Owner->VerP + Dep->ParentVer);};
   inline PkgIterator ParentPkg() {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[Dep->ParentVer].ParentPkg);};
   inline bool Reverse() {return Type == DepRev;};
   inline unsigned long Index() const {return Dep - Owner->DepP;};
   bool IsCritical();
   void GlobOr(DepIterator &Start,DepIterator &End);
   Version **AllTargets();   
   bool SmartTargetPkg(PkgIterator &Result);
   inline const char *CompType() {return Owner->CompType(Dep->CompareOp);};
   inline const char *DepType() {return Owner->DepType(Dep->Type);};
   
   inline DepIterator(pkgCache &Owner,Dependency *Trg,Version * = 0) :
          Dep(Trg), Type(DepVer), Owner(&Owner) 
   {
      if (Dep == 0)
	 Dep = Owner.DepP;
   };
   inline DepIterator(pkgCache &Owner,Dependency *Trg,Package *) :
          Dep(Trg), Type(DepRev), Owner(&Owner)
   {
      if (Dep == 0)
	 Dep = Owner.DepP;
   };
   inline DepIterator() : Dep(0), Type(DepVer), Owner(0) {};
};

// Provides iterator
class pkgCache::PrvIterator
{
   Provides *Prv;
   enum {PrvVer, PrvPkg} Type;
   pkgCache *Owner;
   
   void _dummy();
   
   public:

   // Iteration
   void operator ++(int) {if (Prv != Owner->ProvideP) Prv = Owner->ProvideP +
	(Type == PrvVer?Prv->NextPkgProv:Prv->NextProvides);};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return Owner == 0 || Prv == Owner->ProvideP?true:false;};
   
   // Comparison
   inline bool operator ==(const PrvIterator &B) const {return Prv == B.Prv;};
   inline bool operator !=(const PrvIterator &B) const {return Prv != B.Prv;};

   // Accessors
   inline Provides *operator ->() {return Prv;};
   inline Provides const *operator ->() const {return Prv;};
   inline Provides &operator *() {return *Prv;};
   inline Provides const &operator *() const {return *Prv;};
   inline operator Provides *() {return Prv == Owner->ProvideP?0:Prv;};
   inline operator Provides const *() const {return Prv == Owner->ProvideP?0:Prv;};
   inline pkgCache *Cache() {return Owner;};

   inline const char *Name() const {return Owner->StrP + Owner->PkgP[Prv->ParentPkg].Name;};
   inline const char *ProvideVersion() const {return Prv->ProvideVersion == 0?0:Owner->StrP + Prv->ProvideVersion;};
   inline PkgIterator ParentPkg() {return PkgIterator(*Owner,Owner->PkgP + Prv->ParentPkg);};
   inline VerIterator OwnerVer() {return VerIterator(*Owner,Owner->VerP + Prv->Version);};
   inline PkgIterator OwnerPkg() {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[Prv->Version].ParentPkg);};
   inline unsigned long Index() const {return Prv - Owner->ProvideP;};

   inline PrvIterator() : Prv(0), Type(PrvVer), Owner(0)  {};

   inline PrvIterator(pkgCache &Owner,Provides *Trg,Version *) :
          Prv(Trg), Type(PrvVer), Owner(&Owner) 
   {
      if (Prv == 0)
	 Prv = Owner.ProvideP;
   };
   inline PrvIterator(pkgCache &Owner,Provides *Trg,Package *) : 
          Prv(Trg), Type(PrvPkg), Owner(&Owner)
   {
      if (Prv == 0)
	 Prv = Owner.ProvideP;
   };
};

// Package file 
class pkgCache::PkgFileIterator
{
   pkgCache *Owner;
   PackageFile *File;

   public:

   // Iteration
   void operator ++(int) {if (File!= Owner->PkgFileP) File = Owner->PkgFileP + File->NextFile;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return File == Owner->PkgFileP?true:false;};

   // Comparison
   inline bool operator ==(const PkgFileIterator &B) const {return File == B.File;};
   inline bool operator !=(const PkgFileIterator &B) const {return File != B.File;};
			   
   // Accessors
   inline PackageFile *operator ->() {return File;};
   inline PackageFile const *operator ->() const {return File;};
   inline PackageFile const &operator *() const {return *File;};
   inline operator PackageFile *() {return File == Owner->PkgFileP?0:File;};
   inline operator PackageFile const *() const {return File == Owner->PkgFileP?0:File;};
   inline pkgCache *Cache() {return Owner;};

   inline const char *FileName() const {return File->FileName == 0?0:Owner->StrP + File->FileName;};
   inline const char *Archive() const {return File->Archive == 0?0:Owner->StrP + File->Archive;};
   inline const char *Component() const {return File->Component == 0?0:Owner->StrP + File->Component;};
   inline const char *Version() const {return File->Version == 0?0:Owner->StrP + File->Version;};
   inline const char *Origin() const {return File->Origin == 0?0:Owner->StrP + File->Origin;};
   inline const char *Label() const {return File->Label == 0?0:Owner->StrP + File->Label;};
   inline const char *Site() const {return File->Site == 0?0:Owner->StrP + File->Site;};
   inline const char *Architecture() const {return File->Architecture == 0?0:Owner->StrP + File->Architecture;};
   inline const char *IndexType() const {return File->IndexType == 0?0:Owner->StrP + File->IndexType;};

   inline unsigned long Index() const {return File - Owner->PkgFileP;};

   bool IsOk();
   string RelStr();
   
   // Constructors
   inline PkgFileIterator() : Owner(0), File(0) {};
   inline PkgFileIterator(pkgCache &Owner) : Owner(&Owner), File(Owner.PkgFileP) {};
   inline PkgFileIterator(pkgCache &Owner,PackageFile *Trg) : Owner(&Owner), File(Trg) {};
};

// Version File 
class pkgCache::VerFileIterator
{
   pkgCache *Owner;
   VerFile *FileP;

   public:

   // Iteration
   void operator ++(int) {if (FileP != Owner->VerFileP) FileP = Owner->VerFileP + FileP->NextFile;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return FileP == Owner->VerFileP?true:false;};

   // Comparison
   inline bool operator ==(const VerFileIterator &B) const {return FileP == B.FileP;};
   inline bool operator !=(const VerFileIterator &B) const {return FileP != B.FileP;};
			   
   // Accessors
   inline VerFile *operator ->() {return FileP;};
   inline VerFile const *operator ->() const {return FileP;};
   inline VerFile const &operator *() const {return *FileP;};
   inline operator VerFile *() {return FileP == Owner->VerFileP?0:FileP;};
   inline operator VerFile const *() const {return FileP == Owner->VerFileP?0:FileP;};
   inline pkgCache *Cache() {return Owner;};
  
   inline PkgFileIterator File() const {return PkgFileIterator(*Owner,FileP->File + Owner->PkgFileP);};
   inline unsigned long Index() const {return FileP - Owner->VerFileP;};
      
   inline VerFileIterator() : Owner(0), FileP(0) {};
   inline VerFileIterator(pkgCache &Owner,VerFile *Trg) : Owner(&Owner), FileP(Trg) {};
};

// Description File 
class pkgCache::DescFileIterator
{
   pkgCache *Owner;
   DescFile *FileP;

   public:

   // Iteration
   void operator ++(int) {if (FileP != Owner->DescFileP) FileP = Owner->DescFileP + FileP->NextFile;};
   inline void operator ++() {operator ++(0);};
   inline bool end() const {return FileP == Owner->DescFileP?true:false;};

   // Comparison
   inline bool operator ==(const DescFileIterator &B) const {return FileP == B.FileP;};
   inline bool operator !=(const DescFileIterator &B) const {return FileP != B.FileP;};
			   
   // Accessors
   inline DescFile *operator ->() {return FileP;};
   inline DescFile const *operator ->() const {return FileP;};
   inline DescFile const &operator *() const {return *FileP;};
   inline operator DescFile *() {return FileP == Owner->DescFileP?0:FileP;};
   inline operator DescFile const *() const {return FileP == Owner->DescFileP?0:FileP;};
   inline pkgCache *Cache() {return Owner;};
  
   inline PkgFileIterator File() const {return PkgFileIterator(*Owner,FileP->File + Owner->PkgFileP);};
   inline unsigned long Index() const {return FileP - Owner->DescFileP;};
      
   inline DescFileIterator() : Owner(0), FileP(0) {};
   inline DescFileIterator(pkgCache &Owner,DescFile *Trg) : Owner(&Owner), FileP(Trg) {};
};

// Inlined Begin functions cant be in the class because of order problems
inline pkgCache::VerIterator pkgCache::PkgIterator::VersionList() const
       {return VerIterator(*Owner,Owner->VerP + Pkg->VersionList);};
inline pkgCache::VerIterator pkgCache::PkgIterator::CurrentVer() const
       {return VerIterator(*Owner,Owner->VerP + Pkg->CurrentVer);};
inline pkgCache::DepIterator pkgCache::PkgIterator::RevDependsList() const
       {return DepIterator(*Owner,Owner->DepP + Pkg->RevDepends,Pkg);};
inline pkgCache::PrvIterator pkgCache::PkgIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + Pkg->ProvidesList,Pkg);};
inline pkgCache::DescIterator pkgCache::VerIterator::DescriptionList() const
       {return DescIterator(*Owner,Owner->DescP + Ver->DescriptionList);};
inline pkgCache::PrvIterator pkgCache::VerIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + Ver->ProvidesList,Ver);};
inline pkgCache::DepIterator pkgCache::VerIterator::DependsList() const
       {return DepIterator(*Owner,Owner->DepP + Ver->DependsList,Ver);};
inline pkgCache::VerFileIterator pkgCache::VerIterator::FileList() const
       {return VerFileIterator(*Owner,Owner->VerFileP + Ver->FileList);};
inline pkgCache::DescFileIterator pkgCache::DescIterator::FileList() const
       {return DescFileIterator(*Owner,Owner->DescFileP + Desc->FileList);};

#endif

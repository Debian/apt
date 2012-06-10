// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#include<iterator>

#include<string.h>

// abstract Iterator template						/*{{{*/
/* This template provides the very basic iterator methods we
   need to have for doing some walk-over-the-cache magic */
template<typename Str, typename Itr> class pkgCache::Iterator :
			public std::iterator<std::forward_iterator_tag, Str> {
	protected:
	Str *S;
	pkgCache *Owner;

	/** \brief Returns the Pointer for this struct in the owner
	 *  The implementation of this method should be pretty short
	 *  as it will only return the Pointer into the mmap stored
	 *  in the owner but the name of this pointer is different for
	 *  each stucture and we want to abstract here at least for the
	 *  basic methods from the actual structure.
	 *  \return Pointer to the first structure of this type
	 */
	virtual Str* OwnerPointer() const = 0;

	public:
	// Iteration
	virtual void operator ++(int) = 0;
	virtual void operator ++() = 0; // Should be {operator ++(0);};
	inline bool end() const {return Owner == 0 || S == OwnerPointer();};

	// Comparison
	inline bool operator ==(const Itr &B) const {return S == B.S;};
	inline bool operator !=(const Itr &B) const {return S != B.S;};

	// Accessors
	inline Str *operator ->() {return S;};
	inline Str const *operator ->() const {return S;};
	inline operator Str *() {return S == OwnerPointer() ? 0 : S;};
	inline operator Str const *() const {return S == OwnerPointer() ? 0 : S;};
	inline Str &operator *() {return *S;};
	inline Str const &operator *() const {return *S;};
	inline pkgCache *Cache() const {return Owner;};

	// Mixed stuff
	inline void operator =(const Itr &B) {S = B.S; Owner = B.Owner;};
	inline bool IsGood() const { return S && Owner && ! end();};
	inline unsigned long Index() const {return S - OwnerPointer();};

	void ReMap(void const * const oldMap, void const * const newMap) {
		if (Owner == 0 || S == 0)
			return;
		S += (Str*)(newMap) - (Str*)(oldMap);
	}

	// Constructors - look out for the variable assigning
	inline Iterator() : S(0), Owner(0) {};
	inline Iterator(pkgCache &Owner,Str *T = 0) : S(T), Owner(&Owner) {};
};
									/*}}}*/
// Group Iterator							/*{{{*/
/* Packages with the same name are collected in a Group so someone only
   interest in package names can iterate easily over the names, so the
   different architectures can be treated as of the "same" package
   (apt internally treat them as totally different packages) */
class pkgCache::GrpIterator: public Iterator<Group, GrpIterator> {
	long HashIndex;

	protected:
	inline Group* OwnerPointer() const {
		return (Owner != 0) ? Owner->GrpP : 0;
	};

	public:
	// This constructor is the 'begin' constructor, never use it.
	inline GrpIterator(pkgCache &Owner) : Iterator<Group, GrpIterator>(Owner), HashIndex(-1) {
		S = OwnerPointer();
		operator ++(0);
	};

	virtual void operator ++(int);
	virtual void operator ++() {operator ++(0);};

	inline const char *Name() const {return S->Name == 0?0:Owner->StrP + S->Name;};
	inline PkgIterator PackageList() const;
	PkgIterator FindPkg(std::string Arch = "any") const;
	/** \brief find the package with the "best" architecture

	    The best architecture is either the "native" or the first
	    in the list of Architectures which is not an end-Pointer

	    \param PreferNonVirtual tries to respond with a non-virtual package
		   and only if this fails returns the best virtual package */
	PkgIterator FindPreferredPkg(bool const &PreferNonVirtual = true) const;
	PkgIterator NextPkg(PkgIterator const &Pkg) const;

	// Constructors
	inline GrpIterator(pkgCache &Owner, Group *Trg) : Iterator<Group, GrpIterator>(Owner, Trg), HashIndex(0) {
		if (S == 0)
			S = OwnerPointer();
	};
	inline GrpIterator() : Iterator<Group, GrpIterator>(), HashIndex(0) {};

};
									/*}}}*/
// Package Iterator							/*{{{*/
class pkgCache::PkgIterator: public Iterator<Package, PkgIterator> {
	long HashIndex;

	protected:
	inline Package* OwnerPointer() const {
		return (Owner != 0) ? Owner->PkgP : 0;
	};

	public:
	// This constructor is the 'begin' constructor, never use it.
	inline PkgIterator(pkgCache &Owner) : Iterator<Package, PkgIterator>(Owner), HashIndex(-1) {
		S = OwnerPointer();
		operator ++(0);
	};

	virtual void operator ++(int);
	virtual void operator ++() {operator ++(0);};

	enum OkState {NeedsNothing,NeedsUnpack,NeedsConfigure};

	// Accessors
	inline const char *Name() const {return S->Name == 0?0:Owner->StrP + S->Name;};
	inline const char *Section() const {return S->Section == 0?0:Owner->StrP + S->Section;};
	inline bool Purge() const {return S->CurrentState == pkgCache::State::Purge ||
		(S->CurrentVer == 0 && S->CurrentState == pkgCache::State::NotInstalled);};
	inline const char *Arch() const {return S->Arch == 0?0:Owner->StrP + S->Arch;};
	inline GrpIterator Group() const { return GrpIterator(*Owner, Owner->GrpP + S->Group);};

	inline VerIterator VersionList() const;
	inline VerIterator CurrentVer() const;
	inline DepIterator RevDependsList() const;
	inline PrvIterator ProvidesList() const;
	OkState State() const;
	const char *CandVersion() const;
	const char *CurVersion() const;

	//Nice printable representation
	friend std::ostream& operator <<(std::ostream& out, PkgIterator i);
	std::string FullName(bool const &Pretty = false) const;

	// Constructors
	inline PkgIterator(pkgCache &Owner,Package *Trg) : Iterator<Package, PkgIterator>(Owner, Trg), HashIndex(0) {
		if (S == 0)
			S = OwnerPointer();
	};
	inline PkgIterator() : Iterator<Package, PkgIterator>(), HashIndex(0) {};
};
									/*}}}*/
// Version Iterator							/*{{{*/
class pkgCache::VerIterator : public Iterator<Version, VerIterator> {
	protected:
	inline Version* OwnerPointer() const {
		return (Owner != 0) ? Owner->VerP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->VerP) S = Owner->VerP + S->NextVer;};
	inline void operator ++() {operator ++(0);};

	// Comparison
	int CompareVer(const VerIterator &B) const;
	/** \brief compares two version and returns if they are similar

	    This method should be used to identify if two pseudo versions are
	    refering to the same "real" version */
	inline bool SimilarVer(const VerIterator &B) const {
		return (B.end() == false && S->Hash == B->Hash && strcmp(VerStr(), B.VerStr()) == 0);
	};

	// Accessors
	inline const char *VerStr() const {return S->VerStr == 0?0:Owner->StrP + S->VerStr;};
	inline const char *Section() const {return S->Section == 0?0:Owner->StrP + S->Section;};
	inline const char *Arch() const {
		if ((S->MultiArch & pkgCache::Version::All) == pkgCache::Version::All)
			return "all";
		return S->ParentPkg == 0?0:Owner->StrP + ParentPkg()->Arch;
	};
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + S->ParentPkg);};

	inline DescIterator DescriptionList() const;
	DescIterator TranslatedDescription() const;
	inline DepIterator DependsList() const;
	inline PrvIterator ProvidesList() const;
	inline VerFileIterator FileList() const;
	bool Downloadable() const;
	inline const char *PriorityType() const {return Owner->Priority(S->Priority);};
	std::string RelStr() const;

	bool Automatic() const;
	VerFileIterator NewestFile() const;

	inline VerIterator(pkgCache &Owner,Version *Trg = 0) : Iterator<Version, VerIterator>(Owner, Trg) {
		if (S == 0)
			S = OwnerPointer();
	};
	inline VerIterator() : Iterator<Version, VerIterator>() {};
};
									/*}}}*/
// Description Iterator							/*{{{*/
class pkgCache::DescIterator : public Iterator<Description, DescIterator> {
	protected:
	inline Description* OwnerPointer() const {
		return (Owner != 0) ? Owner->DescP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->DescP) S = Owner->DescP + S->NextDesc;};
	inline void operator ++() {operator ++(0);};

	// Comparison
	int CompareDesc(const DescIterator &B) const;

	// Accessors
	inline const char *LanguageCode() const {return Owner->StrP + S->language_code;};
	inline const char *md5() const {return Owner->StrP + S->md5sum;};
	inline DescFileIterator FileList() const;

	inline DescIterator() : Iterator<Description, DescIterator>() {};
	inline DescIterator(pkgCache &Owner,Description *Trg = 0) : Iterator<Description, DescIterator>(Owner, Trg) {
		if (S == 0)
			S = Owner.DescP;
	};
};
									/*}}}*/
// Dependency iterator							/*{{{*/
class pkgCache::DepIterator : public Iterator<Dependency, DepIterator> {
	enum {DepVer, DepRev} Type;

	protected:
	inline Dependency* OwnerPointer() const {
		return (Owner != 0) ? Owner->DepP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->DepP) S = Owner->DepP +
		(Type == DepVer ? S->NextDepends : S->NextRevDepends);};
	inline void operator ++() {operator ++(0);};

	// Accessors
	inline const char *TargetVer() const {return S->Version == 0?0:Owner->StrP + S->Version;};
	inline PkgIterator TargetPkg() const {return PkgIterator(*Owner,Owner->PkgP + S->Package);};
	inline PkgIterator SmartTargetPkg() const {PkgIterator R(*Owner,0);SmartTargetPkg(R);return R;};
	inline VerIterator ParentVer() const {return VerIterator(*Owner,Owner->VerP + S->ParentVer);};
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[S->ParentVer].ParentPkg);};
	inline bool Reverse() const {return Type == DepRev;};
	bool IsCritical() const;
	bool IsNegative() const;
	bool IsIgnorable(PrvIterator const &Prv) const;
	bool IsIgnorable(PkgIterator const &Pkg) const;
	bool IsMultiArchImplicit() const;
	void GlobOr(DepIterator &Start,DepIterator &End);
	Version **AllTargets() const;
	bool SmartTargetPkg(PkgIterator &Result) const;
	inline const char *CompType() const {return Owner->CompType(S->CompareOp);};
	inline const char *DepType() const {return Owner->DepType(S->Type);};

	//Nice printable representation
	friend std::ostream& operator <<(std::ostream& out, DepIterator D);

	inline DepIterator(pkgCache &Owner, Dependency *Trg, Version* = 0) :
		Iterator<Dependency, DepIterator>(Owner, Trg), Type(DepVer) {
		if (S == 0)
			S = Owner.DepP;
	};
	inline DepIterator(pkgCache &Owner, Dependency *Trg, Package*) :
		Iterator<Dependency, DepIterator>(Owner, Trg), Type(DepRev) {
		if (S == 0)
			S = Owner.DepP;
	};
	inline DepIterator() : Iterator<Dependency, DepIterator>(), Type(DepVer) {};
};
									/*}}}*/
// Provides iterator							/*{{{*/
class pkgCache::PrvIterator : public Iterator<Provides, PrvIterator> {
	enum {PrvVer, PrvPkg} Type;

	protected:
	inline Provides* OwnerPointer() const {
		return (Owner != 0) ? Owner->ProvideP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->ProvideP) S = Owner->ProvideP +
		(Type == PrvVer?S->NextPkgProv:S->NextProvides);};
	inline void operator ++() {operator ++(0);};

	// Accessors
	inline const char *Name() const {return Owner->StrP + Owner->PkgP[S->ParentPkg].Name;};
	inline const char *ProvideVersion() const {return S->ProvideVersion == 0?0:Owner->StrP + S->ProvideVersion;};
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + S->ParentPkg);};
	inline VerIterator OwnerVer() const {return VerIterator(*Owner,Owner->VerP + S->Version);};
	inline PkgIterator OwnerPkg() const {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[S->Version].ParentPkg);};

	bool IsMultiArchImplicit() const;

	inline PrvIterator() : Iterator<Provides, PrvIterator>(), Type(PrvVer) {};
	inline PrvIterator(pkgCache &Owner, Provides *Trg, Version*) :
		Iterator<Provides, PrvIterator>(Owner, Trg), Type(PrvVer) {
		if (S == 0)
			S = Owner.ProvideP;
	};
	inline PrvIterator(pkgCache &Owner, Provides *Trg, Package*) :
		Iterator<Provides, PrvIterator>(Owner, Trg), Type(PrvPkg) {
		if (S == 0)
			S = Owner.ProvideP;
	};
};
									/*}}}*/
// Package file								/*{{{*/
class pkgCache::PkgFileIterator : public Iterator<PackageFile, PkgFileIterator> {
	protected:
	inline PackageFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->PkgFileP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->PkgFileP) S = Owner->PkgFileP + S->NextFile;};
	inline void operator ++() {operator ++(0);};

	// Accessors
	inline const char *FileName() const {return S->FileName == 0?0:Owner->StrP + S->FileName;};
	inline const char *Archive() const {return S->Archive == 0?0:Owner->StrP + S->Archive;};
	inline const char *Component() const {return S->Component == 0?0:Owner->StrP + S->Component;};
	inline const char *Version() const {return S->Version == 0?0:Owner->StrP + S->Version;};
	inline const char *Origin() const {return S->Origin == 0?0:Owner->StrP + S->Origin;};
	inline const char *Codename() const {return S->Codename ==0?0:Owner->StrP + S->Codename;};
	inline const char *Label() const {return S->Label == 0?0:Owner->StrP + S->Label;};
	inline const char *Site() const {return S->Site == 0?0:Owner->StrP + S->Site;};
	inline const char *Architecture() const {return S->Architecture == 0?0:Owner->StrP + S->Architecture;};
	inline const char *IndexType() const {return S->IndexType == 0?0:Owner->StrP + S->IndexType;};

	bool IsOk();
	std::string RelStr();

	// Constructors
	inline PkgFileIterator() : Iterator<PackageFile, PkgFileIterator>() {};
	inline PkgFileIterator(pkgCache &Owner) : Iterator<PackageFile, PkgFileIterator>(Owner, Owner.PkgFileP) {};
	inline PkgFileIterator(pkgCache &Owner,PackageFile *Trg) : Iterator<PackageFile, PkgFileIterator>(Owner, Trg) {};
};
									/*}}}*/
// Version File								/*{{{*/
class pkgCache::VerFileIterator : public pkgCache::Iterator<VerFile, VerFileIterator> {
	protected:
	inline VerFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->VerFileP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->VerFileP) S = Owner->VerFileP + S->NextFile;};
	inline void operator ++() {operator ++(0);};

	// Accessors
	inline PkgFileIterator File() const {return PkgFileIterator(*Owner,S->File + Owner->PkgFileP);};

	inline VerFileIterator() : Iterator<VerFile, VerFileIterator>() {};
	inline VerFileIterator(pkgCache &Owner,VerFile *Trg) : Iterator<VerFile, VerFileIterator>(Owner, Trg) {};
};
									/*}}}*/
// Description File							/*{{{*/
class pkgCache::DescFileIterator : public Iterator<DescFile, DescFileIterator> {
	protected:
	inline DescFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->DescFileP : 0;
	};

	public:
	// Iteration
	void operator ++(int) {if (S != Owner->DescFileP) S = Owner->DescFileP + S->NextFile;};
	inline void operator ++() {operator ++(0);};

	// Accessors
	inline PkgFileIterator File() const {return PkgFileIterator(*Owner,S->File + Owner->PkgFileP);};

	inline DescFileIterator() : Iterator<DescFile, DescFileIterator>() {};
	inline DescFileIterator(pkgCache &Owner,DescFile *Trg) : Iterator<DescFile, DescFileIterator>(Owner, Trg) {};
};
									/*}}}*/
// Inlined Begin functions cant be in the class because of order problems /*{{{*/
inline pkgCache::PkgIterator pkgCache::GrpIterator::PackageList() const
       {return PkgIterator(*Owner,Owner->PkgP + S->FirstPackage);};
inline pkgCache::VerIterator pkgCache::PkgIterator::VersionList() const
       {return VerIterator(*Owner,Owner->VerP + S->VersionList);};
inline pkgCache::VerIterator pkgCache::PkgIterator::CurrentVer() const
       {return VerIterator(*Owner,Owner->VerP + S->CurrentVer);};
inline pkgCache::DepIterator pkgCache::PkgIterator::RevDependsList() const
       {return DepIterator(*Owner,Owner->DepP + S->RevDepends,S);};
inline pkgCache::PrvIterator pkgCache::PkgIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + S->ProvidesList,S);};
inline pkgCache::DescIterator pkgCache::VerIterator::DescriptionList() const
       {return DescIterator(*Owner,Owner->DescP + S->DescriptionList);};
inline pkgCache::PrvIterator pkgCache::VerIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + S->ProvidesList,S);};
inline pkgCache::DepIterator pkgCache::VerIterator::DependsList() const
       {return DepIterator(*Owner,Owner->DepP + S->DependsList,S);};
inline pkgCache::VerFileIterator pkgCache::VerIterator::FileList() const
       {return VerFileIterator(*Owner,Owner->VerFileP + S->FileList);};
inline pkgCache::DescFileIterator pkgCache::DescIterator::FileList() const
       {return DescFileIterator(*Owner,Owner->DescFileP + S->FileList);};
									/*}}}*/
#endif

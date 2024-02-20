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
#ifndef __PKGLIB_IN_PKGCACHE_H
#warning apt-pkg/cacheiterators.h should not be included directly, include apt-pkg/pkgcache.h instead
#endif
#include <apt-pkg/macros.h>

#include <iosfwd>
#include <iterator>
#include <string>
#include <apt-pkg/string_view.h>

#include <cstring>

// abstract Iterator template						/*{{{*/
/* This template provides the very basic iterator methods we
   need to have for doing some walk-over-the-cache magic */
template<typename Str, typename Itr> class APT_PUBLIC pkgCache::Iterator {
	/** \brief Returns the Pointer for this struct in the owner
	 *  The implementation of this method should be pretty short
	 *  as it will only return the Pointer into the mmap stored
	 *  in the owner but the name of this pointer is different for
	 *  each structure and we want to abstract here at least for the
	 *  basic methods from the actual structure.
	 *  \return Pointer to the first structure of this type
	 */
	Str* OwnerPointer() const { return static_cast<Itr const*>(this)->OwnerPointer(); }

	protected:
	Str *S;
	pkgCache *Owner;

	public:
	// iterator_traits
	using iterator_category = std::forward_iterator_tag;
	using value_type = Str;
	using difference_type = std::ptrdiff_t;
	using pointer = Str*;
	using reference = Str&;
	// Iteration
	inline bool end() const {return Owner == 0 || S == OwnerPointer();}

	// Comparison
	inline bool operator ==(const Itr &B) const {return S == B.S;}
	inline bool operator !=(const Itr &B) const {return S != B.S;}

	// Accessors
	inline Str *operator ->() {return S;}
	inline Str const *operator ->() const {return S;}
	inline operator Str *() {return S == OwnerPointer() ? 0 : S;}
	inline operator Str const *() const {return S == OwnerPointer() ? 0 : S;}
	inline Str &operator *() {return *S;}
	inline Str const &operator *() const {return *S;}
	inline pkgCache *Cache() const {return Owner;}

	// Mixed stuff
	inline bool IsGood() const { return S && Owner && ! end();}
	inline unsigned long Index() const {return S - OwnerPointer();}
	inline map_pointer<Str> MapPointer() const {return map_pointer<Str>(Index()) ;}

	void ReMap(void const * const oldMap, void * const newMap) {
		if (Owner == 0 || S == 0)
			return;
		S = static_cast<Str *>(newMap) + (S - static_cast<Str const *>(oldMap));
	}

	// Constructors - look out for the variable assigning
	inline Iterator() : S(0), Owner(0) {}
	inline Iterator(pkgCache &Owner,Str *T = 0) : S(T), Owner(&Owner) {}
};
									/*}}}*/
// Group Iterator							/*{{{*/
/* Packages with the same name are collected in a Group so someone only
   interest in package names can iterate easily over the names, so the
   different architectures can be treated as of the "same" package
   (apt internally treat them as totally different packages) */
class APT_PUBLIC pkgCache::GrpIterator: public Iterator<Group, GrpIterator> {
	long HashIndex;

	public:
	inline Group* OwnerPointer() const {
		return (Owner != 0) ? Owner->GrpP : 0;
	}

	// This constructor is the 'begin' constructor, never use it.
	explicit inline GrpIterator(pkgCache &Owner) : Iterator<Group, GrpIterator>(Owner), HashIndex(-1) {
		S = OwnerPointer();
		operator++();
	}

	GrpIterator& operator++();
	inline GrpIterator operator++(int) { GrpIterator const tmp(*this); operator++(); return tmp; }

	inline const char *Name() const {return S->Name == 0?0:Owner->StrP + S->Name;}
	inline PkgIterator PackageList() const;
	inline VerIterator VersionsInSource() const;
	PkgIterator FindPkg(APT::StringView Arch = APT::StringView("any", 3)) const;
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
	}
	inline GrpIterator() : Iterator<Group, GrpIterator>(), HashIndex(0) {}

};
									/*}}}*/
// Package Iterator							/*{{{*/
class APT_PUBLIC pkgCache::PkgIterator: public Iterator<Package, PkgIterator> {
	long HashIndex;

	public:
	inline Package* OwnerPointer() const {
		return (Owner != 0) ? Owner->PkgP : 0;
	}

	// This constructor is the 'begin' constructor, never use it.
	explicit inline PkgIterator(pkgCache &Owner) : Iterator<Package, PkgIterator>(Owner), HashIndex(-1) {
		S = OwnerPointer();
		operator++();
	}

	PkgIterator& operator++();
	inline PkgIterator operator++(int) { PkgIterator const tmp(*this); operator++(); return tmp; }

	enum OkState {NeedsNothing,NeedsUnpack,NeedsConfigure};

	// Accessors
	inline const char *Name() const { return Group().Name(); }
	inline bool Purge() const {return S->CurrentState == pkgCache::State::Purge ||
		(S->CurrentVer == 0 && S->CurrentState == pkgCache::State::NotInstalled);}
	inline const char *Arch() const {return S->Arch == 0?0:Owner->StrP + S->Arch;}
	inline APT_PURE GrpIterator Group() const { return GrpIterator(*Owner, Owner->GrpP + S->Group);}

	inline VerIterator VersionList() const APT_PURE;
	inline VerIterator CurrentVer() const APT_PURE;
	inline DepIterator RevDependsList() const APT_PURE;
	inline PrvIterator ProvidesList() const APT_PURE;
	OkState State() const APT_PURE;
	const char *CurVersion() const APT_PURE;

	//Nice printable representation
	APT_DEPRECATED_MSG("Use APT::PrettyPkg instead") friend std::ostream& operator <<(std::ostream& out, PkgIterator i);
	std::string FullName(bool const &Pretty = false) const;

	// Constructors
	inline PkgIterator(pkgCache &Owner,Package *Trg) : Iterator<Package, PkgIterator>(Owner, Trg), HashIndex(0) {
		if (S == 0)
			S = OwnerPointer();
	}
	inline PkgIterator() : Iterator<Package, PkgIterator>(), HashIndex(0) {}
};
									/*}}}*/
// Version Iterator							/*{{{*/
class APT_PUBLIC pkgCache::VerIterator : public Iterator<Version, VerIterator> {
	public:
	inline Version* OwnerPointer() const {
		return (Owner != 0) ? Owner->VerP : 0;
	}

	// Iteration
	inline VerIterator& operator++() {if (S != Owner->VerP) S = Owner->VerP + S->NextVer; return *this;}
	inline VerIterator operator++(int) { VerIterator const tmp(*this); operator++(); return tmp; }

	inline VerIterator NextInSource()
	{
	   if (S != Owner->VerP)
	      S = Owner->VerP + S->NextInSource;
	   return *this;
	}

	// Comparison
	int CompareVer(const VerIterator &B) const;
	/** \brief compares two version and returns if they are similar

	    This method should be used to identify if two pseudo versions are
	    referring to the same "real" version */
	inline bool SimilarVer(const VerIterator &B) const {
		return (B.end() == false && S->Hash == B->Hash && strcmp(VerStr(), B.VerStr()) == 0);
	}

	// Accessors
	inline const char *VerStr() const {return S->VerStr == 0?0:Owner->StrP + S->VerStr;}
	inline const char *Section() const {return S->Section == 0?0:Owner->StrP + S->Section;}
	/** \brief source package name this version comes from
	   Always contains the name, even if it is the same as the binary name */
	inline const char *SourcePkgName() const {return Owner->StrP + S->SourcePkgName;}
	/** \brief source version this version comes from
	   Always contains the version string, even if it is the same as the binary version */
	inline const char *SourceVerStr() const {return Owner->StrP + S->SourceVerStr;}
	inline const char *Arch() const {
		if ((S->MultiArch & pkgCache::Version::All) == pkgCache::Version::All)
			return "all";
		return S->ParentPkg == 0?0:Owner->StrP + ParentPkg()->Arch;
	}
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + S->ParentPkg);}

	inline DescIterator DescriptionList() const;
	DescIterator TranslatedDescriptionForLanguage(APT::StringView lang) const;
	DescIterator TranslatedDescription() const;
	inline DepIterator DependsList() const;
	inline PrvIterator ProvidesList() const;
	inline VerFileIterator FileList() const;
	bool Downloadable() const;
	inline const char *PriorityType() const {return Owner->Priority(S->Priority);}
	const char *MultiArchType() const APT_PURE;
	std::string RelStr() const;

	bool Automatic() const;
	VerFileIterator NewestFile() const;
	bool IsSecurityUpdate() const;

#ifdef APT_COMPILING_APT
	inline unsigned int PhasedUpdatePercentage() const
	{
	   return (static_cast<Version::Extra *>(Owner->Map.Data()) + S->d)->PhasedUpdatePercentage;
	}
	inline bool PhasedUpdatePercentage(unsigned int percentage)
	{
	   if (percentage > 100)
	      return false;
	   (static_cast<Version::Extra *>(Owner->Map.Data()) + S->d)->PhasedUpdatePercentage = static_cast<uint8_t>(percentage);
	   return true;
	}
#endif

	inline VerIterator(pkgCache &Owner,Version *Trg = 0) : Iterator<Version, VerIterator>(Owner, Trg) {
		if (S == 0)
			S = OwnerPointer();
	}
	inline VerIterator() : Iterator<Version, VerIterator>() {}
};
									/*}}}*/
// Description Iterator							/*{{{*/
class APT_PUBLIC pkgCache::DescIterator : public Iterator<Description, DescIterator> {
	public:
	inline Description* OwnerPointer() const {
		return (Owner != 0) ? Owner->DescP : 0;
	}

	// Iteration
	inline DescIterator& operator++() {if (S != Owner->DescP) S = Owner->DescP + S->NextDesc; return *this;}
	inline DescIterator operator++(int) { DescIterator const tmp(*this); operator++(); return tmp; }

	// Comparison
	int CompareDesc(const DescIterator &B) const;

	// Accessors
	inline const char *LanguageCode() const {return Owner->StrP + S->language_code;}
	inline const char *md5() const {return Owner->StrP + S->md5sum;}
	inline DescFileIterator FileList() const;

	inline DescIterator() : Iterator<Description, DescIterator>() {}
	inline DescIterator(pkgCache &Owner,Description *Trg = 0) : Iterator<Description, DescIterator>(Owner, Trg) {
		if (S == 0)
			S = Owner.DescP;
	}
};
									/*}}}*/
// Dependency iterator							/*{{{*/
class APT_PUBLIC pkgCache::DepIterator : public Iterator<Dependency, DepIterator> {
	enum {DepVer, DepRev} Type;
	DependencyData * S2;

	public:
	inline Dependency* OwnerPointer() const {
		return (Owner != 0) ? Owner->DepP : 0;
	}

	// Iteration
	DepIterator& operator++();
	inline DepIterator operator++(int) { DepIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline const char *TargetVer() const {return S2->Version == 0?0:Owner->StrP + S2->Version;}
	inline PkgIterator TargetPkg() const {return PkgIterator(*Owner,Owner->PkgP + S2->Package);}
	inline PkgIterator SmartTargetPkg() const {PkgIterator R(*Owner,0);SmartTargetPkg(R);return R;}
	inline VerIterator ParentVer() const {return VerIterator(*Owner,Owner->VerP + S->ParentVer);}
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[uint32_t(S->ParentVer)].ParentPkg);}
	inline bool Reverse() const {return Type == DepRev;}
	bool IsCritical() const APT_PURE;
	bool IsNegative() const APT_PURE;
	bool IsIgnorable(PrvIterator const &Prv) const APT_PURE;
	bool IsIgnorable(PkgIterator const &Pkg) const APT_PURE;
	/* MultiArch can be translated to SingleArch for an resolver and we did so,
	   by adding dependencies to help the resolver understand the problem, but
	   sometimes it is needed to identify these to ignore them… */
	inline bool IsMultiArchImplicit() const APT_PURE {
		return (S2->CompareOp & pkgCache::Dep::MultiArchImplicit) == pkgCache::Dep::MultiArchImplicit;
	}
	/* This covers additionally negative dependencies, which aren't arch-specific,
	   but change architecture nonetheless as a Conflicts: foo does applies for all archs */
	bool IsImplicit() const APT_PURE;

	bool IsSatisfied(VerIterator const &Ver) const APT_PURE;
	bool IsSatisfied(PrvIterator const &Prv) const APT_PURE;
	void GlobOr(DepIterator &Start,DepIterator &End);
	Version **AllTargets() const;
	bool SmartTargetPkg(PkgIterator &Result) const;
	inline const char *CompType() const {return Owner->CompType(S2->CompareOp);}
	inline const char *DepType() const {return Owner->DepType(S2->Type);}

	// overrides because we are special
	struct DependencyProxy
	{
	   map_stringitem_t &Version;
	   map_pointer<pkgCache::Package> &Package;
	   map_id_t &ID;
	   unsigned char &Type;
	   unsigned char &CompareOp;
	   map_pointer<pkgCache::Version> &ParentVer;
	   map_pointer<pkgCache::DependencyData> &DependencyData;
	   map_pointer<Dependency> &NextRevDepends;
	   map_pointer<Dependency> &NextDepends;
	   map_pointer<pkgCache::DependencyData> &NextData;
	   DependencyProxy const * operator->() const { return this; }
	   DependencyProxy * operator->() { return this; }
	};
	inline DependencyProxy operator->() const {return (DependencyProxy) { S2->Version, S2->Package, S->ID, S2->Type, S2->CompareOp, S->ParentVer, S->DependencyData, S->NextRevDepends, S->NextDepends, S2->NextData };}
	inline DependencyProxy operator->() {return (DependencyProxy) { S2->Version, S2->Package, S->ID, S2->Type, S2->CompareOp, S->ParentVer, S->DependencyData, S->NextRevDepends, S->NextDepends, S2->NextData };}
	void ReMap(void const * const oldMap, void * const newMap)
	{
		Iterator<Dependency, DepIterator>::ReMap(oldMap, newMap);
		if (Owner == 0 || S == 0 || S2 == 0)
			return;
		S2 = static_cast<DependencyData *>(newMap) + (S2 - static_cast<DependencyData const *>(oldMap));
	}

	//Nice printable representation
	APT_DEPRECATED_MSG("Use APT::PrettyDep instead") friend std::ostream& operator <<(std::ostream& out, DepIterator D);

	inline DepIterator(pkgCache &Owner, Dependency *Trg, Version* = 0) :
		Iterator<Dependency, DepIterator>(Owner, Trg), Type(DepVer), S2(Trg == 0 ? Owner.DepDataP : (Owner.DepDataP + Trg->DependencyData)) {
		if (S == 0)
			S = Owner.DepP;
	}
	inline DepIterator(pkgCache &Owner, Dependency *Trg, Package*) :
		Iterator<Dependency, DepIterator>(Owner, Trg), Type(DepRev), S2(Trg == 0 ? Owner.DepDataP : (Owner.DepDataP + Trg->DependencyData)) {
		if (S == 0)
			S = Owner.DepP;
	}
	inline DepIterator() : Iterator<Dependency, DepIterator>(), Type(DepVer), S2(0) {}
};
									/*}}}*/
// Provides iterator							/*{{{*/
class APT_PUBLIC pkgCache::PrvIterator : public Iterator<Provides, PrvIterator> {
	enum {PrvVer, PrvPkg} Type;

	public:
	inline Provides* OwnerPointer() const {
		return (Owner != 0) ? Owner->ProvideP : 0;
	}

	// Iteration
	inline PrvIterator& operator ++() {if (S != Owner->ProvideP) S = Owner->ProvideP +
	   (Type == PrvVer?S->NextPkgProv:S->NextProvides); return *this;}
	inline PrvIterator operator++(int) { PrvIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline const char *Name() const {return ParentPkg().Name();}
	inline const char *ProvideVersion() const {return S->ProvideVersion == 0?0:Owner->StrP + S->ProvideVersion;}
	inline PkgIterator ParentPkg() const {return PkgIterator(*Owner,Owner->PkgP + S->ParentPkg);}
	inline VerIterator OwnerVer() const {return VerIterator(*Owner,Owner->VerP + S->Version);}
	inline PkgIterator OwnerPkg() const {return PkgIterator(*Owner,Owner->PkgP + Owner->VerP[uint32_t(S->Version)].ParentPkg);}

	/* MultiArch can be translated to SingleArch for an resolver and we did so,
	   by adding provides to help the resolver understand the problem, but
	   sometimes it is needed to identify these to ignore them… */
	bool IsMultiArchImplicit() const APT_PURE
	{ return (S->Flags & pkgCache::Flag::MultiArchImplicit) == pkgCache::Flag::MultiArchImplicit; }


	inline PrvIterator() : Iterator<Provides, PrvIterator>(), Type(PrvVer) {}
	inline PrvIterator(pkgCache &Owner, Provides *Trg, Version*) :
		Iterator<Provides, PrvIterator>(Owner, Trg), Type(PrvVer) {
		if (S == 0)
			S = Owner.ProvideP;
	}
	inline PrvIterator(pkgCache &Owner, Provides *Trg, Package*) :
		Iterator<Provides, PrvIterator>(Owner, Trg), Type(PrvPkg) {
		if (S == 0)
			S = Owner.ProvideP;
	}
};
									/*}}}*/
// Release file								/*{{{*/
class APT_PUBLIC pkgCache::RlsFileIterator : public Iterator<ReleaseFile, RlsFileIterator> {
	public:
	inline ReleaseFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->RlsFileP : 0;
	}

	// Iteration
	inline RlsFileIterator& operator++() {if (S != Owner->RlsFileP) S = Owner->RlsFileP + S->NextFile;return *this;}
	inline RlsFileIterator operator++(int) { RlsFileIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline const char *FileName() const {return S->FileName == 0?0:Owner->StrP + S->FileName;}
	inline const char *Archive() const {return S->Archive == 0?0:Owner->StrP + S->Archive;}
	inline const char *Version() const {return S->Version == 0?0:Owner->StrP + S->Version;}
	inline const char *Origin() const {return S->Origin == 0?0:Owner->StrP + S->Origin;}
	inline const char *Codename() const {return S->Codename ==0?0:Owner->StrP + S->Codename;}
	inline const char *Label() const {return S->Label == 0?0:Owner->StrP + S->Label;}
	inline const char *Site() const {return S->Site == 0?0:Owner->StrP + S->Site;}
	inline bool Flagged(pkgCache::Flag::ReleaseFileFlags const flag) const {return (S->Flags & flag) == flag; }

	std::string RelStr();

	// Constructors
	inline RlsFileIterator() : Iterator<ReleaseFile, RlsFileIterator>() {}
	explicit inline RlsFileIterator(pkgCache &Owner) : Iterator<ReleaseFile, RlsFileIterator>(Owner, Owner.RlsFileP) {}
	inline RlsFileIterator(pkgCache &Owner,ReleaseFile *Trg) : Iterator<ReleaseFile, RlsFileIterator>(Owner, Trg) {}
};
									/*}}}*/
// Package file								/*{{{*/
class APT_PUBLIC pkgCache::PkgFileIterator : public Iterator<PackageFile, PkgFileIterator> {
	public:
	inline PackageFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->PkgFileP : 0;
	}

	// Iteration
	inline PkgFileIterator& operator++() {if (S != Owner->PkgFileP) S = Owner->PkgFileP + S->NextFile; return *this;}
	inline PkgFileIterator operator++(int) { PkgFileIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline const char *FileName() const {return S->FileName == 0?0:Owner->StrP + S->FileName;}
	inline pkgCache::RlsFileIterator ReleaseFile() const {return RlsFileIterator(*Owner, Owner->RlsFileP + S->Release);}
	inline const char *Archive() const {return S->Release == 0 ? Component() : ReleaseFile().Archive();}
	inline const char *Version() const {return S->Release == 0 ? NULL : ReleaseFile().Version();}
	inline const char *Origin() const {return S->Release == 0 ? NULL : ReleaseFile().Origin();}
	inline const char *Codename() const {return S->Release == 0 ? NULL : ReleaseFile().Codename();}
	inline const char *Label() const {return S->Release == 0 ? NULL : ReleaseFile().Label();}
	inline const char *Site() const {return S->Release == 0 ? NULL : ReleaseFile().Site();}
	inline bool Flagged(pkgCache::Flag::ReleaseFileFlags const flag) const {return S->Release== 0 ? false : ReleaseFile().Flagged(flag);}
	inline bool Flagged(pkgCache::Flag::PkgFFlags const flag) const {return (S->Flags & flag) == flag;}
	inline const char *Component() const {return S->Component == 0?0:Owner->StrP + S->Component;}
	inline const char *Architecture() const {return S->Architecture == 0?0:Owner->StrP + S->Architecture;}
	inline const char *IndexType() const {return S->IndexType == 0?0:Owner->StrP + S->IndexType;}

	std::string RelStr();

	// Constructors
	inline PkgFileIterator() : Iterator<PackageFile, PkgFileIterator>() {}
	explicit inline PkgFileIterator(pkgCache &Owner) : Iterator<PackageFile, PkgFileIterator>(Owner, Owner.PkgFileP) {}
	inline PkgFileIterator(pkgCache &Owner,PackageFile *Trg) : Iterator<PackageFile, PkgFileIterator>(Owner, Trg) {}
};
									/*}}}*/
// Version File								/*{{{*/
class APT_PUBLIC pkgCache::VerFileIterator : public pkgCache::Iterator<VerFile, VerFileIterator> {
	public:
	inline VerFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->VerFileP : 0;
	}

	// Iteration
	inline VerFileIterator& operator++() {if (S != Owner->VerFileP) S = Owner->VerFileP + S->NextFile; return *this;}
	inline VerFileIterator operator++(int) { VerFileIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline PkgFileIterator File() const {return PkgFileIterator(*Owner, Owner->PkgFileP + S->File);}

	inline VerFileIterator() : Iterator<VerFile, VerFileIterator>() {}
	inline VerFileIterator(pkgCache &Owner,VerFile *Trg) : Iterator<VerFile, VerFileIterator>(Owner, Trg) {}
};
									/*}}}*/
// Description File							/*{{{*/
class APT_PUBLIC pkgCache::DescFileIterator : public Iterator<DescFile, DescFileIterator> {
	public:
	inline DescFile* OwnerPointer() const {
		return (Owner != 0) ? Owner->DescFileP : 0;
	}

	// Iteration
	inline DescFileIterator& operator++() {if (S != Owner->DescFileP) S = Owner->DescFileP + S->NextFile; return *this;}
	inline DescFileIterator operator++(int) { DescFileIterator const tmp(*this); operator++(); return tmp; }

	// Accessors
	inline PkgFileIterator File() const {return PkgFileIterator(*Owner, Owner->PkgFileP + S->File);}

	inline DescFileIterator() : Iterator<DescFile, DescFileIterator>() {}
	inline DescFileIterator(pkgCache &Owner,DescFile *Trg) : Iterator<DescFile, DescFileIterator>(Owner, Trg) {}
};
									/*}}}*/
// Inlined Begin functions can't be in the class because of order problems /*{{{*/
inline pkgCache::PkgIterator pkgCache::GrpIterator::PackageList() const
       {return PkgIterator(*Owner,Owner->PkgP + S->FirstPackage);}
       inline pkgCache::VerIterator pkgCache::GrpIterator::VersionsInSource() const
       {
	  return VerIterator(*Owner, Owner->VerP + S->VersionsInSource);
       }
inline pkgCache::VerIterator pkgCache::PkgIterator::VersionList() const
       {return VerIterator(*Owner,Owner->VerP + S->VersionList);}
inline pkgCache::VerIterator pkgCache::PkgIterator::CurrentVer() const
       {return VerIterator(*Owner,Owner->VerP + S->CurrentVer);}
inline pkgCache::DepIterator pkgCache::PkgIterator::RevDependsList() const
       {return DepIterator(*Owner,Owner->DepP + S->RevDepends,S);}
inline pkgCache::PrvIterator pkgCache::PkgIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + S->ProvidesList,S);}
inline pkgCache::DescIterator pkgCache::VerIterator::DescriptionList() const
       {return DescIterator(*Owner,Owner->DescP + S->DescriptionList);}
inline pkgCache::PrvIterator pkgCache::VerIterator::ProvidesList() const
       {return PrvIterator(*Owner,Owner->ProvideP + S->ProvidesList,S);}
inline pkgCache::DepIterator pkgCache::VerIterator::DependsList() const
       {return DepIterator(*Owner,Owner->DepP + S->DependsList,S);}
inline pkgCache::VerFileIterator pkgCache::VerIterator::FileList() const
       {return VerFileIterator(*Owner,Owner->VerFileP + S->FileList);}
inline pkgCache::DescFileIterator pkgCache::DescIterator::FileList() const
       {return DescFileIterator(*Owner,Owner->DescFileP + S->FileList);}
									/*}}}*/
#endif

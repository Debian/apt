// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \file cacheset.h
   Wrappers around std::set to have set::iterators which behave
   similar to the Iterators of the cache structures.

   Provides also a few helper methods which work with these sets */
									/*}}}*/
#ifndef APT_CACHESET_H
#define APT_CACHESET_H
// Include Files							/*{{{*/
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <set>
#include <string>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgcache.h>
									/*}}}*/
namespace APT {
class PackageSet;
class VersionSet;
class CacheSetHelper {							/*{{{*/
/** \class APT::CacheSetHelper
    Simple base class with a lot of virtual methods which can be overridden
    to alter the behavior or the output of the CacheSets.

    This helper is passed around by the static methods in the CacheSets and
    used every time they hit an error condition or something could be
    printed out.
*/
public:									/*{{{*/
	CacheSetHelper(bool const &ShowError = true,
		GlobalError::MsgType ErrorType = GlobalError::ERROR) :
			ShowError(ShowError), ErrorType(ErrorType) {};
	virtual ~CacheSetHelper() {};

	virtual void showTaskSelection(PackageSet const &pkgset, string const &pattern) {};
	virtual void showRegExSelection(PackageSet const &pkgset, string const &pattern) {};
	virtual void showSelectedVersion(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const Ver,
				 string const &ver, bool const &verIsRel) {};

	virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str);
	virtual PackageSet canNotFindTask(pkgCacheFile &Cache, std::string pattern);
	virtual PackageSet canNotFindRegEx(pkgCacheFile &Cache, std::string pattern);
	virtual PackageSet canNotFindPackage(pkgCacheFile &Cache, std::string const &str);
	virtual VersionSet canNotFindAllVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	virtual VersionSet canNotFindInstCandVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual VersionSet canNotFindCandInstVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual pkgCache::VerIterator canNotFindInstalledVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);

	bool showErrors() const { return ShowError; };
	bool showErrors(bool const &newValue) { if (ShowError == newValue) return ShowError; else return ((ShowError = newValue) == false); };
	GlobalError::MsgType errorType() const { return ErrorType; };
	GlobalError::MsgType errorType(GlobalError::MsgType const &newValue)
	{
		if (ErrorType == newValue) return ErrorType;
		else {
			GlobalError::MsgType const &oldValue = ErrorType;
			ErrorType = newValue;
			return oldValue;
		}
	};

									/*}}}*/
protected:
	bool ShowError;
	GlobalError::MsgType ErrorType;
};									/*}}}*/
class PackageSet : public std::set<pkgCache::PkgIterator> {		/*{{{*/
/** \class APT::PackageSet

    Simple wrapper around a std::set to provide a similar interface to
    a set of packages as to the complete set of all packages in the
    pkgCache. */
public:									/*{{{*/
	/** \brief smell like a pkgCache::PkgIterator */
	class const_iterator : public std::set<pkgCache::PkgIterator>::const_iterator {/*{{{*/
	public:
		const_iterator(std::set<pkgCache::PkgIterator>::const_iterator x) :
			 std::set<pkgCache::PkgIterator>::const_iterator(x) {}

		operator pkgCache::PkgIterator(void) { return **this; }

		inline const char *Name() const {return (**this).Name(); }
		inline std::string FullName(bool const &Pretty) const { return (**this).FullName(Pretty); }
		inline std::string FullName() const { return (**this).FullName(); }
		inline const char *Section() const {return (**this).Section(); }
		inline bool Purge() const {return (**this).Purge(); }
		inline const char *Arch() const {return (**this).Arch(); }
		inline pkgCache::GrpIterator Group() const { return (**this).Group(); }
		inline pkgCache::VerIterator VersionList() const { return (**this).VersionList(); }
		inline pkgCache::VerIterator CurrentVer() const { return (**this).CurrentVer(); }
		inline pkgCache::DepIterator RevDependsList() const { return (**this).RevDependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return (**this).ProvidesList(); }
		inline pkgCache::PkgIterator::OkState State() const { return (**this).State(); }
		inline const char *CandVersion() const { return (**this).CandVersion(); }
		inline const char *CurVersion() const { return (**this).CurVersion(); }
		inline pkgCache *Cache() const { return (**this).Cache(); };
		inline unsigned long Index() const {return (**this).Index();};
		// we have only valid iterators here
		inline bool end() const { return false; };

		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, (*i)); }

		inline pkgCache::Package const * operator->() const {
			return &***this;
		};
	};
	// 103. set::iterator is required to be modifiable, but this allows modification of keys
	typedef APT::PackageSet::const_iterator iterator;
									/*}}}*/

	using std::set<pkgCache::PkgIterator>::insert;
	inline void insert(pkgCache::PkgIterator const &P) { if (P.end() == false) std::set<pkgCache::PkgIterator>::insert(P); };
	inline void insert(PackageSet const &pkgset) { insert(pkgset.begin(), pkgset.end()); };

	/** \brief returns all packages in the cache who belong to the given task

	    A simple helper responsible for search for all members of a task
	    in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given task.
	    \param Cache the packages are in
	    \param pattern name of the task
	    \param helper responsible for error and message handling */
	static APT::PackageSet FromTask(pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper);
	static APT::PackageSet FromTask(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return APT::PackageSet::FromTask(Cache, pattern, helper);
	}

	/** \brief returns all packages in the cache whose name matchs a given pattern

	    A simple helper responsible for executing a regular expression on all
	    package names in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given package.
	    \param Cache the packages are in
	    \param pattern regular expression for package names
	    \param helper responsible for error and message handling */
	static APT::PackageSet FromRegEx(pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper);
	static APT::PackageSet FromRegEx(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return APT::PackageSet::FromRegEx(Cache, pattern, helper);
	}

	/** \brief returns all packages specified by a string

	    \param Cache the packages are in
	    \param string String the package name(s) should be extracted from
	    \param helper responsible for error and message handling */
	static APT::PackageSet FromString(pkgCacheFile &Cache, std::string const &string, CacheSetHelper &helper);
	static APT::PackageSet FromString(pkgCacheFile &Cache, std::string const &string) {
		CacheSetHelper helper;
		return APT::PackageSet::FromString(Cache, string, helper);
	}

	/** \brief returns a package specified by a string

	    \param Cache the package is in
	    \param string String the package name should be extracted from
	    \param helper responsible for error and message handling */
	static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &string, CacheSetHelper &helper);
	static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &string) {
		CacheSetHelper helper;
		return APT::PackageSet::FromName(Cache, string, helper);
	}

	/** \brief returns all packages specified on the commandline

	    Get all package names from the commandline and executes regex's if needed.
	    No special package command is supported, just plain names.
	    \param Cache the packages are in
	    \param cmdline Command line the package names should be extracted from
	    \param helper responsible for error and message handling */
	static APT::PackageSet FromCommandLine(pkgCacheFile &Cache, const char **cmdline, CacheSetHelper &helper);
	static APT::PackageSet FromCommandLine(pkgCacheFile &Cache, const char **cmdline) {
		CacheSetHelper helper;
		return APT::PackageSet::FromCommandLine(Cache, cmdline, helper);
	}

	struct Modifier {
		enum Position { NONE, PREFIX, POSTFIX };
		unsigned short ID;
		const char * const Alias;
		Position Pos;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos) : ID(id), Alias(alias), Pos(pos) {};
	};

	/** \brief group packages by a action modifiers

	    At some point it is needed to get from the same commandline
	    different package sets grouped by a modifier. Take
		apt-get install apt awesome-
	    as an example.
	    \param Cache the packages are in
	    \param cmdline Command line the package names should be extracted from
	    \param mods list of modifiers the method should accept
	    \param fallback the default modifier group for a package
	    \param helper responsible for error and message handling */
	static std::map<unsigned short, PackageSet> GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<PackageSet::Modifier> const &mods,
		unsigned short const &fallback, CacheSetHelper &helper);
	static std::map<unsigned short, PackageSet> GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<PackageSet::Modifier> const &mods,
		unsigned short const &fallback) {
		CacheSetHelper helper;
		return APT::PackageSet::GroupedFromCommandLine(Cache, cmdline,
				mods, fallback, helper);
	}

	enum Constructor { UNKNOWN, REGEX, TASK };
	Constructor getConstructor() const { return ConstructedBy; };

	PackageSet() : ConstructedBy(UNKNOWN) {};
	PackageSet(Constructor const &by) : ConstructedBy(by) {};
									/*}}}*/
private:								/*{{{*/
	Constructor ConstructedBy;
									/*}}}*/
};									/*}}}*/
class VersionSet : public std::set<pkgCache::VerIterator> {		/*{{{*/
/** \class APT::VersionSet

    Simple wrapper around a std::set to provide a similar interface to
    a set of versions as to the complete set of all versions in the
    pkgCache. */
public:									/*{{{*/
	/** \brief smell like a pkgCache::VerIterator */
	class const_iterator : public std::set<pkgCache::VerIterator>::const_iterator {/*{{{*/
	public:
		const_iterator(std::set<pkgCache::VerIterator>::const_iterator x) :
			 std::set<pkgCache::VerIterator>::const_iterator(x) {}

		operator pkgCache::VerIterator(void) { return **this; }

		inline pkgCache *Cache() const { return (**this).Cache(); };
		inline unsigned long Index() const {return (**this).Index();};
		// we have only valid iterators here
		inline bool end() const { return false; };

		inline pkgCache::Version const * operator->() const {
			return &***this;
		};

		inline int CompareVer(const pkgCache::VerIterator &B) const { return (**this).CompareVer(B); };
		inline const char *VerStr() const { return (**this).VerStr(); };
		inline const char *Section() const { return (**this).Section(); };
		inline const char *Arch() const { return (**this).Arch(); };
		inline const char *Arch(bool const pseudo) const { return (**this).Arch(pseudo); };
		inline pkgCache::PkgIterator ParentPkg() const { return (**this).ParentPkg(); };
		inline pkgCache::DescIterator DescriptionList() const { return (**this).DescriptionList(); };
		inline pkgCache::DescIterator TranslatedDescription() const { return (**this).TranslatedDescription(); };
		inline pkgCache::DepIterator DependsList() const { return (**this).DependsList(); };
		inline pkgCache::PrvIterator ProvidesList() const { return (**this).ProvidesList(); };
		inline pkgCache::VerFileIterator FileList() const { return (**this).FileList(); };
		inline bool Downloadable() const { return (**this).Downloadable(); };
		inline const char *PriorityType() const { return (**this).PriorityType(); };
		inline string RelStr() const { return (**this).RelStr(); };
		inline bool Automatic() const { return (**this).Automatic(); };
		inline bool Pseudo() const { return (**this).Pseudo(); };
		inline pkgCache::VerFileIterator NewestFile() const { return (**this).NewestFile(); };
	};
									/*}}}*/
	// 103. set::iterator is required to be modifiable, but this allows modification of keys
	typedef APT::VersionSet::const_iterator iterator;

	using std::set<pkgCache::VerIterator>::insert;
	inline void insert(pkgCache::VerIterator const &V) { if (V.end() == false) std::set<pkgCache::VerIterator>::insert(V); };
	inline void insert(VersionSet const &verset) { insert(verset.begin(), verset.end()); };

	/** \brief specifies which version(s) will be returned if non is given */
	enum Version {
		/** All versions */
		ALL,
		/** Candidate and installed version */
		CANDANDINST,
		/** Candidate version */
		CANDIDATE,
		/** Installed version */
		INSTALLED,
		/** Candidate or if non installed version */
		CANDINST,
		/** Installed or if non candidate version */
		INSTCAND,
		/** Newest version */
		NEWEST
	};

	/** \brief returns all versions specified on the commandline

	    Get all versions from the commandline, uses given default version if
	    non specifically requested  and executes regex's if needed on names.
	    \param Cache the packages and versions are in
	    \param cmdline Command line the versions should be extracted from
	    \param helper responsible for error and message handling */
	static APT::VersionSet FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			APT::VersionSet::Version const &fallback, CacheSetHelper &helper);
	static APT::VersionSet FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			APT::VersionSet::Version const &fallback) {
		CacheSetHelper helper;
		return APT::VersionSet::FromCommandLine(Cache, cmdline, fallback, helper);
	}
	static APT::VersionSet FromCommandLine(pkgCacheFile &Cache, const char **cmdline) {
		return APT::VersionSet::FromCommandLine(Cache, cmdline, CANDINST);
	}

	static APT::VersionSet FromString(pkgCacheFile &Cache, std::string pkg,
			APT::VersionSet::Version const &fallback, CacheSetHelper &helper,
			bool const &onlyFromName = false);
	static APT::VersionSet FromString(pkgCacheFile &Cache, std::string pkg,
			APT::VersionSet::Version const &fallback) {
		CacheSetHelper helper;
		return APT::VersionSet::FromString(Cache, pkg, fallback, helper);
	}
	static APT::VersionSet FromString(pkgCacheFile &Cache, std::string pkg) {
		return APT::VersionSet::FromString(Cache, pkg, CANDINST);
	}

	/** \brief returns all versions specified for the package

	    \param Cache the package and versions are in
	    \param P the package in question
	    \param fallback the version(s) you want to get
	    \param helper the helper used for display and error handling */
	static APT::VersionSet FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
		VersionSet::Version const &fallback, CacheSetHelper &helper);
	static APT::VersionSet FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
			APT::VersionSet::Version const &fallback) {
		CacheSetHelper helper;
		return APT::VersionSet::FromPackage(Cache, P, fallback, helper);
	}
	static APT::VersionSet FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P) {
		return APT::VersionSet::FromPackage(Cache, P, CANDINST);
	}

	struct Modifier {
		enum Position { NONE, PREFIX, POSTFIX };
		unsigned short ID;
		const char * const Alias;
		Position Pos;
		VersionSet::Version SelectVersion;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos,
			  VersionSet::Version const &select) : ID(id), Alias(alias), Pos(pos),
			 SelectVersion(select) {};
	};

	static std::map<unsigned short, VersionSet> GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<VersionSet::Modifier> const &mods,
		unsigned short const &fallback, CacheSetHelper &helper);
	static std::map<unsigned short, VersionSet> GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<VersionSet::Modifier> const &mods,
		unsigned short const &fallback) {
		CacheSetHelper helper;
		return APT::VersionSet::GroupedFromCommandLine(Cache, cmdline,
				mods, fallback, helper);
	}
									/*}}}*/
protected:								/*{{{*/

	/** \brief returns the candidate version of the package

	    \param Cache to be used to query for information
	    \param Pkg we want the candidate version from this package */
	static pkgCache::VerIterator getCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper);

	/** \brief returns the installed version of the package

	    \param Cache to be used to query for information
	    \param Pkg we want the installed version from this package */
	static pkgCache::VerIterator getInstalledVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper);
									/*}}}*/
};									/*}}}*/
}
#endif

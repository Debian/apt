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
#include <list>
#include <string>
#include <iterator>

#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/cachefile.h>
#endif
									/*}}}*/

class pkgCacheFile;

namespace APT {
class PackageContainerInterface;
class VersionContainerInterface;

class CacheSetHelper {							/*{{{*/
/** \class APT::CacheSetHelper
    Simple base class with a lot of virtual methods which can be overridden
    to alter the behavior or the output of the CacheSets.

    This helper is passed around by the static methods in the CacheSets and
    used every time they hit an error condition or something could be
    printed out.
*/
public:									/*{{{*/
	CacheSetHelper(bool const ShowError = true,
		GlobalError::MsgType ErrorType = GlobalError::ERROR) :
			ShowError(ShowError), ErrorType(ErrorType) {};
	virtual ~CacheSetHelper() {};

	virtual void showTaskSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	virtual void showRegExSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	virtual void showSelectedVersion(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const Ver,
				 std::string const &ver, bool const verIsRel);

	virtual void canNotFindTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	virtual void canNotFindRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	virtual void canNotFindPackage(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &str);

	virtual void canNotFindAllVer(VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	virtual void canNotFindInstCandVer(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual void canNotFindCandInstVer(VersionContainerInterface * const vci,
				pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);

	virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str);
	virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	virtual pkgCache::VerIterator canNotFindInstalledVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);

	bool showErrors() const { return ShowError; };
	bool showErrors(bool const newValue) { if (ShowError == newValue) return ShowError; else return ((ShowError = newValue) == false); };
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
class PackageContainerInterface {					/*{{{*/
/** \class PackageContainerInterface

 * Interface ensuring that all operations can be executed on the yet to
 * define concrete PackageContainer - access to all methods is possible,
 * but in general the wrappers provided by the PackageContainer template
 * are nicer to use.

 * This class mostly protects use from the need to write all implementation
 * of the methods working on containers in the template */
public:
	class const_iterator {						/*{{{*/
	public:
		virtual pkgCache::PkgIterator getPkg() const = 0;
		operator pkgCache::PkgIterator(void) const { return getPkg(); }

		inline const char *Name() const {return getPkg().Name(); }
		inline std::string FullName(bool const Pretty) const { return getPkg().FullName(Pretty); }
		inline std::string FullName() const { return getPkg().FullName(); }
		inline const char *Section() const {return getPkg().Section(); }
		inline bool Purge() const {return getPkg().Purge(); }
		inline const char *Arch() const {return getPkg().Arch(); }
		inline pkgCache::GrpIterator Group() const { return getPkg().Group(); }
		inline pkgCache::VerIterator VersionList() const { return getPkg().VersionList(); }
		inline pkgCache::VerIterator CurrentVer() const { return getPkg().CurrentVer(); }
		inline pkgCache::DepIterator RevDependsList() const { return getPkg().RevDependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return getPkg().ProvidesList(); }
		inline pkgCache::PkgIterator::OkState State() const { return getPkg().State(); }
		inline const char *CandVersion() const { return getPkg().CandVersion(); }
		inline const char *CurVersion() const { return getPkg().CurVersion(); }
		inline pkgCache *Cache() const { return getPkg().Cache(); };
		inline unsigned long Index() const {return getPkg().Index();};
		// we have only valid iterators here
		inline bool end() const { return false; };

		inline pkgCache::Package const * operator->() const {return &*getPkg();};
	};
									/*}}}*/

	virtual bool insert(pkgCache::PkgIterator const &P) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;

	enum Constructor { UNKNOWN, REGEX, TASK };
	virtual void setConstructor(Constructor const &con) = 0;
	virtual Constructor getConstructor() const = 0;

	static bool FromTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper);
	static bool FromRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper);
	static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper);
	static bool FromGroup(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper);
	static bool FromString(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper);
	static bool FromCommandLine(PackageContainerInterface * const pci, pkgCacheFile &Cache, const char **cmdline, CacheSetHelper &helper);

	struct Modifier {
		enum Position { NONE, PREFIX, POSTFIX };
		unsigned short ID;
		const char * const Alias;
		Position Pos;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos) : ID(id), Alias(alias), Pos(pos) {};
	};

	static bool FromModifierCommandLine(unsigned short &modID, PackageContainerInterface * const pci,
					    pkgCacheFile &Cache, const char * cmdline,
					    std::list<Modifier> const &mods, CacheSetHelper &helper);
};
									/*}}}*/
template<class Container> class PackageContainer : public PackageContainerInterface {/*{{{*/
/** \class APT::PackageContainer

    Simple wrapper around a container class like std::set to provide a similar
    interface to a set of packages as to the complete set of all packages in the
    pkgCache. */
	Container _cont;
public:									/*{{{*/
	/** \brief smell like a pkgCache::PkgIterator */
	class const_iterator : public PackageContainerInterface::const_iterator,/*{{{*/
			       public std::iterator<std::forward_iterator_tag, typename Container::const_iterator> {
		typename Container::const_iterator _iter;
	public:
		const_iterator(typename Container::const_iterator i) : _iter(i) {}
		pkgCache::PkgIterator getPkg(void) const { return *_iter; }
		inline pkgCache::PkgIterator operator*(void) const { return *_iter; };
		operator typename Container::const_iterator(void) const { return _iter; }
		inline const_iterator& operator++() { ++_iter; return *this; }
		inline const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(const_iterator const &i) const { return _iter != i._iter; };
		inline bool operator==(const_iterator const &i) const { return _iter == i._iter; };
		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, *i); }
	};
	class iterator : public PackageContainerInterface::const_iterator,
			 public std::iterator<std::forward_iterator_tag, typename Container::iterator> {
		typename Container::iterator _iter;
	public:
		iterator(typename Container::iterator i) : _iter(i) {}
		pkgCache::PkgIterator getPkg(void) const { return *_iter; }
		inline pkgCache::PkgIterator operator*(void) const { return *_iter; };
		operator typename Container::iterator(void) const { return _iter; }
		operator typename PackageContainer<Container>::const_iterator() { return typename PackageContainer<Container>::const_iterator(_iter); }
		inline iterator& operator++() { ++_iter; return *this; }
		inline iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(iterator const &i) const { return _iter != i._iter; };
		inline bool operator==(iterator const &i) const { return _iter == i._iter; };
		inline iterator& operator=(iterator const &i) { _iter = i._iter; return *this; };
		inline iterator& operator=(typename Container::iterator const &i) { _iter = i; return *this; };
		friend std::ostream& operator<<(std::ostream& out, iterator i) { return operator<<(out, *i); }
	};
									/*}}}*/

	bool insert(pkgCache::PkgIterator const &P) { if (P.end() == true) return false; _cont.insert(P); return true; };
	template<class Cont> void insert(PackageContainer<Cont> const &pkgcont) { _cont.insert((typename Cont::const_iterator)pkgcont.begin(), (typename Cont::const_iterator)pkgcont.end()); };
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); };

	bool empty() const { return _cont.empty(); };
	void clear() { return _cont.clear(); };
	//FIXME: on ABI break, replace the first with the second without bool
	void erase(iterator position) { _cont.erase((typename Container::iterator)position); };
	iterator& erase(iterator &position, bool) { return position = _cont.erase((typename Container::iterator)position); };
	size_t erase(const pkgCache::PkgIterator x) { return _cont.erase(x); };
	void erase(iterator first, iterator last) { _cont.erase(first, last); };
	size_t size() const { return _cont.size(); };

	const_iterator begin() const { return const_iterator(_cont.begin()); };
	const_iterator end() const { return const_iterator(_cont.end()); };
	iterator begin() { return iterator(_cont.begin()); };
	iterator end() { return iterator(_cont.end()); };
	const_iterator find(pkgCache::PkgIterator const &P) const { return const_iterator(_cont.find(P)); };

	void setConstructor(Constructor const &by) { ConstructedBy = by; };
	Constructor getConstructor() const { return ConstructedBy; };

	PackageContainer() : ConstructedBy(UNKNOWN) {};
	PackageContainer(Constructor const &by) : ConstructedBy(by) {};

	/** \brief returns all packages in the cache who belong to the given task

	    A simple helper responsible for search for all members of a task
	    in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given task.
	    \param Cache the packages are in
	    \param pattern name of the task
	    \param helper responsible for error and message handling */
	static PackageContainer FromTask(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont(TASK);
		PackageContainerInterface::FromTask(&cont, Cache, pattern, helper);
		return cont;
	}
	static PackageContainer FromTask(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromTask(Cache, pattern, helper);
	}

	/** \brief returns all packages in the cache whose name matchs a given pattern

	    A simple helper responsible for executing a regular expression on all
	    package names in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given package.
	    \param Cache the packages are in
	    \param pattern regular expression for package names
	    \param helper responsible for error and message handling */
	static PackageContainer FromRegEx(pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
		PackageContainer cont(REGEX);
		PackageContainerInterface::FromRegEx(&cont, Cache, pattern, helper);
		return cont;
	}

	static PackageContainer FromRegEx(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromRegEx(Cache, pattern, helper);
	}

	/** \brief returns a package specified by a string

	    \param Cache the package is in
	    \param pattern String the package name should be extracted from
	    \param helper responsible for error and message handling */
	static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		return PackageContainerInterface::FromName(Cache, pattern, helper);
	}
	static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return PackageContainerInterface::FromName(Cache, pattern, helper);
	}

	/** \brief returns all packages specified by a string

	    \param Cache the packages are in
	    \param pattern String the package name(s) should be extracted from
	    \param helper responsible for error and message handling */
	static PackageContainer FromString(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont;
		PackageContainerInterface::FromString(&cont, Cache, pattern, helper);
		return cont;
	}
	static PackageContainer FromString(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromString(Cache, pattern, helper);
	}

	/** \brief returns all packages specified on the commandline

	    Get all package names from the commandline and executes regex's if needed.
	    No special package command is supported, just plain names.
	    \param Cache the packages are in
	    \param cmdline Command line the package names should be extracted from
	    \param helper responsible for error and message handling */
	static PackageContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline, CacheSetHelper &helper) {
		PackageContainer cont;
		PackageContainerInterface::FromCommandLine(&cont, Cache, cmdline, helper);
		return cont;
	}
	static PackageContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline) {
		CacheSetHelper helper;
		return FromCommandLine(Cache, cmdline, helper);
	}

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
	static std::map<unsigned short, PackageContainer> GroupedFromCommandLine(
										 pkgCacheFile &Cache,
										 const char **cmdline,
										 std::list<Modifier> const &mods,
										 unsigned short const &fallback,
										 CacheSetHelper &helper) {
		std::map<unsigned short, PackageContainer> pkgsets;
		for (const char **I = cmdline; *I != 0; ++I) {
			unsigned short modID = fallback;
			PackageContainer pkgset;
			PackageContainerInterface::FromModifierCommandLine(modID, &pkgset, Cache, *I, mods, helper);
			pkgsets[modID].insert(pkgset);
		}
		return pkgsets;
	}
	static std::map<unsigned short, PackageContainer> GroupedFromCommandLine(
										 pkgCacheFile &Cache,
										 const char **cmdline,
										 std::list<Modifier> const &mods,
										 unsigned short const &fallback) {
		CacheSetHelper helper;
		return GroupedFromCommandLine(Cache, cmdline,
				mods, fallback, helper);
	}
									/*}}}*/
private:								/*{{{*/
	Constructor ConstructedBy;
									/*}}}*/
};									/*}}}*/

template<> template<class Cont> void PackageContainer<std::list<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
};
// these two are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool PackageContainer<std::list<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
	if (P.end() == true)
		return false;
	_cont.push_back(P);
	return true;
};
template<> inline void PackageContainer<std::list<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_back(*p);
};
typedef PackageContainer<std::set<pkgCache::PkgIterator> > PackageSet;
typedef PackageContainer<std::list<pkgCache::PkgIterator> > PackageList;

class VersionContainerInterface {					/*{{{*/
/** \class APT::VersionContainerInterface

    Same as APT::PackageContainerInterface, just for Versions */
public:
	/** \brief smell like a pkgCache::VerIterator */
	class const_iterator {						/*{{{*/
	public:
		virtual pkgCache::VerIterator getVer() const = 0;
		operator pkgCache::VerIterator(void) { return getVer(); }

		inline pkgCache *Cache() const { return getVer().Cache(); };
		inline unsigned long Index() const {return getVer().Index();};
		inline int CompareVer(const pkgCache::VerIterator &B) const { return getVer().CompareVer(B); };
		inline const char *VerStr() const { return getVer().VerStr(); };
		inline const char *Section() const { return getVer().Section(); };
		inline const char *Arch() const { return getVer().Arch(); };
		inline pkgCache::PkgIterator ParentPkg() const { return getVer().ParentPkg(); };
		inline pkgCache::DescIterator DescriptionList() const { return getVer().DescriptionList(); };
		inline pkgCache::DescIterator TranslatedDescription() const { return getVer().TranslatedDescription(); };
		inline pkgCache::DepIterator DependsList() const { return getVer().DependsList(); };
		inline pkgCache::PrvIterator ProvidesList() const { return getVer().ProvidesList(); };
		inline pkgCache::VerFileIterator FileList() const { return getVer().FileList(); };
		inline bool Downloadable() const { return getVer().Downloadable(); };
		inline const char *PriorityType() const { return getVer().PriorityType(); };
		inline std::string RelStr() const { return getVer().RelStr(); };
		inline bool Automatic() const { return getVer().Automatic(); };
		inline pkgCache::VerFileIterator NewestFile() const { return getVer().NewestFile(); };
		// we have only valid iterators here
		inline bool end() const { return false; };

		inline pkgCache::Version const * operator->() const { return &*getVer(); };
	};
									/*}}}*/

	virtual bool insert(pkgCache::VerIterator const &V) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;

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

	struct Modifier {
		enum Position { NONE, PREFIX, POSTFIX };
		unsigned short ID;
		const char * const Alias;
		Position Pos;
		Version SelectVersion;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos,
			  Version const &select) : ID(id), Alias(alias), Pos(pos),
			 SelectVersion(select) {};
	};

	static bool FromCommandLine(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				    const char **cmdline, Version const &fallback,
				    CacheSetHelper &helper);

	static bool FromString(VersionContainerInterface * const vci, pkgCacheFile &Cache,
			       std::string pkg, Version const &fallback, CacheSetHelper &helper,
			       bool const onlyFromName = false);

	static bool FromPackage(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &P, Version const &fallback,
				CacheSetHelper &helper);

	static bool FromModifierCommandLine(unsigned short &modID,
					    VersionContainerInterface * const vci,
					    pkgCacheFile &Cache, const char * cmdline,
					    std::list<Modifier> const &mods,
					    CacheSetHelper &helper);


	static bool FromDependency(VersionContainerInterface * const vci,
				   pkgCacheFile &Cache,
				   pkgCache::DepIterator const &D,
				   Version const &selector,
				   CacheSetHelper &helper);

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
};
									/*}}}*/
template<class Container> class VersionContainer : public VersionContainerInterface {/*{{{*/
/** \class APT::VersionContainer

    Simple wrapper around a container class like std::set to provide a similar
    interface to a set of versions as to the complete set of all versions in the
    pkgCache. */
	Container _cont;
public:									/*{{{*/
	/** \brief smell like a pkgCache::VerIterator */
	class const_iterator : public VersionContainerInterface::const_iterator,
			       public std::iterator<std::forward_iterator_tag, typename Container::const_iterator> {/*{{{*/
		typename Container::const_iterator _iter;
	public:
		const_iterator(typename Container::const_iterator i) : _iter(i) {}
		pkgCache::VerIterator getVer(void) const { return *_iter; }
		inline pkgCache::VerIterator operator*(void) const { return *_iter; };
		operator typename Container::const_iterator(void) const { return _iter; }
		inline const_iterator& operator++() { ++_iter; return *this; }
		inline const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(const_iterator const &i) const { return _iter != i._iter; };
		inline bool operator==(const_iterator const &i) const { return _iter == i._iter; };
		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, *i); }
	};
	class iterator : public VersionContainerInterface::const_iterator,
			 public std::iterator<std::forward_iterator_tag, typename Container::iterator> {
		typename Container::iterator _iter;
	public:
		iterator(typename Container::iterator i) : _iter(i) {}
		pkgCache::VerIterator getVer(void) const { return *_iter; }
		inline pkgCache::VerIterator operator*(void) const { return *_iter; };
		operator typename Container::iterator(void) const { return _iter; }
		operator typename VersionContainer<Container>::const_iterator() { return typename VersionContainer<Container>::const_iterator(_iter); }
		inline iterator& operator++() { ++_iter; return *this; }
		inline iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(iterator const &i) const { return _iter != i._iter; };
		inline bool operator==(iterator const &i) const { return _iter == i._iter; };
		inline iterator& operator=(iterator const &i) { _iter = i._iter; return *this; };
		inline iterator& operator=(typename Container::iterator const &i) { _iter = i; return *this; };
		friend std::ostream& operator<<(std::ostream& out, iterator i) { return operator<<(out, *i); }
	};
									/*}}}*/

	bool insert(pkgCache::VerIterator const &V) { if (V.end() == true) return false; _cont.insert(V); return true; };
	template<class Cont> void insert(VersionContainer<Cont> const &vercont) { _cont.insert((typename Cont::const_iterator)vercont.begin(), (typename Cont::const_iterator)vercont.end()); };
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); };
	bool empty() const { return _cont.empty(); };
	void clear() { return _cont.clear(); };
	//FIXME: on ABI break, replace the first with the second without bool
	void erase(iterator position) { _cont.erase((typename Container::iterator)position); };
	iterator& erase(iterator &position, bool) { return position = _cont.erase((typename Container::iterator)position); };
	size_t erase(const pkgCache::VerIterator x) { return _cont.erase(x); };
	void erase(iterator first, iterator last) { _cont.erase(first, last); };
	size_t size() const { return _cont.size(); };

	const_iterator begin() const { return const_iterator(_cont.begin()); };
	const_iterator end() const { return const_iterator(_cont.end()); };
	iterator begin() { return iterator(_cont.begin()); };
	iterator end() { return iterator(_cont.end()); };
	const_iterator find(pkgCache::VerIterator const &V) const { return const_iterator(_cont.find(V)); };

	/** \brief returns all versions specified on the commandline

	    Get all versions from the commandline, uses given default version if
	    non specifically requested  and executes regex's if needed on names.
	    \param Cache the packages and versions are in
	    \param cmdline Command line the versions should be extracted from
	    \param helper responsible for error and message handling */
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			Version const &fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromCommandLine(&vercon, Cache, cmdline, fallback, helper);
		return vercon;
	}
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			Version const &fallback) {
		CacheSetHelper helper;
		return FromCommandLine(Cache, cmdline, fallback, helper);
	}
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline) {
		return FromCommandLine(Cache, cmdline, CANDINST);
	}

	static VersionContainer FromString(pkgCacheFile &Cache, std::string const &pkg,
			Version const &fallback, CacheSetHelper &helper,
			bool const onlyFromName = false) {
		VersionContainer vercon;
		VersionContainerInterface::FromString(&vercon, Cache, pkg, fallback, helper);
		return vercon;
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string pkg,
			Version const &fallback) {
		CacheSetHelper helper;
		return FromString(Cache, pkg, fallback, helper);
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string pkg) {
		return FromString(Cache, pkg, CANDINST);
	}

	/** \brief returns all versions specified for the package

	    \param Cache the package and versions are in
	    \param P the package in question
	    \param fallback the version(s) you want to get
	    \param helper the helper used for display and error handling */
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
		Version const &fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromPackage(&vercon, Cache, P, fallback, helper);
		return vercon;
	}
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
					    Version const &fallback) {
		CacheSetHelper helper;
		return FromPackage(Cache, P, fallback, helper);
	}
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P) {
		return FromPackage(Cache, P, CANDIDATE);
	}

	static std::map<unsigned short, VersionContainer> GroupedFromCommandLine(
										 pkgCacheFile &Cache,
										 const char **cmdline,
										 std::list<Modifier> const &mods,
										 unsigned short const fallback,
										 CacheSetHelper &helper) {
		std::map<unsigned short, VersionContainer> versets;
		for (const char **I = cmdline; *I != 0; ++I) {
			unsigned short modID = fallback;
			VersionContainer verset;
			VersionContainerInterface::FromModifierCommandLine(modID, &verset, Cache, *I, mods, helper);
			versets[modID].insert(verset);
		}
		return versets;

	}
	static std::map<unsigned short, VersionContainer> GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<Modifier> const &mods,
		unsigned short const fallback) {
		CacheSetHelper helper;
		return GroupedFromCommandLine(Cache, cmdline,
				mods, fallback, helper);
	}

	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D,
					       Version const &selector, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromDependency(&vercon, Cache, D, selector, helper);
		return vercon;
	}
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D,
					       Version const &selector) {
		CacheSetHelper helper;
		return FromPackage(Cache, D, selector, helper);
	}
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D) {
		return FromPackage(Cache, D, CANDIDATE);
	}
									/*}}}*/
};									/*}}}*/

template<> template<class Cont> void VersionContainer<std::list<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
};
// these two are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool VersionContainer<std::list<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
	if (V.end() == true)
		return false;
	_cont.push_back(V);
	return true;
};
template<> inline void VersionContainer<std::list<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_back(*v);
};
typedef VersionContainer<std::set<pkgCache::VerIterator> > VersionSet;
typedef VersionContainer<std::list<pkgCache::VerIterator> > VersionList;
}
#endif

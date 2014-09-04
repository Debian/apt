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
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <string>
#include <iterator>
#include <algorithm>

#include <stddef.h>

#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/cachefile.h>
#endif
#ifndef APT_10_CLEANER_HEADERS
#include <iostream>
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
			ShowError(ShowError), ErrorType(ErrorType) {}
	virtual ~CacheSetHelper() {}

	enum PkgSelector { UNKNOWN, REGEX, TASK, FNMATCH, PACKAGENAME, STRING };

	virtual bool PackageFrom(enum PkgSelector const select, PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern);

	virtual bool PackageFromCommandLine(PackageContainerInterface * const pci, pkgCacheFile &Cache, const char **cmdline);

	struct PkgModifier {
		enum Position { NONE, PREFIX, POSTFIX };
		unsigned short ID;
		const char * const Alias;
		Position Pos;
		PkgModifier (unsigned short const &id, const char * const alias, Position const &pos) : ID(id), Alias(alias), Pos(pos) {}
	};
	virtual bool PackageFromModifierCommandLine(unsigned short &modID, PackageContainerInterface * const pci,
					    pkgCacheFile &Cache, const char * cmdline,
					    std::list<PkgModifier> const &mods);

	// use PackageFrom(PACKAGENAME, …) instead
	APT_DEPRECATED pkgCache::PkgIterator PackageFromName(pkgCacheFile &Cache, std::string const &pattern);

	/** \brief be notified about the package being selected via pattern
	 *
	 * Main use is probably to show a message to the user what happened
	 *
	 * \param pkg is the package which was selected
	 * \param select is the selection method which choose the package
	 * \param pattern is the string used by the selection method to pick the package
	 */
	virtual void showPackageSelection(pkgCache::PkgIterator const &pkg, PkgSelector const select, std::string const &pattern);
	// use the method above instead, react only on the type you need and let the base handle the rest if need be
	// this allows use to add new selection methods without breaking the ABI constantly with new virtual methods
	APT_DEPRECATED virtual void showTaskSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	APT_DEPRECATED virtual void showRegExSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	APT_DEPRECATED virtual void showFnmatchSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);

	/** \brief be notified if a package can't be found via pattern
	 *
	 * Can be used to show a message as well as to try something else to make it match
	 *
	 * \param select is the method tried for selection
	 * \param pci is the container the package should be inserted in
	 * \param Cache is the package universe available
	 * \param pattern is the string not matching anything
	 */
	virtual void canNotFindPackage(enum PkgSelector const select, PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern);
	// same as above for showPackageSelection
	APT_DEPRECATED virtual void canNotFindTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	APT_DEPRECATED virtual void canNotFindRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	APT_DEPRECATED virtual void canNotFindFnmatch(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	APT_DEPRECATED virtual void canNotFindPackage(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &str);

	/** \brief specifies which version(s) we want to refer to */
	enum VerSelector {
		/** by release string */
		RELEASE,
		/** by version number string */
		VERSIONNUMBER,
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

	/** \brief be notified about the version being selected via pattern
	 *
	 * Main use is probably to show a message to the user what happened
	 * Note that at the moment this method is only called for RELEASE
	 * and VERSION selections, not for the others.
	 *
	 * \param Pkg is the package which was selected for
	 * \param Ver is the version selected
	 * \param select is the selection method which choose the version
	 * \param pattern is the string used by the selection method to pick the version
	 */
	virtual void showVersionSelection(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const &Ver,
	      enum VerSelector const select, std::string const &pattern);
	// renamed to have a similar interface to showPackageSelection
	APT_DEPRECATED virtual void showSelectedVersion(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const Ver,
				 std::string const &ver, bool const verIsRel);

	/** \brief be notified if a version can't be found for a package
	 *
	 * Main use is probably to show a message to the user what happened
	 *
	 * \param select is the method tried for selection
	 * \param vci is the container the version should be inserted in
	 * \param Cache is the package universe available
	 * \param Pkg is the package we wanted a version from
	 */
	virtual void canNotFindVersion(enum VerSelector const select, VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	// same as above for showPackageSelection
	APT_DEPRECATED virtual void canNotFindAllVer(VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	APT_DEPRECATED virtual void canNotFindInstCandVer(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	APT_DEPRECATED virtual void canNotFindCandInstVer(VersionContainerInterface * const vci,
				pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);

	// the difference between canNotFind and canNotGet is that the later is more low-level
	// and called from other places: In this case looking into the code is the only real answer…
	virtual pkgCache::VerIterator canNotGetVersion(enum VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	// same as above for showPackageSelection
	APT_DEPRECATED virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	APT_DEPRECATED virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);
	APT_DEPRECATED virtual pkgCache::VerIterator canNotFindInstalledVer(pkgCacheFile &Cache,
				pkgCache::PkgIterator const &Pkg);

	virtual pkgCache::PkgIterator canNotFindPkgName(pkgCacheFile &Cache, std::string const &str);

	bool showErrors() const { return ShowError; }
	bool showErrors(bool const newValue) { if (ShowError == newValue) return ShowError; else return ((ShowError = newValue) == false); }
	GlobalError::MsgType errorType() const { return ErrorType; }
	GlobalError::MsgType errorType(GlobalError::MsgType const &newValue)
	{
		if (ErrorType == newValue) return ErrorType;
		else {
			GlobalError::MsgType const &oldValue = ErrorType;
			ErrorType = newValue;
			return oldValue;
		}
	}

									/*}}}*/
protected:
	bool ShowError;
	GlobalError::MsgType ErrorType;

	pkgCache::VerIterator canNotGetInstCandVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotGetCandInstVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg);

	bool PackageFromTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromFnmatch(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromPackageName(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromString(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern);
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
		APT_DEPRECATED inline const char *Section() const {
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	   return getPkg().Section();
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
		}
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
		inline pkgCache *Cache() const { return getPkg().Cache(); }
		inline unsigned long Index() const {return getPkg().Index();}
		// we have only valid iterators here
		inline bool end() const { return false; }

		inline pkgCache::Package const * operator->() const {return &*getPkg();}
	};
									/*}}}*/

	virtual bool insert(pkgCache::PkgIterator const &P) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;

	// FIXME: This is a bloody hack removed soon. Use CacheSetHelper::PkgSelector !
	enum APT_DEPRECATED Constructor { UNKNOWN = CacheSetHelper::UNKNOWN,
		REGEX = CacheSetHelper::REGEX,
		TASK = CacheSetHelper::TASK,
		FNMATCH = CacheSetHelper::FNMATCH };
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	void setConstructor(Constructor const by) { ConstructedBy = (CacheSetHelper::PkgSelector)by; }
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	void setConstructor(CacheSetHelper::PkgSelector const by) { ConstructedBy = by; }
	CacheSetHelper::PkgSelector getConstructor() const { return ConstructedBy; }
	PackageContainerInterface() : ConstructedBy(CacheSetHelper::UNKNOWN) {}
	PackageContainerInterface(CacheSetHelper::PkgSelector const by) : ConstructedBy(by) {}

	APT_DEPRECATED static bool FromTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	   return helper.PackageFrom(CacheSetHelper::TASK, pci, Cache, pattern); }
	APT_DEPRECATED static bool FromRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	   return helper.PackageFrom(CacheSetHelper::REGEX, pci, Cache, pattern); }
	APT_DEPRECATED static bool FromFnmatch(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	   return helper.PackageFrom(CacheSetHelper::FNMATCH, pci, Cache, pattern); }
	APT_DEPRECATED static bool FromGroup(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	   return helper.PackageFrom(CacheSetHelper::PACKAGENAME, pci, Cache, pattern); }
	APT_DEPRECATED static bool FromString(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
	   return helper.PackageFrom(CacheSetHelper::STRING, pci, Cache, pattern); }
	APT_DEPRECATED static bool FromCommandLine(PackageContainerInterface * const pci, pkgCacheFile &Cache, const char **cmdline, CacheSetHelper &helper) {
	   return helper.PackageFromCommandLine(pci, Cache, cmdline); }

	APT_DEPRECATED typedef CacheSetHelper::PkgModifier Modifier;

#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
	   return helper.PackageFromName(Cache, pattern); }
	APT_DEPRECATED static bool FromModifierCommandLine(unsigned short &modID, PackageContainerInterface * const pci,
	      pkgCacheFile &Cache, const char * cmdline,
	      std::list<Modifier> const &mods, CacheSetHelper &helper) {
	   return helper.PackageFromModifierCommandLine(modID, pci, Cache, cmdline, mods); }
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

private:
	CacheSetHelper::PkgSelector ConstructedBy;
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
		inline pkgCache::PkgIterator operator*(void) const { return *_iter; }
		operator typename Container::const_iterator(void) const { return _iter; }
		inline const_iterator& operator++() { ++_iter; return *this; }
		inline const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(const_iterator const &i) const { return _iter != i._iter; }
		inline bool operator==(const_iterator const &i) const { return _iter == i._iter; }
		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, *i); }
	};
	class iterator : public PackageContainerInterface::const_iterator,
			 public std::iterator<std::forward_iterator_tag, typename Container::iterator> {
		typename Container::iterator _iter;
	public:
		iterator(typename Container::iterator i) : _iter(i) {}
		pkgCache::PkgIterator getPkg(void) const { return *_iter; }
		inline pkgCache::PkgIterator operator*(void) const { return *_iter; }
		operator typename Container::iterator(void) const { return _iter; }
		operator typename PackageContainer<Container>::const_iterator() { return typename PackageContainer<Container>::const_iterator(_iter); }
		inline iterator& operator++() { ++_iter; return *this; }
		inline iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(iterator const &i) const { return _iter != i._iter; }
		inline bool operator==(iterator const &i) const { return _iter == i._iter; }
		inline iterator& operator=(iterator const &i) { _iter = i._iter; return *this; }
		inline iterator& operator=(typename Container::iterator const &i) { _iter = i; return *this; }
		friend std::ostream& operator<<(std::ostream& out, iterator i) { return operator<<(out, *i); }
	};
									/*}}}*/

	bool insert(pkgCache::PkgIterator const &P) { if (P.end() == true) return false; _cont.insert(P); return true; }
	template<class Cont> void insert(PackageContainer<Cont> const &pkgcont) { _cont.insert((typename Cont::const_iterator)pkgcont.begin(), (typename Cont::const_iterator)pkgcont.end()); }
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); }

	bool empty() const { return _cont.empty(); }
	void clear() { return _cont.clear(); }
	//FIXME: on ABI break, replace the first with the second without bool
	void erase(iterator position) { _cont.erase((typename Container::iterator)position); }
	iterator& erase(iterator &position, bool) { return position = _cont.erase((typename Container::iterator)position); }
	size_t erase(const pkgCache::PkgIterator x) { return _cont.erase(x); }
	void erase(iterator first, iterator last) { _cont.erase(first, last); }
	size_t size() const { return _cont.size(); }

	const_iterator begin() const { return const_iterator(_cont.begin()); }
	const_iterator end() const { return const_iterator(_cont.end()); }
	iterator begin() { return iterator(_cont.begin()); }
	iterator end() { return iterator(_cont.end()); }
	const_iterator find(pkgCache::PkgIterator const &P) const { return const_iterator(_cont.find(P)); }

	PackageContainer() : PackageContainerInterface() {}
	PackageContainer(CacheSetHelper::PkgSelector const &by) : PackageContainerInterface(by) {}
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED PackageContainer(Constructor const &by) : PackageContainerInterface((CacheSetHelper::PkgSelector)by) {}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	/** \brief sort all included versions with given comparer

	    Some containers are sorted by default, some are not and can't be,
	    but a few like std::vector can be sorted if need be, so this can be
	    specialized in later on. The default is that this will fail though.
	    Specifically, already sorted containers like std::set will return
	    false as well as there is no easy way to check that the given comparer
	    would sort in the same way the set is currently sorted

	    \return \b true if the set was sorted, \b false if not. */
	template<class Compare> bool sort(Compare /*Comp*/) { return false; }

	/** \brief returns all packages in the cache who belong to the given task

	    A simple helper responsible for search for all members of a task
	    in the cache. Optional it prints a a notice about the
	    packages chosen cause of the given task.
	    \param Cache the packages are in
	    \param pattern name of the task
	    \param helper responsible for error and message handling */
	static PackageContainer FromTask(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont(CacheSetHelper::TASK);
		helper.PackageFrom(CacheSetHelper::TASK, &cont, Cache, pattern);
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
	static PackageContainer FromRegEx(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont(CacheSetHelper::REGEX);
		helper.PackageFrom(CacheSetHelper::REGEX, &cont, Cache, pattern);
		return cont;
	}

	static PackageContainer FromRegEx(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromRegEx(Cache, pattern, helper);
	}

	static PackageContainer FromFnmatch(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont(CacheSetHelper::FNMATCH);
		helper.PackageFrom(CacheSetHelper::FNMATCH, &cont, Cache, pattern);
		return cont;
	}
	static PackageContainer FromFnMatch(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromFnmatch(Cache, pattern, helper);
	}

#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	/** \brief returns a package specified by a string

	    \param Cache the package is in
	    \param pattern String the package name should be extracted from
	    \param helper responsible for error and message handling */
	APT_DEPRECATED static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		return helper.PackageFromName(Cache, pattern);
	}
	APT_DEPRECATED static pkgCache::PkgIterator FromName(pkgCacheFile &Cache, std::string const &pattern) {
		CacheSetHelper helper;
		return FromName(Cache, pattern, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	/** \brief returns all packages specified by a string

	    \param Cache the packages are in
	    \param pattern String the package name(s) should be extracted from
	    \param helper responsible for error and message handling */
	static PackageContainer FromString(pkgCacheFile &Cache, std::string const &pattern, CacheSetHelper &helper) {
		PackageContainer cont;
		helper.PackageFrom(CacheSetHelper::PACKAGENAME, &cont, Cache, pattern);
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
		helper.PackageFromCommandLine(&cont, Cache, cmdline);
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
										 std::list<CacheSetHelper::PkgModifier> const &mods,
										 unsigned short const &fallback,
										 CacheSetHelper &helper) {
		std::map<unsigned short, PackageContainer> pkgsets;
		for (const char **I = cmdline; *I != 0; ++I) {
			unsigned short modID = fallback;
			PackageContainer pkgset;
			helper.PackageFromModifierCommandLine(modID, &pkgset, Cache, *I, mods);
			pkgsets[modID].insert(pkgset);
		}
		return pkgsets;
	}
	static std::map<unsigned short, PackageContainer> GroupedFromCommandLine(
										 pkgCacheFile &Cache,
										 const char **cmdline,
										 std::list<CacheSetHelper::PkgModifier> const &mods,
										 unsigned short const &fallback) {
		CacheSetHelper helper;
		return GroupedFromCommandLine(Cache, cmdline,
				mods, fallback, helper);
	}
									/*}}}*/
};									/*}}}*/
// specialisations for push_back containers: std::list & std::vector	/*{{{*/
template<> template<class Cont> void PackageContainer<std::list<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
}
template<> template<class Cont> void PackageContainer<std::vector<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
}
// these two are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool PackageContainer<std::list<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
	if (P.end() == true)
		return false;
	_cont.push_back(P);
	return true;
}
template<> inline bool PackageContainer<std::vector<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
	if (P.end() == true)
		return false;
	_cont.push_back(P);
	return true;
}
template<> inline void PackageContainer<std::list<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_back(*p);
}
template<> inline void PackageContainer<std::vector<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_back(*p);
}
									/*}}}*/

template<> template<class Compare> inline bool PackageContainer<std::vector<pkgCache::PkgIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}

// class PackageUniverse - pkgCache as PackageContainerInterface	/*{{{*/
/** \class PackageUniverse

    Wraps around our usual pkgCache, so that it can be stuffed into methods
    expecting a PackageContainer.

    The wrapping is read-only in practice modeled by making erase and co
    private methods. */
class PackageUniverse : public PackageContainerInterface {
	pkgCache * const _cont;
public:
	typedef pkgCache::PkgIterator iterator;
	typedef pkgCache::PkgIterator const_iterator;

	bool empty() const { return false; }
	size_t size() const { return _cont->Head().PackageCount; }

	const_iterator begin() const { return _cont->PkgBegin(); }
	const_iterator end() const { return  _cont->PkgEnd(); }
	iterator begin() { return _cont->PkgBegin(); }
	iterator end() { return _cont->PkgEnd(); }

	PackageUniverse(pkgCache * const Owner) : _cont(Owner) { }

private:
	bool insert(pkgCache::PkgIterator const &) { return true; }
	template<class Cont> void insert(PackageContainer<Cont> const &) { }
	void insert(const_iterator, const_iterator) { }

	void clear() { }
	iterator& erase(iterator &iter) { return iter; }
	size_t erase(const pkgCache::PkgIterator) { return 0; }
	void erase(iterator, iterator) { }
};
									/*}}}*/
typedef PackageContainer<std::set<pkgCache::PkgIterator> > PackageSet;
typedef PackageContainer<std::list<pkgCache::PkgIterator> > PackageList;
typedef PackageContainer<std::vector<pkgCache::PkgIterator> > PackageVector;

class VersionContainerInterface {					/*{{{*/
/** \class APT::VersionContainerInterface

    Same as APT::PackageContainerInterface, just for Versions */
public:
	/** \brief smell like a pkgCache::VerIterator */
	class const_iterator {						/*{{{*/
	public:
		virtual pkgCache::VerIterator getVer() const = 0;
		operator pkgCache::VerIterator(void) { return getVer(); }

		inline pkgCache *Cache() const { return getVer().Cache(); }
		inline unsigned long Index() const {return getVer().Index();}
		inline int CompareVer(const pkgCache::VerIterator &B) const { return getVer().CompareVer(B); }
		inline const char *VerStr() const { return getVer().VerStr(); }
		inline const char *Section() const { return getVer().Section(); }
		inline const char *Arch() const { return getVer().Arch(); }
		inline pkgCache::PkgIterator ParentPkg() const { return getVer().ParentPkg(); }
		inline pkgCache::DescIterator DescriptionList() const { return getVer().DescriptionList(); }
		inline pkgCache::DescIterator TranslatedDescription() const { return getVer().TranslatedDescription(); }
		inline pkgCache::DepIterator DependsList() const { return getVer().DependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return getVer().ProvidesList(); }
		inline pkgCache::VerFileIterator FileList() const { return getVer().FileList(); }
		inline bool Downloadable() const { return getVer().Downloadable(); }
		inline const char *PriorityType() const { return getVer().PriorityType(); }
		inline std::string RelStr() const { return getVer().RelStr(); }
		inline bool Automatic() const { return getVer().Automatic(); }
		inline pkgCache::VerFileIterator NewestFile() const { return getVer().NewestFile(); }
		// we have only valid iterators here
		inline bool end() const { return false; }

		inline pkgCache::Version const * operator->() const { return &*getVer(); }
	};
									/*}}}*/

	virtual bool insert(pkgCache::VerIterator const &V) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;

	/** \brief specifies which version(s) will be returned if non is given */
	enum APT_DEPRECATED Version {
		ALL = CacheSetHelper::ALL,
		CANDANDINST = CacheSetHelper::CANDANDINST,
		CANDIDATE = CacheSetHelper::CANDIDATE,
		INSTALLED = CacheSetHelper::INSTALLED,
		CANDINST = CacheSetHelper::CANDINST,
		INSTCAND = CacheSetHelper::INSTCAND,
		NEWEST = CacheSetHelper::NEWEST
	};

	struct Modifier {
		unsigned short const ID;
		const char * const Alias;
		enum Position { NONE, PREFIX, POSTFIX } const Pos;
		enum CacheSetHelper::VerSelector const SelectVersion;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos,
			  enum CacheSetHelper::VerSelector const select) : ID(id), Alias(alias), Pos(pos),
			 SelectVersion(select) {}
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
		APT_DEPRECATED Modifier(unsigned short const &id, const char * const alias, Position const &pos,
			  Version const &select) : ID(id), Alias(alias), Pos(pos),
			 SelectVersion((CacheSetHelper::VerSelector)select) {}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
	};

	static bool FromCommandLine(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				    const char **cmdline, CacheSetHelper::VerSelector const fallback,
				    CacheSetHelper &helper);
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED static bool FromCommandLine(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				    const char **cmdline, Version const &fallback,
				    CacheSetHelper &helper) {
	   return FromCommandLine(vci, Cache, cmdline, (CacheSetHelper::VerSelector)fallback, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	static bool FromString(VersionContainerInterface * const vci, pkgCacheFile &Cache,
			       std::string pkg, CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper,
			       bool const onlyFromName = false);
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED static bool FromString(VersionContainerInterface * const vci, pkgCacheFile &Cache,
			       std::string pkg, Version const &fallback, CacheSetHelper &helper,
			       bool const onlyFromName = false) {
	   return FromString(vci, Cache, pkg, (CacheSetHelper::VerSelector)fallback, helper, onlyFromName);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	static bool FromPackage(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &P, CacheSetHelper::VerSelector const fallback,
				CacheSetHelper &helper);
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED static bool FromPackage(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &P, Version const &fallback,
				CacheSetHelper &helper) {
	   return FromPackage(vci, Cache, P, (CacheSetHelper::VerSelector)fallback, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	static bool FromModifierCommandLine(unsigned short &modID,
					    VersionContainerInterface * const vci,
					    pkgCacheFile &Cache, const char * cmdline,
					    std::list<Modifier> const &mods,
					    CacheSetHelper &helper);


	static bool FromDependency(VersionContainerInterface * const vci,
				   pkgCacheFile &Cache,
				   pkgCache::DepIterator const &D,
				   CacheSetHelper::VerSelector const selector,
				   CacheSetHelper &helper);
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	APT_DEPRECATED static bool FromDependency(VersionContainerInterface * const vci,
				   pkgCacheFile &Cache,
				   pkgCache::DepIterator const &D,
				   Version const &selector,
				   CacheSetHelper &helper) {
	   return FromDependency(vci, Cache, D, (CacheSetHelper::VerSelector)selector, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

protected:								/*{{{*/

	/** \brief returns the candidate version of the package

	    \param Cache to be used to query for information
	    \param Pkg we want the candidate version from this package
	    \param helper used in this container instance */
	static pkgCache::VerIterator getCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper);

	/** \brief returns the installed version of the package

	    \param Cache to be used to query for information
	    \param Pkg we want the installed version from this package
	    \param helper used in this container instance */
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
		inline pkgCache::VerIterator operator*(void) const { return *_iter; }
		operator typename Container::const_iterator(void) const { return _iter; }
		inline const_iterator& operator++() { ++_iter; return *this; }
		inline const_iterator operator++(int) { const_iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(const_iterator const &i) const { return _iter != i._iter; }
		inline bool operator==(const_iterator const &i) const { return _iter == i._iter; }
		friend std::ostream& operator<<(std::ostream& out, const_iterator i) { return operator<<(out, *i); }
	};
	class iterator : public VersionContainerInterface::const_iterator,
			 public std::iterator<std::forward_iterator_tag, typename Container::iterator> {
		typename Container::iterator _iter;
	public:
		iterator(typename Container::iterator i) : _iter(i) {}
		pkgCache::VerIterator getVer(void) const { return *_iter; }
		inline pkgCache::VerIterator operator*(void) const { return *_iter; }
		operator typename Container::iterator(void) const { return _iter; }
		operator typename VersionContainer<Container>::const_iterator() { return typename VersionContainer<Container>::const_iterator(_iter); }
		inline iterator& operator++() { ++_iter; return *this; }
		inline iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
		inline bool operator!=(iterator const &i) const { return _iter != i._iter; }
		inline bool operator==(iterator const &i) const { return _iter == i._iter; }
		inline iterator& operator=(iterator const &i) { _iter = i._iter; return *this; }
		inline iterator& operator=(typename Container::iterator const &i) { _iter = i; return *this; }
		friend std::ostream& operator<<(std::ostream& out, iterator i) { return operator<<(out, *i); }
	};
									/*}}}*/

	bool insert(pkgCache::VerIterator const &V) { if (V.end() == true) return false; _cont.insert(V); return true; }
	template<class Cont> void insert(VersionContainer<Cont> const &vercont) { _cont.insert((typename Cont::const_iterator)vercont.begin(), (typename Cont::const_iterator)vercont.end()); }
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); }
	bool empty() const { return _cont.empty(); }
	void clear() { return _cont.clear(); }
	//FIXME: on ABI break, replace the first with the second without bool
	void erase(iterator position) { _cont.erase((typename Container::iterator)position); }
	iterator& erase(iterator &position, bool) { return position = _cont.erase((typename Container::iterator)position); }
	size_t erase(const pkgCache::VerIterator x) { return _cont.erase(x); }
	void erase(iterator first, iterator last) { _cont.erase(first, last); }
	size_t size() const { return _cont.size(); }

	const_iterator begin() const { return const_iterator(_cont.begin()); }
	const_iterator end() const { return const_iterator(_cont.end()); }
	iterator begin() { return iterator(_cont.begin()); }
	iterator end() { return iterator(_cont.end()); }
	const_iterator find(pkgCache::VerIterator const &V) const { return const_iterator(_cont.find(V)); }

	/** \brief sort all included versions with given comparer

	    Some containers are sorted by default, some are not and can't be,
	    but a few like std::vector can be sorted if need be, so this can be
	    specialized in later on. The default is that this will fail though.
	    Specifically, already sorted containers like std::set will return
	    false as well as there is no easy way to check that the given comparer
	    would sort in the same way the set is currently sorted

	    \return \b true if the set was sorted, \b false if not. */
	template<class Compare> bool sort(Compare /*Comp*/) { return false; }

	/** \brief returns all versions specified on the commandline

	    Get all versions from the commandline, uses given default version if
	    non specifically requested  and executes regex's if needed on names.
	    \param Cache the packages and versions are in
	    \param cmdline Command line the versions should be extracted from
	    \param fallback version specification
	    \param helper responsible for error and message handling */
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromCommandLine(&vercon, Cache, cmdline, fallback, helper);
		return vercon;
	}
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			CacheSetHelper::VerSelector const fallback) {
		CacheSetHelper helper;
		return FromCommandLine(Cache, cmdline, fallback, helper);
	}
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline) {
		return FromCommandLine(Cache, cmdline, CacheSetHelper::CANDINST);
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string const &pkg,
			CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper,
                                           bool const /*onlyFromName = false*/) {
		VersionContainer vercon;
		VersionContainerInterface::FromString(&vercon, Cache, pkg, fallback, helper);
		return vercon;
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string pkg,
			CacheSetHelper::VerSelector const fallback) {
		CacheSetHelper helper;
		return FromString(Cache, pkg, fallback, helper);
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string pkg) {
		return FromString(Cache, pkg, CacheSetHelper::CANDINST);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			Version const &fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromCommandLine(&vercon, Cache, cmdline, (CacheSetHelper::VerSelector)fallback, helper);
		return vercon;
	}
	static VersionContainer FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
			Version const &fallback) {
		CacheSetHelper helper;
		return FromCommandLine(Cache, cmdline, (CacheSetHelper::VerSelector)fallback, helper);
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string const &pkg,
			Version const &fallback, CacheSetHelper &helper,
                                           bool const /*onlyFromName = false*/) {
		VersionContainer vercon;
		VersionContainerInterface::FromString(&vercon, Cache, pkg, (CacheSetHelper::VerSelector)fallback, helper);
		return vercon;
	}
	static VersionContainer FromString(pkgCacheFile &Cache, std::string pkg,
			Version const &fallback) {
		CacheSetHelper helper;
		return FromString(Cache, pkg, (CacheSetHelper::VerSelector)fallback, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif

	/** \brief returns all versions specified for the package

	    \param Cache the package and versions are in
	    \param P the package in question
	    \param fallback the version(s) you want to get
	    \param helper the helper used for display and error handling */
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
		CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromPackage(&vercon, Cache, P, fallback, helper);
		return vercon;
	}
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
					    CacheSetHelper::VerSelector const fallback) {
		CacheSetHelper helper;
		return FromPackage(Cache, P, fallback, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
		Version const &fallback, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromPackage(&vercon, Cache, P, (CacheSetHelper::VerSelector)fallback, helper);
		return vercon;
	}
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
					    Version const &fallback) {
		CacheSetHelper helper;
		return FromPackage(Cache, P, (CacheSetHelper::VerSelector)fallback, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
	static VersionContainer FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P) {
		return FromPackage(Cache, P, CacheSetHelper::CANDIDATE);
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
					       CacheSetHelper::VerSelector const selector, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromDependency(&vercon, Cache, D, selector, helper);
		return vercon;
	}
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D,
					       CacheSetHelper::VerSelector const selector) {
		CacheSetHelper helper;
		return FromPackage(Cache, D, selector, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D,
					       Version const &selector, CacheSetHelper &helper) {
		VersionContainer vercon;
		VersionContainerInterface::FromDependency(&vercon, Cache, D, (CacheSetHelper::VerSelector)selector, helper);
		return vercon;
	}
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D,
					       Version const &selector) {
		CacheSetHelper helper;
		return FromPackage(Cache, D, (CacheSetHelper::VerSelector)selector, helper);
	}
#if __GNUC__ >= 4
	#pragma GCC diagnostic pop
#endif
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D) {
		return FromPackage(Cache, D, CacheSetHelper::CANDIDATE);
	}
									/*}}}*/
};									/*}}}*/
// specialisations for push_back containers: std::list & std::vector	/*{{{*/
template<> template<class Cont> void VersionContainer<std::list<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
}
template<> template<class Cont> void VersionContainer<std::vector<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
}
// these two are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool VersionContainer<std::list<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
	if (V.end() == true)
		return false;
	_cont.push_back(V);
	return true;
}
template<> inline bool VersionContainer<std::vector<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
	if (V.end() == true)
		return false;
	_cont.push_back(V);
	return true;
}
template<> inline void VersionContainer<std::list<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_back(*v);
}
template<> inline void VersionContainer<std::vector<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_back(*v);
}
									/*}}}*/

template<> template<class Compare> inline bool VersionContainer<std::vector<pkgCache::VerIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}

typedef VersionContainer<std::set<pkgCache::VerIterator> > VersionSet;
typedef VersionContainer<std::list<pkgCache::VerIterator> > VersionList;
typedef VersionContainer<std::vector<pkgCache::VerIterator> > VersionVector;
}
#endif

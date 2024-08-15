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
#if __cplusplus >= 201103L
#include <forward_list>
#include <initializer_list>
#include <unordered_set>
#endif
#include <algorithm>
#include <deque>
#include <iterator>
#include <list>
#include <string>
#include <vector>

#include <cstddef>

#include <apt-pkg/error.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

									/*}}}*/

class pkgCacheFile;

namespace APT {
class PackageContainerInterface;
class VersionContainerInterface;

class APT_PUBLIC CacheSetHelper {							/*{{{*/
/** \class APT::CacheSetHelper
    Simple base class with a lot of virtual methods which can be overridden
    to alter the behavior or the output of the CacheSets.

    This helper is passed around by the static methods in the CacheSets and
    used every time they hit an error condition or something could be
    printed out.
*/
public:									/*{{{*/
	CacheSetHelper(bool const ShowError = true,
		GlobalError::MsgType ErrorType = GlobalError::ERROR);
	virtual ~CacheSetHelper();

	enum PkgSelector { UNKNOWN, REGEX, TASK, FNMATCH, PACKAGENAME, STRING, PATTERN };

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


	/** \brief be notified about the package being selected via pattern
	 *
	 * Main use is probably to show a message to the user what happened
	 *
	 * \param pkg is the package which was selected
	 * \param select is the selection method which choose the package
	 * \param pattern is the string used by the selection method to pick the package
	 */
	virtual void showPackageSelection(pkgCache::PkgIterator const &pkg, PkgSelector const select, std::string const &pattern);

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

	// the difference between canNotFind and canNotGet is that the later is more low-level
	// and called from other places: In this case looking into the code is the only real answer…
	virtual pkgCache::VerIterator canNotGetVersion(enum VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);

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

	std::string getLastVersionMatcher() const;
	void setLastVersionMatcher(std::string const &matcher);
									/*}}}*/
protected:
	bool ShowError;
	GlobalError::MsgType ErrorType;

	pkgCache::VerIterator canNotGetInstCandVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotGetCandInstVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg);

	pkgCache::VerIterator canNotGetVerFromRelease(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, std::string const &release);
	pkgCache::VerIterator canNotGetVerFromVersionNumber(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, std::string const &verstr);

	bool PackageFromTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromFnmatch(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromPackageName(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern);
	bool PackageFromString(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern);
	bool PackageFromPattern(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &pattern);
private:
	void showTaskSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	void showRegExSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	void showFnmatchSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	void showPatternSelection(pkgCache::PkgIterator const &pkg, std::string const &pattern);
	void canNotFindTask(PackageContainerInterface *const pci, pkgCacheFile &Cache, std::string pattern);
	void canNotFindRegEx(PackageContainerInterface *const pci, pkgCacheFile &Cache, std::string pattern);
	void canNotFindFnmatch(PackageContainerInterface *const pci, pkgCacheFile &Cache, std::string pattern);
	void canNotFindPackage(PackageContainerInterface *const pci, pkgCacheFile &Cache, std::string const &str);
	void showSelectedVersion(pkgCache::PkgIterator const &Pkg, pkgCache::VerIterator const Ver,
				 std::string const &ver, bool const verIsRel);
	void canNotFindAllVer(VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	void canNotFindInstCandVer(VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	void canNotFindCandInstVer(VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);
	pkgCache::VerIterator canNotFindInstalledVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg);

	class Private;
	Private * const d;
};									/*}}}*/
// Iterator templates for our Containers				/*{{{*/
template<typename Interface, typename Master, typename iterator_type, typename container_iterator, typename container_value> class Container_iterator_base :
   public Interface::template iterator_base<iterator_type>
{
protected:
	container_iterator _iter;
public:
	using iterator_category = typename std::iterator_traits<container_iterator>::iterator_category;
	using value_type = container_value;
	using difference_type = std::ptrdiff_t;
	using pointer = container_value*;
	using reference = container_value&;
	explicit Container_iterator_base(container_iterator const &i) : _iter(i) {}
	inline container_value operator*(void) const { return static_cast<iterator_type const*>(this)->getType(); };
	operator container_iterator(void) const { return _iter; }
	inline iterator_type& operator++() { ++_iter; return static_cast<iterator_type&>(*this); }
	inline iterator_type operator++(int) { iterator_type tmp(*this); operator++(); return tmp; }
	inline iterator_type operator+(typename container_iterator::difference_type const &n) const { return iterator_type(_iter + n); }
	inline iterator_type operator+=(typename container_iterator::difference_type const &n) { _iter += n; return static_cast<iterator_type&>(*this); }
	inline iterator_type& operator--() { --_iter;; return static_cast<iterator_type&>(*this); }
	inline iterator_type operator--(int) { iterator_type tmp(*this); operator--(); return tmp; }
	inline iterator_type operator-(typename container_iterator::difference_type const &n) const { return iterator_type(_iter - n); }
	inline typename container_iterator::difference_type operator-(iterator_type const &b) { return (_iter - b._iter); }
	inline iterator_type operator-=(typename container_iterator::difference_type const &n) { _iter -= n; return static_cast<iterator_type&>(*this); }
	inline bool operator!=(iterator_type const &i) const { return _iter != i._iter; }
	inline bool operator==(iterator_type const &i) const { return _iter == i._iter; }
	inline bool operator<(iterator_type const &i) const { return _iter < i._iter; }
	inline bool operator>(iterator_type const &i) const { return _iter > i._iter; }
	inline bool operator<=(iterator_type const &i) const { return _iter <= i._iter; }
	inline bool operator>=(iterator_type const &i) const { return _iter >= i._iter; }
	inline typename container_iterator::reference operator[](typename container_iterator::difference_type const &n) const { return _iter[n]; }

	friend std::ostream& operator<<(std::ostream& out, iterator_type i) { return operator<<(out, *i); }
	friend Master;
};
template<class Interface, class Container, class Master> class Container_const_iterator :
   public Container_iterator_base<Interface, Master, Container_const_iterator<Interface, Container, Master>, typename Container::const_iterator, typename Container::value_type>
{
	typedef Container_const_iterator<Interface, Container, Master> iterator_type;
	typedef typename Container::const_iterator container_iterator;
public:
	explicit Container_const_iterator(container_iterator i) :
	   Container_iterator_base<Interface, Master, iterator_type, container_iterator, typename Container::value_type>(i) {}

	inline typename Container::value_type getType(void) const { return *this->_iter; }
};
template<class Interface, class Container, class Master> class Container_iterator :
   public Container_iterator_base<Interface, Master, Container_iterator<Interface, Container, Master>, typename Container::iterator, typename Container::value_type>
{
	typedef Container_iterator<Interface, Container, Master> iterator_type;
	typedef typename Container::iterator container_iterator;
public:
	explicit Container_iterator(container_iterator const &i) :
	   Container_iterator_base<Interface, Master, iterator_type, container_iterator, typename Container::value_type>(i) {}

	operator typename Master::const_iterator() { return typename Master::const_iterator(this->_iter); }
	inline typename Container::iterator::reference operator*(void) const { return *this->_iter; }

	inline typename Container::value_type getType(void) const { return *this->_iter; }
};
template<class Interface, class Container, class Master> class Container_const_reverse_iterator :
   public Container_iterator_base<Interface, Master, Container_const_reverse_iterator<Interface, Container, Master>, typename Container::const_reverse_iterator, typename Container::value_type>
{
	typedef Container_const_reverse_iterator<Interface, Container, Master> iterator_type;
	typedef typename Container::const_reverse_iterator container_iterator;
public:
	explicit Container_const_reverse_iterator(container_iterator i) :
	   Container_iterator_base<Interface, Master, iterator_type, container_iterator, typename Container::value_type>(i) {}

	inline typename Container::value_type getType(void) const { return *this->_iter; }
};
template<class Interface, class Container, class Master> class Container_reverse_iterator :
   public Container_iterator_base<Interface, Master, Container_reverse_iterator<Interface, Container, Master>, typename Container::reverse_iterator, typename Container::value_type>
{
	typedef Container_reverse_iterator<Interface, Container, Master> iterator_type;
	typedef typename Container::reverse_iterator container_iterator;
public:
	explicit Container_reverse_iterator(container_iterator i) :
	   Container_iterator_base<Interface, Master, iterator_type, container_iterator, typename Container::value_type>(i) {}

	operator typename Master::const_iterator() { return typename Master::const_iterator(this->_iter); }
	inline iterator_type& operator=(iterator_type const &i) { this->_iter = i._iter; return static_cast<iterator_type&>(*this); }
	inline iterator_type& operator=(container_iterator const &i) { this->_iter = i; return static_cast<iterator_type&>(*this); }
	inline typename Container::reverse_iterator::reference operator*(void) const { return *this->_iter; }

	inline typename Container::value_type getType(void) const { return *this->_iter; }
};
									/*}}}*/
class APT_PUBLIC PackageContainerInterface {					/*{{{*/
/** \class PackageContainerInterface

 * Interface ensuring that all operations can be executed on the yet to
 * define concrete PackageContainer - access to all methods is possible,
 * but in general the wrappers provided by the PackageContainer template
 * are nicer to use.

 * This class mostly protects use from the need to write all implementation
 * of the methods working on containers in the template */
public:
	template<class Itr> class iterator_base {			/*{{{*/
		pkgCache::PkgIterator getType() const { return static_cast<Itr const*>(this)->getType(); };
	public:
		operator pkgCache::PkgIterator(void) const { return getType(); }

		inline const char *Name() const {return getType().Name(); }
		inline std::string FullName(bool const Pretty) const { return getType().FullName(Pretty); }
		inline std::string FullName() const { return getType().FullName(); }
		inline bool Purge() const {return getType().Purge(); }
		inline const char *Arch() const {return getType().Arch(); }
		inline pkgCache::GrpIterator Group() const { return getType().Group(); }
		inline pkgCache::VerIterator VersionList() const { return getType().VersionList(); }
		inline pkgCache::VerIterator CurrentVer() const { return getType().CurrentVer(); }
		inline pkgCache::DepIterator RevDependsList() const { return getType().RevDependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return getType().ProvidesList(); }
		inline pkgCache::PkgIterator::OkState State() const { return getType().State(); }
		inline const char *CurVersion() const { return getType().CurVersion(); }
		inline pkgCache *Cache() const { return getType().Cache(); }
		inline unsigned long Index() const {return getType().Index();}
		// we have only valid iterators here
		inline bool end() const { return false; }

		inline pkgCache::Package const * operator->() const {return &*getType();}
	};
									/*}}}*/

	virtual bool insert(pkgCache::PkgIterator const &P) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;
	virtual size_t size() const = 0;

	void setConstructor(CacheSetHelper::PkgSelector const by) { ConstructedBy = by; }
	CacheSetHelper::PkgSelector getConstructor() const { return ConstructedBy; }
	PackageContainerInterface();
	explicit PackageContainerInterface(CacheSetHelper::PkgSelector const by);
	PackageContainerInterface(PackageContainerInterface const &by);
	PackageContainerInterface& operator=(PackageContainerInterface const &other);
	virtual ~PackageContainerInterface();

private:
	CacheSetHelper::PkgSelector ConstructedBy;
	void * const d;
};
									/*}}}*/
template<class Container> class APT_PUBLIC PackageContainer : public PackageContainerInterface {/*{{{*/
/** \class APT::PackageContainer

    Simple wrapper around a container class like std::set to provide a similar
    interface to a set of packages as to the complete set of all packages in the
    pkgCache. */
	Container _cont;
public:									/*{{{*/
	/** \brief smell like a pkgCache::PkgIterator */
	typedef Container_const_iterator<PackageContainerInterface, Container, PackageContainer> const_iterator;
	typedef Container_iterator<PackageContainerInterface, Container, PackageContainer> iterator;
	typedef Container_const_reverse_iterator<PackageContainerInterface, Container, PackageContainer> const_reverse_iterator;
	typedef Container_reverse_iterator<PackageContainerInterface, Container, PackageContainer> reverse_iterator;
	typedef typename Container::value_type value_type;
	typedef typename Container::pointer pointer;
	typedef typename Container::const_pointer const_pointer;
	typedef typename Container::reference reference;
	typedef typename Container::const_reference const_reference;
	typedef typename Container::difference_type difference_type;
	typedef typename Container::size_type size_type;
	typedef typename Container::allocator_type allocator_type;

	bool insert(pkgCache::PkgIterator const &P) APT_OVERRIDE { if (P.end() == true) return false; _cont.insert(P); return true; }
	template<class Cont> void insert(PackageContainer<Cont> const &pkgcont) { _cont.insert((typename Cont::const_iterator)pkgcont.begin(), (typename Cont::const_iterator)pkgcont.end()); }
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); }

	bool empty() const APT_OVERRIDE { return _cont.empty(); }
	void clear() APT_OVERRIDE { return _cont.clear(); }
	size_t size() const APT_OVERRIDE { return _cont.size(); }
#if __GNUC__ >= 5 || (__GNUC_MINOR__ >= 9 && __GNUC__ >= 4)
	iterator erase( const_iterator pos ) { return iterator(_cont.erase(pos._iter)); }
	iterator erase( const_iterator first, const_iterator last ) { return iterator(_cont.erase(first._iter, last._iter)); }
#else
	iterator erase( iterator pos ) { return iterator(_cont.erase(pos._iter)); }
	iterator erase( iterator first, iterator last ) { return iterator(_cont.erase(first._iter, last._iter)); }
#endif
	const_iterator begin() const { return const_iterator(_cont.begin()); }
	const_iterator end() const { return const_iterator(_cont.end()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(_cont.rbegin()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(_cont.rend()); }
#if __cplusplus >= 201103L
	const_iterator cbegin() const { return const_iterator(_cont.cbegin()); }
	const_iterator cend() const { return const_iterator(_cont.cend()); }
	const_reverse_iterator crbegin() const { return const_reverse_iterator(_cont.crbegin()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(_cont.crend()); }
#endif
	iterator begin() { return iterator(_cont.begin()); }
	iterator end() { return iterator(_cont.end()); }
	reverse_iterator rbegin() { return reverse_iterator(_cont.rbegin()); }
	reverse_iterator rend() { return reverse_iterator(_cont.rend()); }
	const_iterator find(pkgCache::PkgIterator const &P) const { return const_iterator(_cont.find(P)); }

	PackageContainer() : PackageContainerInterface(CacheSetHelper::UNKNOWN) {}
	explicit PackageContainer(CacheSetHelper::PkgSelector const &by) : PackageContainerInterface(by) {}
	template<typename Itr> PackageContainer(Itr first, Itr last) : PackageContainerInterface(CacheSetHelper::UNKNOWN), _cont(first, last) {}
#if __cplusplus >= 201103L
	PackageContainer(std::initializer_list<value_type> list) : PackageContainerInterface(CacheSetHelper::UNKNOWN), _cont(list) {}
	void push_back(value_type&& P) { _cont.emplace_back(std::move(P)); }
	template<typename... Args> void emplace_back(Args&&... args) { _cont.emplace_back(std::forward<Args>(args)...); }
#endif
	void push_back(const value_type& P) { _cont.push_back(P); }

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
	    in the cache. Optional it prints a notice about the
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

	/** \brief returns all packages in the cache whose name matches a given pattern

	    A simple helper responsible for executing a regular expression on all
	    package names in the cache. Optional it prints a notice about the
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
// various specialisations for PackageContainer				/*{{{*/
template<> template<class Cont> void PackageContainer<std::list<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
}
#if __cplusplus >= 201103L
template<> template<class Cont> void PackageContainer<std::forward_list<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_front(*p);
}
#endif
template<> template<class Cont> void PackageContainer<std::deque<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
}
template<> template<class Cont> void PackageContainer<std::vector<pkgCache::PkgIterator> >::insert(PackageContainer<Cont> const &pkgcont) {
	for (typename PackageContainer<Cont>::const_iterator p = pkgcont.begin(); p != pkgcont.end(); ++p)
		_cont.push_back(*p);
}
// these are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool PackageContainer<std::list<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
	if (P.end() == true)
		return false;
	_cont.push_back(P);
	return true;
}
#if __cplusplus >= 201103L
template<> inline bool PackageContainer<std::forward_list<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
	if (P.end() == true)
		return false;
	_cont.push_front(P);
	return true;
}
#endif
template<> inline bool PackageContainer<std::deque<pkgCache::PkgIterator> >::insert(pkgCache::PkgIterator const &P) {
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
#if __cplusplus >= 201103L
template<> inline void PackageContainer<std::forward_list<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_front(*p);
}
#endif
template<> inline void PackageContainer<std::deque<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_back(*p);
}
template<> inline void PackageContainer<std::vector<pkgCache::PkgIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator p = begin; p != end; ++p)
		_cont.push_back(*p);
}
#if APT_GCC_VERSION < 0x409
template<> inline PackageContainer<std::set<pkgCache::PkgIterator> >::iterator PackageContainer<std::set<pkgCache::PkgIterator> >::erase(iterator i) {
	_cont.erase(i._iter);
	return end();
}
template<> inline PackageContainer<std::set<pkgCache::PkgIterator> >::iterator PackageContainer<std::set<pkgCache::PkgIterator> >::erase(iterator first, iterator last) {
	_cont.erase(first, last);
	return end();
}
#endif
template<> template<class Compare> inline bool PackageContainer<std::vector<pkgCache::PkgIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}
template<> template<class Compare> inline bool PackageContainer<std::list<pkgCache::PkgIterator> >::sort(Compare Comp) {
	_cont.sort(Comp);
	return true;
}
#if __cplusplus >= 201103L
template<> template<class Compare> inline bool PackageContainer<std::forward_list<pkgCache::PkgIterator> >::sort(Compare Comp) {
	_cont.sort(Comp);
	return true;
}
#endif
template<> template<class Compare> inline bool PackageContainer<std::deque<pkgCache::PkgIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}
									/*}}}*/

// class PackageUniverse - pkgCache as PackageContainerInterface	/*{{{*/
/** \class PackageUniverse

    Wraps around our usual pkgCache, so that it can be stuffed into methods
    expecting a PackageContainer.

    The wrapping is read-only in practice modeled by making erase and co
    private methods. */
class APT_PUBLIC PackageUniverse : public PackageContainerInterface {
	pkgCache * const _cont;
	void * const d;
public:
	class const_iterator : public APT::Container_iterator_base<APT::PackageContainerInterface, PackageUniverse, PackageUniverse::const_iterator, pkgCache::PkgIterator, pkgCache::PkgIterator>
	{
	public:
	   explicit const_iterator(pkgCache::PkgIterator i):
	      Container_iterator_base<APT::PackageContainerInterface, PackageUniverse, PackageUniverse::const_iterator, pkgCache::PkgIterator, pkgCache::PkgIterator>(i) {}

	   inline pkgCache::PkgIterator getType(void) const { return _iter; }
	};
	typedef const_iterator iterator;
	typedef pkgCache::PkgIterator value_type;
	typedef typename pkgCache::PkgIterator* pointer;
	typedef typename pkgCache::PkgIterator const* const_pointer;
	typedef const pkgCache::PkgIterator& const_reference;
	typedef const_reference reference;
	typedef const_iterator::difference_type difference_type;
	typedef std::make_unsigned<const_iterator::difference_type>::type size_type;


	bool empty() const APT_OVERRIDE { return false; }
	size_t size() const APT_OVERRIDE { return _cont->Head().PackageCount; }

	const_iterator begin() const { return const_iterator(_cont->PkgBegin()); }
	const_iterator end() const { return const_iterator(_cont->PkgEnd()); }
	const_iterator cbegin() const { return const_iterator(_cont->PkgBegin()); }
	const_iterator cend() const { return const_iterator(_cont->PkgEnd()); }
	iterator begin() { return iterator(_cont->PkgBegin()); }
	iterator end() { return iterator(_cont->PkgEnd()); }

	pkgCache * data() const { return _cont; }

	explicit PackageUniverse(pkgCache * const Owner);
	explicit PackageUniverse(pkgCacheFile * const Owner);
	virtual ~PackageUniverse();

private:
	APT_HIDDEN bool insert(pkgCache::PkgIterator const &) APT_OVERRIDE { return true; }
	template<class Cont> APT_HIDDEN void insert(PackageContainer<Cont> const &) { }
	APT_HIDDEN void insert(const_iterator, const_iterator) { }

	APT_HIDDEN void clear() APT_OVERRIDE { }
	APT_HIDDEN iterator erase( const_iterator pos );
	APT_HIDDEN iterator erase( const_iterator first, const_iterator last );
};
									/*}}}*/
typedef PackageContainer<std::set<pkgCache::PkgIterator> > PackageSet;
#if __cplusplus >= 201103L
typedef PackageContainer<std::unordered_set<pkgCache::PkgIterator> > PackageUnorderedSet;
typedef PackageContainer<std::forward_list<pkgCache::PkgIterator> > PackageForwardList;
#endif
typedef PackageContainer<std::list<pkgCache::PkgIterator> > PackageList;
typedef PackageContainer<std::deque<pkgCache::PkgIterator> > PackageDeque;
typedef PackageContainer<std::vector<pkgCache::PkgIterator> > PackageVector;

class APT_PUBLIC VersionContainerInterface {					/*{{{*/
/** \class APT::VersionContainerInterface

    Same as APT::PackageContainerInterface, just for Versions */
public:
	/** \brief smell like a pkgCache::VerIterator */
	template<class Itr> class iterator_base {			/*{{{*/
	   pkgCache::VerIterator getType() const { return static_cast<Itr const*>(this)->getType(); };
	public:
		operator pkgCache::VerIterator(void) { return getType(); }

		inline pkgCache *Cache() const { return getType().Cache(); }
		inline unsigned long Index() const {return getType().Index();}
		inline int CompareVer(const pkgCache::VerIterator &B) const { return getType().CompareVer(B); }
		inline const char *VerStr() const { return getType().VerStr(); }
		inline const char *Section() const { return getType().Section(); }
		inline const char *Arch() const { return getType().Arch(); }
		inline pkgCache::PkgIterator ParentPkg() const { return getType().ParentPkg(); }
		inline pkgCache::DescIterator DescriptionList() const { return getType().DescriptionList(); }
		inline pkgCache::DescIterator TranslatedDescription() const { return getType().TranslatedDescription(); }
		inline pkgCache::DepIterator DependsList() const { return getType().DependsList(); }
		inline pkgCache::PrvIterator ProvidesList() const { return getType().ProvidesList(); }
		inline pkgCache::VerFileIterator FileList() const { return getType().FileList(); }
		inline bool Downloadable() const { return getType().Downloadable(); }
		inline const char *PriorityType() const { return getType().PriorityType(); }
		inline std::string RelStr() const { return getType().RelStr(); }
		inline bool Automatic() const { return getType().Automatic(); }
		inline pkgCache::VerFileIterator NewestFile() const { return getType().NewestFile(); }
		// we have only valid iterators here
		inline bool end() const { return false; }

		inline pkgCache::Version const * operator->() const { return &*getType(); }
	};
									/*}}}*/

	virtual bool insert(pkgCache::VerIterator const &V) = 0;
	virtual bool empty() const = 0;
	virtual void clear() = 0;
	virtual size_t size() const = 0;

	struct Modifier {
		unsigned short const ID;
		const char * const Alias;
		enum Position { NONE, PREFIX, POSTFIX } const Pos;
		enum CacheSetHelper::VerSelector const SelectVersion;
		Modifier (unsigned short const &id, const char * const alias, Position const &pos,
			  enum CacheSetHelper::VerSelector const select) : ID(id), Alias(alias), Pos(pos),
			 SelectVersion(select) {}
	};

	static bool FromCommandLine(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				    const char **cmdline, CacheSetHelper::VerSelector const fallback,
				    CacheSetHelper &helper);

	static bool FromString(VersionContainerInterface * const vci, pkgCacheFile &Cache,
			       std::string pkg, CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper,
			       bool const onlyFromName = false);

	static bool FromPattern(VersionContainerInterface *const vci, pkgCacheFile &Cache,
				std::string pkg, CacheSetHelper::VerSelector const fallback, CacheSetHelper &helper);

	static bool FromPackage(VersionContainerInterface * const vci, pkgCacheFile &Cache,
				pkgCache::PkgIterator const &P, CacheSetHelper::VerSelector const fallback,
				CacheSetHelper &helper);

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

	VersionContainerInterface();
	VersionContainerInterface(VersionContainerInterface const &other);
	VersionContainerInterface& operator=(VersionContainerInterface const &other);
	virtual ~VersionContainerInterface();
private:
	void * const d;

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
template<class Container> class APT_PUBLIC VersionContainer : public VersionContainerInterface {/*{{{*/
/** \class APT::VersionContainer

    Simple wrapper around a container class like std::set to provide a similar
    interface to a set of versions as to the complete set of all versions in the
    pkgCache. */
	Container _cont;
public:									/*{{{*/

	typedef Container_const_iterator<VersionContainerInterface, Container, VersionContainer> const_iterator;
	typedef Container_iterator<VersionContainerInterface, Container, VersionContainer> iterator;
	typedef Container_const_reverse_iterator<VersionContainerInterface, Container, VersionContainer> const_reverse_iterator;
	typedef Container_reverse_iterator<VersionContainerInterface, Container, VersionContainer> reverse_iterator;
	typedef typename Container::value_type value_type;
	typedef typename Container::pointer pointer;
	typedef typename Container::const_pointer const_pointer;
	typedef typename Container::reference reference;
	typedef typename Container::const_reference const_reference;
	typedef typename Container::difference_type difference_type;
	typedef typename Container::size_type size_type;
	typedef typename Container::allocator_type allocator_type;

	bool insert(pkgCache::VerIterator const &V) APT_OVERRIDE { if (V.end() == true) return false; _cont.insert(V); return true; }
	template<class Cont> void insert(VersionContainer<Cont> const &vercont) { _cont.insert((typename Cont::const_iterator)vercont.begin(), (typename Cont::const_iterator)vercont.end()); }
	void insert(const_iterator begin, const_iterator end) { _cont.insert(begin, end); }
	bool empty() const APT_OVERRIDE { return _cont.empty(); }
	void clear() APT_OVERRIDE { return _cont.clear(); }
	size_t size() const APT_OVERRIDE { return _cont.size(); }
#if APT_GCC_VERSION >= 0x409
	iterator erase( const_iterator pos ) { return iterator(_cont.erase(pos._iter)); }
	iterator erase( const_iterator first, const_iterator last ) { return iterator(_cont.erase(first._iter, last._iter)); }
#else
	iterator erase( iterator pos ) { return iterator(_cont.erase(pos._iter)); }
	iterator erase( iterator first, iterator last ) { return iterator(_cont.erase(first._iter, last._iter)); }
#endif
	const_iterator begin() const { return const_iterator(_cont.begin()); }
	const_iterator end() const { return const_iterator(_cont.end()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(_cont.rbegin()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(_cont.rend()); }
#if __cplusplus >= 201103L
	const_iterator cbegin() const { return const_iterator(_cont.cbegin()); }
	const_iterator cend() const { return const_iterator(_cont.cend()); }
	const_reverse_iterator crbegin() const { return const_reverse_iterator(_cont.crbegin()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(_cont.crend()); }
#endif
	iterator begin() { return iterator(_cont.begin()); }
	iterator end() { return iterator(_cont.end()); }
	reverse_iterator rbegin() { return reverse_iterator(_cont.rbegin()); }
	reverse_iterator rend() { return reverse_iterator(_cont.rend()); }
	const_iterator find(pkgCache::VerIterator const &V) const { return const_iterator(_cont.find(V)); }

	VersionContainer() : VersionContainerInterface() {}
	template<typename Itr> VersionContainer(Itr first, Itr last) : VersionContainerInterface(), _cont(first, last) {}
#if __cplusplus >= 201103L
	VersionContainer(std::initializer_list<value_type> list) : VersionContainerInterface(), _cont(list) {}
	void push_back(value_type&& P) { _cont.emplace_back(std::move(P)); }
	template<typename... Args> void emplace_back(Args&&... args) { _cont.emplace_back(std::forward<Args>(args)...); }
#endif
	void push_back(const value_type& P) { _cont.push_back(P); }

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
		return FromDependency(Cache, D, selector, helper);
	}
	static VersionContainer FromDependency(pkgCacheFile &Cache, pkgCache::DepIterator const &D) {
		return FromDependency(Cache, D, CacheSetHelper::CANDIDATE);
	}
									/*}}}*/
};									/*}}}*/
// various specialisations for VersionContainer				/*{{{*/
template<> template<class Cont> void VersionContainer<std::list<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
}
#if __cplusplus >= 201103L
template<> template<class Cont> void VersionContainer<std::forward_list<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_front(*v);
}
#endif
template<> template<class Cont> void VersionContainer<std::deque<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
}
template<> template<class Cont> void VersionContainer<std::vector<pkgCache::VerIterator> >::insert(VersionContainer<Cont> const &vercont) {
	for (typename VersionContainer<Cont>::const_iterator v = vercont.begin(); v != vercont.end(); ++v)
		_cont.push_back(*v);
}
// these are 'inline' as otherwise the linker has problems with seeing these untemplated
// specializations again and again - but we need to see them, so that library users can use them
template<> inline bool VersionContainer<std::list<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
	if (V.end() == true)
		return false;
	_cont.push_back(V);
	return true;
}
#if __cplusplus >= 201103L
template<> inline bool VersionContainer<std::forward_list<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
	if (V.end() == true)
		return false;
	_cont.push_front(V);
	return true;
}
#endif
template<> inline bool VersionContainer<std::deque<pkgCache::VerIterator> >::insert(pkgCache::VerIterator const &V) {
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
#if __cplusplus >= 201103L
template<> inline void VersionContainer<std::forward_list<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_front(*v);
}
#endif
template<> inline void VersionContainer<std::deque<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_back(*v);
}
template<> inline void VersionContainer<std::vector<pkgCache::VerIterator> >::insert(const_iterator begin, const_iterator end) {
	for (const_iterator v = begin; v != end; ++v)
		_cont.push_back(*v);
}
#if APT_GCC_VERSION < 0x409
template<> inline VersionContainer<std::set<pkgCache::VerIterator> >::iterator VersionContainer<std::set<pkgCache::VerIterator> >::erase(iterator i) {
	_cont.erase(i._iter);
	return end();
}
template<> inline VersionContainer<std::set<pkgCache::VerIterator> >::iterator VersionContainer<std::set<pkgCache::VerIterator> >::erase(iterator first, iterator last) {
	_cont.erase(first, last);
	return end();
}
#endif
template<> template<class Compare> inline bool VersionContainer<std::vector<pkgCache::VerIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}
template<> template<class Compare> inline bool VersionContainer<std::list<pkgCache::VerIterator> >::sort(Compare Comp) {
	_cont.sort(Comp);
	return true;
}
#if __cplusplus >= 201103L
template<> template<class Compare> inline bool VersionContainer<std::forward_list<pkgCache::VerIterator> >::sort(Compare Comp) {
	_cont.sort(Comp);
	return true;
}
#endif
template<> template<class Compare> inline bool VersionContainer<std::deque<pkgCache::VerIterator> >::sort(Compare Comp) {
	std::sort(_cont.begin(), _cont.end(), Comp);
	return true;
}
									/*}}}*/

typedef VersionContainer<std::set<pkgCache::VerIterator> > VersionSet;
#if __cplusplus >= 201103L
typedef VersionContainer<std::unordered_set<pkgCache::VerIterator> > VersionUnorderedSet;
typedef VersionContainer<std::forward_list<pkgCache::VerIterator> > VersionForwardList;
#endif
typedef VersionContainer<std::list<pkgCache::VerIterator> > VersionList;
typedef VersionContainer<std::deque<pkgCache::VerIterator> > VersionDeque;
typedef VersionContainer<std::vector<pkgCache::VerIterator> > VersionVector;
}
#endif

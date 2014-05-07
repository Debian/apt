#ifndef APT_PRIVATE_CACHESET_H
#define APT_PRIVATE_CACHESET_H

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/macros.h>

#include <algorithm>
#include <vector>
#include <string.h>
#include <list>
#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "private-output.h"

#include <apti18n.h>

class OpProgress;

struct VersionSortDescriptionLocality
{
   bool operator () (const pkgCache::VerIterator &v_lhs, 
                     const pkgCache::VerIterator &v_rhs)
    {
        pkgCache::DescFile *A = v_lhs.TranslatedDescription().FileList();
        pkgCache::DescFile *B = v_rhs.TranslatedDescription().FileList();
        if (A == 0 && B == 0)
           return false;

       if (A == 0)
          return true;

       if (B == 0)
          return false;

       if (A->File == B->File)
          return A->Offset < B->Offset;

       return A->File < B->File;
    }
};

// sorted by locality which makes iterating much faster
typedef APT::VersionContainer<
   std::set<pkgCache::VerIterator,
            VersionSortDescriptionLocality> > LocalitySortedVersionSet;

class Matcher {
public:
    virtual bool operator () (const pkgCache::PkgIterator &/*P*/) {
        return true;}
};

// FIXME: add default argument for OpProgress (or overloaded function)
bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile, 
                                    LocalitySortedVersionSet &output_set,
                                    Matcher &matcher,
                                    OpProgress &progress);
bool GetLocalitySortedVersionSet(pkgCacheFile &CacheFile, 
                                    LocalitySortedVersionSet &output_set,
                                    OpProgress &progress);


// CacheSetHelper saving virtual packages				/*{{{*/
class CacheSetHelperVirtuals: public APT::CacheSetHelper {
public:
   APT::PackageSet virtualPkgs;

   virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
      virtualPkgs.insert(Pkg);
      return CacheSetHelper::canNotFindCandidateVer(Cache, Pkg);
   }

   virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
      virtualPkgs.insert(Pkg);
      return CacheSetHelper::canNotFindNewestVer(Cache, Pkg);
   }

   virtual void canNotFindAllVer(APT::VersionContainerInterface * vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
      virtualPkgs.insert(Pkg);
      CacheSetHelper::canNotFindAllVer(vci, Cache, Pkg);
   }

   CacheSetHelperVirtuals(bool const ShowErrors = true, GlobalError::MsgType const &ErrorType = GlobalError::NOTICE) : CacheSetHelper(ShowErrors, ErrorType) {}
};
									/*}}}*/

// CacheSetHelperAPTGet - responsible for message telling from the CacheSets/*{{{*/
class CacheSetHelperAPTGet : public APT::CacheSetHelper {
	/** \brief stream message should be printed to */
	std::ostream &out;
	/** \brief were things like Task or RegEx used to select packages? */
	bool explicitlyNamed;

	APT::PackageSet virtualPkgs;

public:
	std::list<std::pair<pkgCache::VerIterator, std::string> > selectedByRelease;

	CacheSetHelperAPTGet(std::ostream &out) : APT::CacheSetHelper(true), out(out) {
		explicitlyNamed = true;
	}

	virtual void showTaskSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) {
		ioprintf(out, _("Note, selecting '%s' for task '%s'\n"),
				Pkg.FullName(true).c_str(), pattern.c_str());
		explicitlyNamed = false;
	}
        virtual void showFnmatchSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) {
		ioprintf(out, _("Note, selecting '%s' for glob '%s'\n"),
				Pkg.FullName(true).c_str(), pattern.c_str());
		explicitlyNamed = false;
	}
	virtual void showRegExSelection(pkgCache::PkgIterator const &Pkg, std::string const &pattern) {
		ioprintf(out, _("Note, selecting '%s' for regex '%s'\n"),
				Pkg.FullName(true).c_str(), pattern.c_str());
		explicitlyNamed = false;
	}
	virtual void showSelectedVersion(pkgCache::PkgIterator const &/*Pkg*/, pkgCache::VerIterator const Ver,
				 std::string const &ver, bool const /*verIsRel*/) {
		if (ver == Ver.VerStr())
			return;
		selectedByRelease.push_back(make_pair(Ver, ver));
	}

	bool showVirtualPackageErrors(pkgCacheFile &Cache) {
		if (virtualPkgs.empty() == true)
			return true;
		for (APT::PackageSet::const_iterator Pkg = virtualPkgs.begin();
		     Pkg != virtualPkgs.end(); ++Pkg) {
			if (Pkg->ProvidesList != 0) {
				ioprintf(c1out,_("Package %s is a virtual package provided by:\n"),
					 Pkg.FullName(true).c_str());

				pkgCache::PrvIterator I = Pkg.ProvidesList();
				unsigned short provider = 0;
				for (; I.end() == false; ++I) {
					pkgCache::PkgIterator Pkg = I.OwnerPkg();

					if (Cache[Pkg].CandidateVerIter(Cache) == I.OwnerVer()) {
						c1out << "  " << Pkg.FullName(true) << " " << I.OwnerVer().VerStr();
						if (Cache[Pkg].Install() == true && Cache[Pkg].NewInstall() == false)
							c1out << _(" [Installed]");
						c1out << std::endl;
						++provider;
					}
				}
				// if we found no candidate which provide this package, show non-candidates
				if (provider == 0)
					for (I = Pkg.ProvidesList(); I.end() == false; ++I)
						c1out << "  " << I.OwnerPkg().FullName(true) << " " << I.OwnerVer().VerStr()
                                                      << _(" [Not candidate version]") << std::endl;
				else
                                   out << _("You should explicitly select one to install.") << std::endl;
			} else {
				ioprintf(c1out,
					_("Package %s is not available, but is referred to by another package.\n"
					  "This may mean that the package is missing, has been obsoleted, or\n"
					  "is only available from another source\n"),Pkg.FullName(true).c_str());

				std::string List;
				std::string VersionsList;
				SPtrArray<bool> Seen = new bool[Cache.GetPkgCache()->Head().PackageCount];
				memset(Seen,0,Cache.GetPkgCache()->Head().PackageCount*sizeof(*Seen));
				for (pkgCache::DepIterator Dep = Pkg.RevDependsList();
				     Dep.end() == false; ++Dep) {
					if (Dep->Type != pkgCache::Dep::Replaces)
						continue;
					if (Seen[Dep.ParentPkg()->ID] == true)
						continue;
					Seen[Dep.ParentPkg()->ID] = true;
					List += Dep.ParentPkg().FullName(true) + " ";
					//VersionsList += std::string(Dep.ParentPkg().CurVersion) + "\n"; ???
				}
				ShowList(c1out,_("However the following packages replace it:"),List,VersionsList);
			}
			c1out << std::endl;
		}
		return false;
	}

	virtual pkgCache::VerIterator canNotFindCandidateVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
		APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, APT::VersionSet::CANDIDATE);
		if (verset.empty() == false)
			return *(verset.begin());
		else if (ShowError == true) {
			_error->Error(_("Package '%s' has no installation candidate"),Pkg.FullName(true).c_str());
			virtualPkgs.insert(Pkg);
		}
		return pkgCache::VerIterator(Cache, 0);
	}

	virtual pkgCache::VerIterator canNotFindNewestVer(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
		if (Pkg->ProvidesList != 0)
		{
			APT::VersionSet const verset = tryVirtualPackage(Cache, Pkg, APT::VersionSet::NEWEST);
			if (verset.empty() == false)
				return *(verset.begin());
			if (ShowError == true)
				ioprintf(out, _("Virtual packages like '%s' can't be removed\n"), Pkg.FullName(true).c_str());
		}
		else
		{
			pkgCache::GrpIterator Grp = Pkg.Group();
			pkgCache::PkgIterator P = Grp.PackageList();
			for (; P.end() != true; P = Grp.NextPkg(P))
			{
				if (P == Pkg)
					continue;
				if (P->CurrentVer != 0) {
					// TRANSLATORS: Note, this is not an interactive question
					ioprintf(c1out,_("Package '%s' is not installed, so not removed. Did you mean '%s'?\n"),
						 Pkg.FullName(true).c_str(), P.FullName(true).c_str());
					break;
				}
			}
			if (P.end() == true)
				ioprintf(c1out,_("Package '%s' is not installed, so not removed\n"),Pkg.FullName(true).c_str());
		}
		return pkgCache::VerIterator(Cache, 0);
	}

	APT::VersionSet tryVirtualPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg,
						APT::VersionSet::Version const &select) {
		/* This is a pure virtual package and there is a single available
		   candidate providing it. */
		if (unlikely(Cache[Pkg].CandidateVer != 0) || Pkg->ProvidesList == 0)
			return APT::VersionSet();

		pkgCache::PkgIterator Prov;
		bool found_one = false;
		for (pkgCache::PrvIterator P = Pkg.ProvidesList(); P; ++P) {
			pkgCache::VerIterator const PVer = P.OwnerVer();
			pkgCache::PkgIterator const PPkg = PVer.ParentPkg();

			/* Ignore versions that are not a candidate. */
			if (Cache[PPkg].CandidateVer != PVer)
				continue;

			if (found_one == false) {
				Prov = PPkg;
				found_one = true;
			} else if (PPkg != Prov) {
				// same group, so it's a foreign package
				if (PPkg->Group == Prov->Group) {
					// do we already have the requested arch?
					if (strcmp(Pkg.Arch(), Prov.Arch()) == 0 ||
					    strcmp(Prov.Arch(), "all") == 0 ||
					    unlikely(strcmp(PPkg.Arch(), Prov.Arch()) == 0)) // packages have only on candidate, but just to be sure
						continue;
					// see which architecture we prefer more and switch to it
					std::vector<std::string> archs = APT::Configuration::getArchitectures();
					if (std::find(archs.begin(), archs.end(), PPkg.Arch()) < std::find(archs.begin(), archs.end(), Prov.Arch()))
						Prov = PPkg;
					continue;
				}
				found_one = false; // we found at least two
				break;
			}
		}

		if (found_one == true) {
			ioprintf(out, _("Note, selecting '%s' instead of '%s'\n"),
				 Prov.FullName(true).c_str(), Pkg.FullName(true).c_str());
			return APT::VersionSet::FromPackage(Cache, Prov, select, *this);
		}
		return APT::VersionSet();
	}

	inline bool allPkgNamedExplicitly() const { return explicitlyNamed; }

};
									/*}}}*/

#endif

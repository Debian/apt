// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Simple wrapper around a std::set to provide a similar interface to
   a set of cache structures as to the complete set of all structures
   in the pkgCache. Currently only Package is supported.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/error.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/versionmatch.h>

#include <apti18n.h>

#include <vector>

#include <regex.h>
									/*}}}*/
namespace APT {
// FromTask - Return all packages in the cache from a specific task	/*{{{*/
PackageSet PackageSet::FromTask(pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	size_t const archfound = pattern.find_last_of(':');
	std::string arch = "native";
	if (archfound != std::string::npos) {
		arch = pattern.substr(archfound+1);
		pattern.erase(archfound);
	}

	if (pattern[pattern.length() -1] != '^')
		return APT::PackageSet(TASK);
	pattern.erase(pattern.length()-1);

	if (unlikely(Cache.GetPkgCache() == 0 || Cache.GetDepCache() == 0))
		return APT::PackageSet(TASK);

	PackageSet pkgset(TASK);
	// get the records
	pkgRecords Recs(Cache);

	// build regexp for the task
	regex_t Pattern;
	char S[300];
	snprintf(S, sizeof(S), "^Task:.*[, ]%s([, ]|$)", pattern.c_str());
	if(regcomp(&Pattern,S, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) != 0) {
		_error->Error("Failed to compile task regexp");
		return pkgset;
	}

	for (pkgCache::GrpIterator Grp = Cache->GrpBegin(); Grp.end() == false; ++Grp) {
		pkgCache::PkgIterator Pkg = Grp.FindPkg(arch);
		if (Pkg.end() == true)
			continue;
		pkgCache::VerIterator ver = Cache[Pkg].CandidateVerIter(Cache);
		if(ver.end() == true)
			continue;

		pkgRecords::Parser &parser = Recs.Lookup(ver.FileList());
		const char *start, *end;
		parser.GetRec(start,end);
		unsigned int const length = end - start;
		char buf[length];
		strncpy(buf, start, length);
		buf[length-1] = '\0';
		if (regexec(&Pattern, buf, 0, 0, 0) != 0)
			continue;

		pkgset.insert(Pkg);
	}
	regfree(&Pattern);

	if (pkgset.empty() == true)
		return helper.canNotFindTask(Cache, pattern);

	helper.showTaskSelection(pkgset, pattern);
	return pkgset;
}
									/*}}}*/
// FromRegEx - Return all packages in the cache matching a pattern	/*{{{*/
PackageSet PackageSet::FromRegEx(pkgCacheFile &Cache, std::string pattern, CacheSetHelper &helper) {
	static const char * const isregex = ".?+*|[^$";
	if (pattern.find_first_of(isregex) == std::string::npos)
		return PackageSet(REGEX);

	size_t archfound = pattern.find_last_of(':');
	std::string arch = "native";
	if (archfound != std::string::npos) {
		arch = pattern.substr(archfound+1);
		if (arch.find_first_of(isregex) == std::string::npos)
			pattern.erase(archfound);
		else
			arch = "native";
	}

	if (unlikely(Cache.GetPkgCache() == 0))
		return PackageSet(REGEX);

	APT::CacheFilter::PackageNameMatchesRegEx regexfilter(pattern);

	PackageSet pkgset(REGEX);
	for (pkgCache::GrpIterator Grp = Cache.GetPkgCache()->GrpBegin(); Grp.end() == false; ++Grp) {
		if (regexfilter(Grp) == false)
			continue;
		pkgCache::PkgIterator Pkg = Grp.FindPkg(arch);
		if (Pkg.end() == true) {
			if (archfound == std::string::npos) {
				std::vector<std::string> archs = APT::Configuration::getArchitectures();
				for (std::vector<std::string>::const_iterator a = archs.begin();
				     a != archs.end() && Pkg.end() != true; ++a)
					Pkg = Grp.FindPkg(*a);
			}
			if (Pkg.end() == true)
				continue;
		}

		pkgset.insert(Pkg);
	}

	if (pkgset.empty() == true)
		return helper.canNotFindRegEx(Cache, pattern);

	helper.showRegExSelection(pkgset, pattern);
	return pkgset;
}
									/*}}}*/
// FromName - Returns the package defined  by this string		/*{{{*/
pkgCache::PkgIterator PackageSet::FromName(pkgCacheFile &Cache,
			std::string const &str, CacheSetHelper &helper) {
	std::string pkg = str;
	size_t archfound = pkg.find_last_of(':');
	std::string arch;
	if (archfound != std::string::npos) {
		arch = pkg.substr(archfound+1);
		pkg.erase(archfound);
	}

	if (Cache.GetPkgCache() == 0)
		return pkgCache::PkgIterator(Cache, 0);

	pkgCache::PkgIterator Pkg(Cache, 0);
	if (arch.empty() == true) {
		pkgCache::GrpIterator Grp = Cache.GetPkgCache()->FindGrp(pkg);
		if (Grp.end() == false)
			Pkg = Grp.FindPreferredPkg();
	} else
		Pkg = Cache.GetPkgCache()->FindPkg(pkg, arch);

	if (Pkg.end() == true)
		return helper.canNotFindPkgName(Cache, str);
	return Pkg;
}
									/*}}}*/
// GroupedFromCommandLine - Return all versions specified on commandline/*{{{*/
std::map<unsigned short, PackageSet> PackageSet::GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<PackageSet::Modifier> const &mods,
		unsigned short const &fallback, CacheSetHelper &helper) {
	std::map<unsigned short, PackageSet> pkgsets;
	for (const char **I = cmdline; *I != 0; ++I) {
		unsigned short modID = fallback;
		std::string str = *I;
		bool modifierPresent = false;
		for (std::list<PackageSet::Modifier>::const_iterator mod = mods.begin();
		     mod != mods.end(); ++mod) {
			size_t const alength = strlen(mod->Alias);
			switch(mod->Pos) {
			case PackageSet::Modifier::POSTFIX:
				if (str.compare(str.length() - alength, alength,
						mod->Alias, 0, alength) != 0)
					continue;
				str.erase(str.length() - alength);
				modID = mod->ID;
				break;
			case PackageSet::Modifier::PREFIX:
				continue;
			case PackageSet::Modifier::NONE:
				continue;
			}
			modifierPresent = true;
			break;
		}
		if (modifierPresent == true) {
			bool const errors = helper.showErrors(false);
			pkgCache::PkgIterator Pkg = FromName(Cache, *I, helper);
			helper.showErrors(errors);
			if (Pkg.end() == false) {
				pkgsets[fallback].insert(Pkg);
				continue;
			}
		}
		pkgsets[modID].insert(PackageSet::FromString(Cache, str, helper));
	}
	return pkgsets;
}
									/*}}}*/
// FromCommandLine - Return all packages specified on commandline	/*{{{*/
PackageSet PackageSet::FromCommandLine(pkgCacheFile &Cache, const char **cmdline, CacheSetHelper &helper) {
	PackageSet pkgset;
	for (const char **I = cmdline; *I != 0; ++I) {
		PackageSet pset = FromString(Cache, *I, helper);
		pkgset.insert(pset.begin(), pset.end());
	}
	return pkgset;
}
									/*}}}*/
// FromString - Return all packages matching a specific string		/*{{{*/
PackageSet PackageSet::FromString(pkgCacheFile &Cache, std::string const &str, CacheSetHelper &helper) {
	_error->PushToStack();

	PackageSet pkgset;
	pkgCache::PkgIterator Pkg = FromName(Cache, str, helper);
	if (Pkg.end() == false)
		pkgset.insert(Pkg);
	else {
		pkgset = FromTask(Cache, str, helper);
		if (pkgset.empty() == true) {
			pkgset = FromRegEx(Cache, str, helper);
			if (pkgset.empty() == true)
				pkgset = helper.canNotFindPackage(Cache, str);
		}
	}

	if (pkgset.empty() == false)
		_error->RevertToStack();
	else
		_error->MergeWithStack();
	return pkgset;
}
									/*}}}*/
// GroupedFromCommandLine - Return all versions specified on commandline/*{{{*/
std::map<unsigned short, VersionSet> VersionSet::GroupedFromCommandLine(
		pkgCacheFile &Cache, const char **cmdline,
		std::list<VersionSet::Modifier> const &mods,
		unsigned short const &fallback, CacheSetHelper &helper) {
	std::map<unsigned short, VersionSet> versets;
	for (const char **I = cmdline; *I != 0; ++I) {
		unsigned short modID = fallback;
		VersionSet::Version select = VersionSet::NEWEST;
		std::string str = *I;
		bool modifierPresent = false;
		for (std::list<VersionSet::Modifier>::const_iterator mod = mods.begin();
		     mod != mods.end(); ++mod) {
			if (modID == fallback && mod->ID == fallback)
				select = mod->SelectVersion;
			size_t const alength = strlen(mod->Alias);
			switch(mod->Pos) {
			case VersionSet::Modifier::POSTFIX:
				if (str.compare(str.length() - alength, alength,
						mod->Alias, 0, alength) != 0)
					continue;
				str.erase(str.length() - alength);
				modID = mod->ID;
				select = mod->SelectVersion;
				break;
			case VersionSet::Modifier::PREFIX:
				continue;
			case VersionSet::Modifier::NONE:
				continue;
			}
			modifierPresent = true;
			break;
		}

		if (modifierPresent == true) {
			bool const errors = helper.showErrors(false);
			VersionSet const vset = VersionSet::FromString(Cache, std::string(*I), select, helper, true);
			helper.showErrors(errors);
			if (vset.empty() == false) {
				versets[fallback].insert(vset);
				continue;
			}
		}
		versets[modID].insert(VersionSet::FromString(Cache, str, select , helper));
	}
	return versets;
}
									/*}}}*/
// FromCommandLine - Return all versions specified on commandline	/*{{{*/
APT::VersionSet VersionSet::FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
		APT::VersionSet::Version const &fallback, CacheSetHelper &helper) {
	VersionSet verset;
	for (const char **I = cmdline; *I != 0; ++I)
		verset.insert(VersionSet::FromString(Cache, *I, fallback, helper));
	return verset;
}
									/*}}}*/
// FromString - Returns all versions spedcified by a string		/*{{{*/
APT::VersionSet VersionSet::FromString(pkgCacheFile &Cache, std::string pkg,
		APT::VersionSet::Version const &fallback, CacheSetHelper &helper,
		bool const &onlyFromName) {
	std::string ver;
	bool verIsRel = false;
	size_t const vertag = pkg.find_last_of("/=");
	if (vertag != string::npos) {
		ver = pkg.substr(vertag+1);
		verIsRel = (pkg[vertag] == '/');
		pkg.erase(vertag);
	}
	PackageSet pkgset;
	if (onlyFromName == false)
		pkgset = PackageSet::FromString(Cache, pkg, helper);
	else {
		pkgset.insert(PackageSet::FromName(Cache, pkg, helper));
	}

	VersionSet verset;
	bool errors = true;
	if (pkgset.getConstructor() != PackageSet::UNKNOWN)
		errors = helper.showErrors(false);
	for (PackageSet::const_iterator P = pkgset.begin();
	     P != pkgset.end(); ++P) {
		if (vertag == string::npos) {
			verset.insert(VersionSet::FromPackage(Cache, P, fallback, helper));
			continue;
		}
		pkgCache::VerIterator V;
		if (ver == "installed")
			V = getInstalledVer(Cache, P, helper);
		else if (ver == "candidate")
			V = getCandidateVer(Cache, P, helper);
		else if (ver == "newest") {
			if (P->VersionList != 0)
				V = P.VersionList();
			else
				V = helper.canNotFindNewestVer(Cache, P);
		} else {
			pkgVersionMatch Match(ver, (verIsRel == true ? pkgVersionMatch::Release :
					pkgVersionMatch::Version));
			V = Match.Find(P);
			if (V.end() == true) {
				if (verIsRel == true)
					_error->Error(_("Release '%s' for '%s' was not found"),
							ver.c_str(), P.FullName(true).c_str());
				else
					_error->Error(_("Version '%s' for '%s' was not found"),
							ver.c_str(), P.FullName(true).c_str());
				continue;
			}
		}
		if (V.end() == true)
			continue;
		helper.showSelectedVersion(P, V, ver, verIsRel);
		verset.insert(V);
	}
	if (pkgset.getConstructor() != PackageSet::UNKNOWN)
		helper.showErrors(errors);
	return verset;
}
									/*}}}*/
// FromPackage - versions from package based on fallback		/*{{{*/
VersionSet VersionSet::FromPackage(pkgCacheFile &Cache, pkgCache::PkgIterator const &P,
		VersionSet::Version const &fallback, CacheSetHelper &helper) {
	VersionSet verset;
	pkgCache::VerIterator V;
	bool showErrors;
	switch(fallback) {
	case VersionSet::ALL:
		if (P->VersionList != 0)
			for (V = P.VersionList(); V.end() != true; ++V)
				verset.insert(V);
		else
			verset.insert(helper.canNotFindAllVer(Cache, P));
		break;
	case VersionSet::CANDANDINST:
		verset.insert(getInstalledVer(Cache, P, helper));
		verset.insert(getCandidateVer(Cache, P, helper));
		break;
	case VersionSet::CANDIDATE:
		verset.insert(getCandidateVer(Cache, P, helper));
		break;
	case VersionSet::INSTALLED:
		verset.insert(getInstalledVer(Cache, P, helper));
		break;
	case VersionSet::CANDINST:
		showErrors = helper.showErrors(false);
		V = getCandidateVer(Cache, P, helper);
		if (V.end() == true)
			V = getInstalledVer(Cache, P, helper);
		helper.showErrors(showErrors);
		if (V.end() == false)
			verset.insert(V);
		else
			verset.insert(helper.canNotFindInstCandVer(Cache, P));
		break;
	case VersionSet::INSTCAND:
		showErrors = helper.showErrors(false);
		V = getInstalledVer(Cache, P, helper);
		if (V.end() == true)
			V = getCandidateVer(Cache, P, helper);
		helper.showErrors(showErrors);
		if (V.end() == false)
			verset.insert(V);
		else
			verset.insert(helper.canNotFindInstCandVer(Cache, P));
		break;
	case VersionSet::NEWEST:
		if (P->VersionList != 0)
			verset.insert(P.VersionList());
		else
			verset.insert(helper.canNotFindNewestVer(Cache, P));
		break;
	}
	return verset;
}
									/*}}}*/
// getCandidateVer - Returns the candidate version of the given package	/*{{{*/
pkgCache::VerIterator VersionSet::getCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper) {
	pkgCache::VerIterator Cand;
	if (Cache.IsPolicyBuilt() == true || Cache.IsDepCacheBuilt() == false)
	{
		if (unlikely(Cache.GetPolicy() == 0))
			return pkgCache::VerIterator(Cache);
		Cand = Cache.GetPolicy()->GetCandidateVer(Pkg);
	} else {
		Cand = Cache[Pkg].CandidateVerIter(Cache);
	}
	if (Cand.end() == true)
		return helper.canNotFindCandidateVer(Cache, Pkg);
	return Cand;
}
									/*}}}*/
// getInstalledVer - Returns the installed version of the given package	/*{{{*/
pkgCache::VerIterator VersionSet::getInstalledVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper) {
	if (Pkg->CurrentVer == 0)
		return helper.canNotFindInstalledVer(Cache, Pkg);
	return Pkg.CurrentVer();
}
									/*}}}*/
// canNotFindPkgName - handle the case no package has this name		/*{{{*/
pkgCache::PkgIterator CacheSetHelper::canNotFindPkgName(pkgCacheFile &Cache,
			std::string const &str) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Unable to locate package %s"), str.c_str());
	return pkgCache::PkgIterator(Cache, 0);
}
									/*}}}*/
// canNotFindTask - handle the case no package is found for a task	/*{{{*/
PackageSet CacheSetHelper::canNotFindTask(pkgCacheFile &Cache, std::string pattern) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Couldn't find task '%s'"), pattern.c_str());
	return PackageSet();
}
									/*}}}*/
// canNotFindRegEx - handle the case no package is found by a regex	/*{{{*/
PackageSet CacheSetHelper::canNotFindRegEx(pkgCacheFile &Cache, std::string pattern) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Couldn't find any package by regex '%s'"), pattern.c_str());
	return PackageSet();
}
									/*}}}*/
// canNotFindPackage - handle the case no package is found from a string/*{{{*/
PackageSet CacheSetHelper::canNotFindPackage(pkgCacheFile &Cache, std::string const &str) {
	return PackageSet();
}
									/*}}}*/
// canNotFindAllVer							/*{{{*/
VersionSet CacheSetHelper::canNotFindAllVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select versions from package '%s' as it is purely virtual"), Pkg.FullName(true).c_str());
	return VersionSet();
}
									/*}}}*/
// canNotFindInstCandVer						/*{{{*/
VersionSet CacheSetHelper::canNotFindInstCandVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select installed nor candidate version from package '%s' as it has neither of them"), Pkg.FullName(true).c_str());
	return VersionSet();
}
									/*}}}*/
// canNotFindInstCandVer						/*{{{*/
VersionSet CacheSetHelper::canNotFindCandInstVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select installed nor candidate version from package '%s' as it has neither of them"), Pkg.FullName(true).c_str());
	return VersionSet();
}
									/*}}}*/
// canNotFindNewestVer							/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotFindNewestVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select newest version from package '%s' as it is purely virtual"), Pkg.FullName(true).c_str());
	return pkgCache::VerIterator(Cache, 0);
}
									/*}}}*/
// canNotFindCandidateVer						/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotFindCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select candidate version from package %s as it has no candidate"), Pkg.FullName(true).c_str());
	return pkgCache::VerIterator(Cache, 0);
}
									/*}}}*/
// canNotFindInstalledVer						/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotFindInstalledVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select installed version from package %s as it is not installed"), Pkg.FullName(true).c_str());
	return pkgCache::VerIterator(Cache, 0);
}
									/*}}}*/
}

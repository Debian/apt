// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Simple wrapper around a std::set to provide a similar interface to
   a set of cache structures as to the complete set of all structures
   in the pkgCache. Currently only Package is supported.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/error.h>
#include <apt-pkg/versionmatch.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/fileutl.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <list>
#include <string>
#include <vector>

#include <apti18n.h>
									/*}}}*/
namespace APT {
// PackageFrom - selecting the appropriate method for package selection	/*{{{*/
bool CacheSetHelper::PackageFrom(enum PkgSelector const select, PackageContainerInterface * const pci,
      pkgCacheFile &Cache, std::string const &pattern) {
	switch (select) {
	case UNKNOWN: return false;
	case REGEX: return PackageFromRegEx(pci, Cache, pattern);
	case TASK: return PackageFromTask(pci, Cache, pattern);
	case FNMATCH: return PackageFromFnmatch(pci, Cache, pattern);
	case PACKAGENAME: return PackageFromPackageName(pci, Cache, pattern);
	case STRING: return PackageFromString(pci, Cache, pattern);
	}
	return false;
}
									/*}}}*/
// PackageFromTask - Return all packages in the cache from a specific task /*{{{*/
bool CacheSetHelper::PackageFromTask(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern) {
	size_t const archfound = pattern.find_last_of(':');
	std::string arch = "native";
	if (archfound != std::string::npos) {
		arch = pattern.substr(archfound+1);
		pattern.erase(archfound);
	}

	if (pattern[pattern.length() -1] != '^')
		return false;
	pattern.erase(pattern.length()-1);

	if (unlikely(Cache.GetPkgCache() == 0 || Cache.GetDepCache() == 0))
		return false;

	bool const wasEmpty = pci->empty();
	if (wasEmpty == true)
		pci->setConstructor(CacheSetHelper::TASK);

	// get the records
	pkgRecords Recs(Cache);

	// build regexp for the task
	regex_t Pattern;
	char S[300];
	snprintf(S, sizeof(S), "^Task:.*[, ]%s([, ]|$)", pattern.c_str());
	if(regcomp(&Pattern,S, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) != 0) {
		_error->Error("Failed to compile task regexp");
		return false;
	}

	bool found = false;
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
		if (unlikely(length == 0))
		   continue;
		char buf[length];
		strncpy(buf, start, length);
		buf[length-1] = '\0';
		if (regexec(&Pattern, buf, 0, 0, 0) != 0)
			continue;

		pci->insert(Pkg);
		showPackageSelection(Pkg, CacheSetHelper::TASK, pattern);
		found = true;
	}
	regfree(&Pattern);

	if (found == false) {
		canNotFindPackage(CacheSetHelper::TASK, pci, Cache, pattern);
		pci->setConstructor(CacheSetHelper::UNKNOWN);
		return false;
	}

	if (wasEmpty == false && pci->getConstructor() != CacheSetHelper::UNKNOWN)
		pci->setConstructor(CacheSetHelper::UNKNOWN);

	return true;
}
									/*}}}*/
// PackageFromRegEx - Return all packages in the cache matching a pattern /*{{{*/
bool CacheSetHelper::PackageFromRegEx(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string pattern) {
	static const char * const isregex = ".?+*|[^$";
	if (pattern.find_first_of(isregex) == std::string::npos)
		return false;

	bool const wasEmpty = pci->empty();
	if (wasEmpty == true)
		pci->setConstructor(CacheSetHelper::REGEX);

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
		return false;

	APT::CacheFilter::PackageNameMatchesRegEx regexfilter(pattern);

	bool found = false;
	for (pkgCache::GrpIterator Grp = Cache.GetPkgCache()->GrpBegin(); Grp.end() == false; ++Grp) {
		if (regexfilter(Grp) == false)
			continue;
		pkgCache::PkgIterator Pkg = Grp.FindPkg(arch);
		if (Pkg.end() == true) {
			if (archfound == std::string::npos)
				Pkg = Grp.FindPreferredPkg(true);
			if (Pkg.end() == true)
				continue;
		}

		pci->insert(Pkg);
		showPackageSelection(Pkg, CacheSetHelper::REGEX, pattern);
		found = true;
	}

	if (found == false) {
		canNotFindPackage(CacheSetHelper::REGEX, pci, Cache, pattern);
		pci->setConstructor(CacheSetHelper::UNKNOWN);
		return false;
	}

	if (wasEmpty == false && pci->getConstructor() != CacheSetHelper::UNKNOWN)
		pci->setConstructor(CacheSetHelper::UNKNOWN);

	return true;
}
									/*}}}*/
// PackageFromFnmatch - Returns the package defined  by this fnmatch	/*{{{*/
bool CacheSetHelper::PackageFromFnmatch(PackageContainerInterface * const pci,
                                       pkgCacheFile &Cache, std::string pattern)
{
	static const char * const isfnmatch = ".?*[]!";
	if (pattern.find_first_of(isfnmatch) == std::string::npos)
		return false;

	bool const wasEmpty = pci->empty();
	if (wasEmpty == true)
		pci->setConstructor(CacheSetHelper::FNMATCH);

	size_t archfound = pattern.find_last_of(':');
	std::string arch = "native";
	if (archfound != std::string::npos) {
		arch = pattern.substr(archfound+1);
		if (arch.find_first_of(isfnmatch) == std::string::npos)
			pattern.erase(archfound);
		else
			arch = "native";
	}

	if (unlikely(Cache.GetPkgCache() == 0))
		return false;

	APT::CacheFilter::PackageNameMatchesFnmatch filter(pattern);

	bool found = false;
	for (pkgCache::GrpIterator Grp = Cache.GetPkgCache()->GrpBegin(); Grp.end() == false; ++Grp) {
		if (filter(Grp) == false)
			continue;
		pkgCache::PkgIterator Pkg = Grp.FindPkg(arch);
		if (Pkg.end() == true) {
			if (archfound == std::string::npos)
				Pkg = Grp.FindPreferredPkg(true);
			if (Pkg.end() == true)
				continue;
		}

		pci->insert(Pkg);
		showPackageSelection(Pkg, CacheSetHelper::FNMATCH, pattern);
		found = true;
	}

	if (found == false) {
		canNotFindPackage(CacheSetHelper::FNMATCH, pci, Cache, pattern);
		pci->setConstructor(CacheSetHelper::UNKNOWN);
		return false;
	}

	if (wasEmpty == false && pci->getConstructor() != CacheSetHelper::UNKNOWN)
		pci->setConstructor(CacheSetHelper::UNKNOWN);

	return true;
}
									/*}}}*/
// PackageFromName - Returns the package defined  by this string	/*{{{*/
pkgCache::PkgIterator CacheSetHelper::PackageFromName(pkgCacheFile &Cache,
			std::string const &str) {
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
		return canNotFindPkgName(Cache, str);
	return Pkg;
}
									/*}}}*/
// PackageFromPackageName - Returns the package defined  by this string /*{{{*/
bool CacheSetHelper::PackageFromPackageName(PackageContainerInterface * const pci, pkgCacheFile &Cache,
			std::string pkg) {
	if (unlikely(Cache.GetPkgCache() == 0))
		return false;

	std::string const pkgstring = pkg;
	size_t const archfound = pkg.find_last_of(':');
	std::string arch;
	if (archfound != std::string::npos) {
		arch = pkg.substr(archfound+1);
		pkg.erase(archfound);
		if (arch == "all" || arch == "native")
			arch = _config->Find("APT::Architecture");
	}

	pkgCache::GrpIterator Grp = Cache.GetPkgCache()->FindGrp(pkg);
	if (Grp.end() == false) {
		if (arch.empty() == true) {
			pkgCache::PkgIterator Pkg = Grp.FindPreferredPkg();
			if (Pkg.end() == false)
			{
			   pci->insert(Pkg);
			   return true;
			}
		} else {
			bool found = false;
			// for 'linux-any' return the first package matching, for 'linux-*' return all matches
			bool const isGlobal = arch.find('*') != std::string::npos;
			APT::CacheFilter::PackageArchitectureMatchesSpecification pams(arch);
			for (pkgCache::PkgIterator Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg)) {
				if (pams(Pkg) == false)
					continue;
				pci->insert(Pkg);
				found = true;
				if (isGlobal == false)
					break;
			}
			if (found == true)
				return true;
		}
	}

	pkgCache::PkgIterator Pkg = canNotFindPkgName(Cache, pkgstring);
	if (Pkg.end() == true)
	   return false;

	pci->insert(Pkg);
	return true;
}
									/*}}}*/
// PackageFromString - Return all packages matching a specific string	/*{{{*/
bool CacheSetHelper::PackageFromString(PackageContainerInterface * const pci, pkgCacheFile &Cache, std::string const &str) {
	bool found = true;
	_error->PushToStack();

	if (PackageFrom(CacheSetHelper::PACKAGENAME, pci, Cache, str) == false &&
		 PackageFrom(CacheSetHelper::TASK, pci, Cache, str) == false &&
		 // FIXME: hm, hm, regexp/fnmatch incompatible?
		 PackageFrom(CacheSetHelper::FNMATCH, pci, Cache, str) == false &&
		 PackageFrom(CacheSetHelper::REGEX, pci, Cache, str) == false)
	{
		canNotFindPackage(CacheSetHelper::PACKAGENAME, pci, Cache, str);
		found = false;
	}

	if (found == true)
		_error->RevertToStack();
	else
		_error->MergeWithStack();
	return found;
}
									/*}}}*/
// PackageFromCommandLine - Return all packages specified on commandline /*{{{*/
bool CacheSetHelper::PackageFromCommandLine(PackageContainerInterface * const pci, pkgCacheFile &Cache, const char **cmdline) {
	bool found = false;
	for (const char **I = cmdline; *I != 0; ++I)
		found |= PackageFrom(CacheSetHelper::STRING, pci, Cache, *I);
	return found;
}
									/*}}}*/
// FromModifierCommandLine - helper doing the work for PKG:GroupedFromCommandLine	/*{{{*/
bool CacheSetHelper::PackageFromModifierCommandLine(unsigned short &modID, PackageContainerInterface * const pci,
							pkgCacheFile &Cache, const char * cmdline,
							std::list<PkgModifier> const &mods) {
	std::string str = cmdline;
	unsigned short fallback = modID;
	bool modifierPresent = false;
	for (std::list<PkgModifier>::const_iterator mod = mods.begin();
	     mod != mods.end(); ++mod) {
		size_t const alength = strlen(mod->Alias);
		switch(mod->Pos) {
		case PkgModifier::POSTFIX:
			if (str.compare(str.length() - alength, alength,
					mod->Alias, 0, alength) != 0)
				continue;
			str.erase(str.length() - alength);
			modID = mod->ID;
			break;
		case PkgModifier::PREFIX:
			continue;
		case PkgModifier::NONE:
			continue;
		}
		modifierPresent = true;
		break;
	}
	if (modifierPresent == true) {
		bool const errors = showErrors(false);
		bool const found = PackageFrom(PACKAGENAME, pci, Cache, cmdline);
		showErrors(errors);
		if (found == true) {
			modID = fallback;
			return true;
		}
	}
	return PackageFrom(CacheSetHelper::PACKAGENAME, pci, Cache, str);
}
									/*}}}*/
// FromModifierCommandLine - helper doing the work for VER:GroupedFromCommandLine	/*{{{*/
bool VersionContainerInterface::FromModifierCommandLine(unsigned short &modID,
							VersionContainerInterface * const vci,
							pkgCacheFile &Cache, const char * cmdline,
							std::list<Modifier> const &mods,
							CacheSetHelper &helper) {
	CacheSetHelper::VerSelector select = CacheSetHelper::NEWEST;
	std::string str = cmdline;
	if (unlikely(str.empty() == true))
		return false;
	bool modifierPresent = false;
	unsigned short fallback = modID;
	for (std::list<Modifier>::const_iterator mod = mods.begin();
	     mod != mods.end(); ++mod) {
		if (modID == fallback && mod->ID == fallback)
			select = mod->SelectVersion;
		size_t const alength = strlen(mod->Alias);
		switch(mod->Pos) {
		case Modifier::POSTFIX:
			if (str.length() <= alength ||
			      str.compare(str.length() - alength, alength, mod->Alias, 0, alength) != 0)
				continue;
			str.erase(str.length() - alength);
			modID = mod->ID;
			select = mod->SelectVersion;
			break;
		case Modifier::PREFIX:
			continue;
		case Modifier::NONE:
			continue;
		}
		modifierPresent = true;
		break;
	}
	if (modifierPresent == true) {
		bool const errors = helper.showErrors(false);
		bool const found = VersionContainerInterface::FromString(vci, Cache, cmdline, select, helper, true);
		helper.showErrors(errors);
		if (found == true) {
			modID = fallback;
			return true;
		}
	}
	return FromString(vci, Cache, str, select, helper);
}
									/*}}}*/
// FromCommandLine - Return all versions specified on commandline	/*{{{*/
bool VersionContainerInterface::FromCommandLine(VersionContainerInterface * const vci,
						pkgCacheFile &Cache, const char **cmdline,
						CacheSetHelper::VerSelector const fallback,
						CacheSetHelper &helper) {
	bool found = false;
	for (const char **I = cmdline; *I != 0; ++I)
		found |= VersionContainerInterface::FromString(vci, Cache, *I, fallback, helper);
	return found;
}
									/*}}}*/
// FromString - Returns all versions spedcified by a string		/*{{{*/
bool VersionContainerInterface::FromString(VersionContainerInterface * const vci,
					   pkgCacheFile &Cache, std::string pkg,
					   CacheSetHelper::VerSelector const fallback,
					   CacheSetHelper &helper,
					   bool const onlyFromName) {
	std::string ver;
	bool verIsRel = false;
	size_t const vertag = pkg.find_last_of("/=");
	if (vertag != std::string::npos) {
		ver = pkg.substr(vertag+1);
		verIsRel = (pkg[vertag] == '/');
		pkg.erase(vertag);
	}

	PackageSet pkgset;
	if (onlyFromName == false)
		helper.PackageFrom(CacheSetHelper::STRING, &pkgset, Cache, pkg);
	else {
		helper.PackageFrom(CacheSetHelper::PACKAGENAME, &pkgset, Cache, pkg);
	}

	bool errors = true;
	if (pkgset.getConstructor() != CacheSetHelper::UNKNOWN)
		errors = helper.showErrors(false);

	bool found = false;
	for (PackageSet::const_iterator P = pkgset.begin();
	     P != pkgset.end(); ++P) {
		if (vertag == std::string::npos) {
			found |= VersionContainerInterface::FromPackage(vci, Cache, P, fallback, helper);
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
				V = helper.canNotGetVersion(CacheSetHelper::NEWEST, Cache, P);
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
		if (verIsRel == true)
			helper.showVersionSelection(P, V, CacheSetHelper::RELEASE, ver);
		else
			helper.showVersionSelection(P, V, CacheSetHelper::VERSIONNUMBER, ver);
		vci->insert(V);
		found = true;
	}
	if (pkgset.getConstructor() != CacheSetHelper::UNKNOWN)
		helper.showErrors(errors);
	return found;
}
									/*}}}*/
// FromPackage - versions from package based on fallback		/*{{{*/
bool VersionContainerInterface::FromPackage(VersionContainerInterface * const vci,
					    pkgCacheFile &Cache,
					    pkgCache::PkgIterator const &P,
					    CacheSetHelper::VerSelector const fallback,
					    CacheSetHelper &helper) {
	pkgCache::VerIterator V;
	bool showErrors;
	bool found = false;
	switch(fallback) {
	case CacheSetHelper::ALL:
		if (P->VersionList != 0)
			for (V = P.VersionList(); V.end() != true; ++V)
				found |= vci->insert(V);
		else
			helper.canNotFindVersion(CacheSetHelper::ALL, vci, Cache, P);
		break;
	case CacheSetHelper::CANDANDINST:
		found |= vci->insert(getInstalledVer(Cache, P, helper));
		found |= vci->insert(getCandidateVer(Cache, P, helper));
		break;
	case CacheSetHelper::CANDIDATE:
		found |= vci->insert(getCandidateVer(Cache, P, helper));
		break;
	case CacheSetHelper::INSTALLED:
		found |= vci->insert(getInstalledVer(Cache, P, helper));
		break;
	case CacheSetHelper::CANDINST:
		showErrors = helper.showErrors(false);
		V = getCandidateVer(Cache, P, helper);
		if (V.end() == true)
			V = getInstalledVer(Cache, P, helper);
		helper.showErrors(showErrors);
		if (V.end() == false)
			found |= vci->insert(V);
		else
			helper.canNotFindVersion(CacheSetHelper::CANDINST, vci, Cache, P);
		break;
	case CacheSetHelper::INSTCAND:
		showErrors = helper.showErrors(false);
		V = getInstalledVer(Cache, P, helper);
		if (V.end() == true)
			V = getCandidateVer(Cache, P, helper);
		helper.showErrors(showErrors);
		if (V.end() == false)
			found |= vci->insert(V);
		else
			helper.canNotFindVersion(CacheSetHelper::INSTCAND, vci, Cache, P);
		break;
	case CacheSetHelper::NEWEST:
		if (P->VersionList != 0)
			found |= vci->insert(P.VersionList());
		else
			helper.canNotFindVersion(CacheSetHelper::NEWEST, vci, Cache, P);
		break;
	case CacheSetHelper::RELEASE:
	case CacheSetHelper::VERSIONNUMBER:
		// both make no sense here, so always false
		return false;
	}
	return found;
}
									/*}}}*/
// FromDependency - versions satisfying a given dependency		/*{{{*/
bool VersionContainerInterface::FromDependency(VersionContainerInterface * const vci,
					       pkgCacheFile &Cache,
					       pkgCache::DepIterator const &D,
					       CacheSetHelper::VerSelector const selector,
					       CacheSetHelper &helper)
{
	bool found = false;
	switch(selector) {
	case CacheSetHelper::ALL:
	{
		pkgCache::PkgIterator const T = D.TargetPkg();
		for (pkgCache::VerIterator Ver = T.VersionList(); Ver.end() == false; ++Ver)
		{
		   if (D.IsSatisfied(Ver) == true)
		   {
		      vci->insert(Ver);
		      found = true;
		   }
		   for (pkgCache::PrvIterator Prv = T.ProvidesList(); Prv.end() != true; ++Prv)
		   {
		      pkgCache::VerIterator const V = Prv.OwnerVer();
		      if (unlikely(V.end() == true) || D.IsSatisfied(Prv) == false)
			 continue;
		      vci->insert(V);
		      found = true;
		   }
		}
		return found;
	}
	case CacheSetHelper::CANDANDINST:
	{
		found = FromDependency(vci, Cache, D, CacheSetHelper::CANDIDATE, helper);
		found &= FromDependency(vci, Cache, D, CacheSetHelper::INSTALLED, helper);
		return found;
	}
	case CacheSetHelper::CANDIDATE:
	{
		pkgCache::PkgIterator const T = D.TargetPkg();
		pkgCache::VerIterator const Cand = Cache[T].CandidateVerIter(Cache);
		if (Cand.end() == false && D.IsSatisfied(Cand) == true)
		{
		   vci->insert(Cand);
		   found = true;
		}
		for (pkgCache::PrvIterator Prv = T.ProvidesList(); Prv.end() != true; ++Prv)
		{
		   pkgCache::VerIterator const V = Prv.OwnerVer();
		   pkgCache::VerIterator const Cand = Cache[Prv.OwnerPkg()].CandidateVerIter(Cache);
		   if (Cand.end() == true || V != Cand || D.IsSatisfied(Prv) == false)
		      continue;
		   vci->insert(Cand);
		   found = true;
		}
		return found;
	}
	case CacheSetHelper::INSTALLED:
	{
		pkgCache::PkgIterator const T = D.TargetPkg();
		pkgCache::VerIterator const Cand = T.CurrentVer();
		if (Cand.end() == false && D.IsSatisfied(Cand) == true)
		{
		   vci->insert(Cand);
		   found = true;
		}
		for (pkgCache::PrvIterator Prv = T.ProvidesList(); Prv.end() != true; ++Prv)
		{
		   pkgCache::VerIterator const V = Prv.OwnerVer();
		   pkgCache::VerIterator const Cand = Prv.OwnerPkg().CurrentVer();
		   if (Cand.end() == true || V != Cand || D.IsSatisfied(Prv) == false)
		      continue;
		   vci->insert(Cand);
		   found = true;
		}
		return found;
	}
	case CacheSetHelper::CANDINST:
		return FromDependency(vci, Cache, D, CacheSetHelper::CANDIDATE, helper) ||
		   FromDependency(vci, Cache, D, CacheSetHelper::INSTALLED, helper);
	case CacheSetHelper::INSTCAND:
		return FromDependency(vci, Cache, D, CacheSetHelper::INSTALLED, helper) ||
		   FromDependency(vci, Cache, D, CacheSetHelper::CANDIDATE, helper);
	case CacheSetHelper::NEWEST:
	{
		pkgCache::PkgIterator const T = D.TargetPkg();
		pkgCache::VerIterator const Cand = T.VersionList();
		if (Cand.end() == false && D.IsSatisfied(Cand) == true)
		{
		   vci->insert(Cand);
		   found = true;
		}
		for (pkgCache::PrvIterator Prv = T.ProvidesList(); Prv.end() != true; ++Prv)
		{
		   pkgCache::VerIterator const V = Prv.OwnerVer();
		   pkgCache::VerIterator const Cand = Prv.OwnerPkg().VersionList();
		   if (Cand.end() == true || V != Cand || D.IsSatisfied(Prv) == false)
		      continue;
		   vci->insert(Cand);
		   found = true;
		}
		return found;
	}
	case CacheSetHelper::RELEASE:
	case CacheSetHelper::VERSIONNUMBER:
		// both make no sense here, so always false
		return false;
	}
	return found;
}
									/*}}}*/
// getCandidateVer - Returns the candidate version of the given package	/*{{{*/
pkgCache::VerIterator VersionContainerInterface::getCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper) {
	pkgCache::VerIterator Cand;
	if (Cache.IsDepCacheBuilt() == true) {
		Cand = Cache[Pkg].CandidateVerIter(Cache);
	} else if (unlikely(Cache.GetPolicy() == nullptr)) {
		return pkgCache::VerIterator(Cache);
	} else {
		Cand = Cache.GetPolicy()->GetCandidateVer(Pkg);
	}
	if (Cand.end() == true)
		return helper.canNotGetVersion(CacheSetHelper::CANDIDATE, Cache, Pkg);
	return Cand;
}
									/*}}}*/
// getInstalledVer - Returns the installed version of the given package	/*{{{*/
pkgCache::VerIterator VersionContainerInterface::getInstalledVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, CacheSetHelper &helper) {
	if (Pkg->CurrentVer == 0)
		return helper.canNotGetVersion(CacheSetHelper::INSTALLED, Cache, Pkg);
	return Pkg.CurrentVer();
}
									/*}}}*/

// canNotFindPackage - with the given selector and pattern		/*{{{*/
void CacheSetHelper::canNotFindPackage(enum PkgSelector const select,
      PackageContainerInterface * const pci, pkgCacheFile &Cache,
      std::string const &pattern) {
	switch (select) {
APT_IGNORE_DEPRECATED_PUSH
	case REGEX: canNotFindRegEx(pci, Cache, pattern); break;
	case TASK: canNotFindTask(pci, Cache, pattern); break;
	case FNMATCH: canNotFindFnmatch(pci, Cache, pattern); break;
	case PACKAGENAME: canNotFindPackage(pci, Cache, pattern); break;
	case STRING: canNotFindPackage(pci, Cache, pattern); break;
	case UNKNOWN: break;
APT_IGNORE_DEPRECATED_POP
	}
}
// canNotFindTask - handle the case no package is found for a task	/*{{{*/
void CacheSetHelper::canNotFindTask(PackageContainerInterface * const /*pci*/, pkgCacheFile &/*Cache*/, std::string pattern) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Couldn't find task '%s'"), pattern.c_str());
}
									/*}}}*/
// canNotFindRegEx - handle the case no package is found by a regex	/*{{{*/
void CacheSetHelper::canNotFindRegEx(PackageContainerInterface * const /*pci*/, pkgCacheFile &/*Cache*/, std::string pattern) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Couldn't find any package by regex '%s'"), pattern.c_str());
}
									/*}}}*/
// canNotFindFnmatch - handle the case no package is found by a fnmatch	/*{{{*/
   void CacheSetHelper::canNotFindFnmatch(PackageContainerInterface * const /*pci*/, pkgCacheFile &/*Cache*/, std::string pattern) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Couldn't find any package by glob '%s'"), pattern.c_str());
}
									/*}}}*/
// canNotFindPackage - handle the case no package is found from a string/*{{{*/
APT_CONST void CacheSetHelper::canNotFindPackage(PackageContainerInterface * const /*pci*/, pkgCacheFile &/*Cache*/, std::string const &/*str*/) {
}
									/*}}}*/
									/*}}}*/
// canNotFindPkgName - handle the case no package has this name		/*{{{*/
pkgCache::PkgIterator CacheSetHelper::canNotFindPkgName(pkgCacheFile &Cache,
			std::string const &str) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Unable to locate package %s"), str.c_str());
	return pkgCache::PkgIterator(Cache, 0);
}
									/*}}}*/
// canNotFindVersion - for package by selector				/*{{{*/
void CacheSetHelper::canNotFindVersion(enum VerSelector const select, VersionContainerInterface * const vci, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg)
{
	switch (select) {
APT_IGNORE_DEPRECATED_PUSH
	case ALL: canNotFindAllVer(vci, Cache, Pkg); break;
	case INSTCAND: canNotFindInstCandVer(vci, Cache, Pkg); break;
	case CANDINST: canNotFindCandInstVer(vci, Cache, Pkg); break;
	case NEWEST: canNotFindNewestVer(Cache, Pkg); break;
	case CANDIDATE: canNotFindCandidateVer(Cache, Pkg); break;
	case INSTALLED: canNotFindInstalledVer(Cache, Pkg); break;
APT_IGNORE_DEPRECATED_POP
	case CANDANDINST: canNotGetCandInstVer(Cache, Pkg); break;
	case RELEASE:
	case VERSIONNUMBER:
		// invalid in this branch
		break;
	}
}
// canNotFindAllVer							/*{{{*/
void CacheSetHelper::canNotFindAllVer(VersionContainerInterface * const /*vci*/, pkgCacheFile &/*Cache*/,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select versions from package '%s' as it is purely virtual"), Pkg.FullName(true).c_str());
}
									/*}}}*/
// canNotFindInstCandVer						/*{{{*/
void CacheSetHelper::canNotFindInstCandVer(VersionContainerInterface * const /*vci*/, pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	canNotGetInstCandVer(Cache, Pkg);
}
									/*}}}*/
// canNotFindInstCandVer						/*{{{*/
void CacheSetHelper::canNotFindCandInstVer(VersionContainerInterface * const /*vci*/, pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	canNotGetCandInstVer(Cache, Pkg);
}
									/*}}}*/
									/*}}}*/
// canNotGetVersion - for package by selector				/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotGetVersion(enum VerSelector const select, pkgCacheFile &Cache, pkgCache::PkgIterator const &Pkg) {
	switch (select) {
APT_IGNORE_DEPRECATED_PUSH
	case NEWEST: return canNotFindNewestVer(Cache, Pkg);
	case CANDIDATE: return canNotFindCandidateVer(Cache, Pkg);
	case INSTALLED: return canNotFindInstalledVer(Cache, Pkg);
APT_IGNORE_DEPRECATED_POP
	case CANDINST: return canNotGetCandInstVer(Cache, Pkg);
	case INSTCAND: return canNotGetInstCandVer(Cache, Pkg);
	case ALL:
	case CANDANDINST:
	case RELEASE:
	case VERSIONNUMBER:
		// invalid in this branch
		return pkgCache::VerIterator(Cache, 0);
	}
	return pkgCache::VerIterator(Cache, 0);
}
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
// canNotFindInstCandVer						/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotGetInstCandVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select installed nor candidate version from package '%s' as it has neither of them"), Pkg.FullName(true).c_str());
	return pkgCache::VerIterator(Cache, 0);
}
									/*}}}*/
// canNotFindInstCandVer						/*{{{*/
pkgCache::VerIterator CacheSetHelper::canNotGetCandInstVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg) {
	if (ShowError == true)
		_error->Insert(ErrorType, _("Can't select installed nor candidate version from package '%s' as it has neither of them"), Pkg.FullName(true).c_str());
	return pkgCache::VerIterator(Cache, 0);
}
									/*}}}*/
									/*}}}*/
// showPackageSelection - by selector and given pattern			/*{{{*/
void CacheSetHelper::showPackageSelection(pkgCache::PkgIterator const &pkg, enum PkgSelector const select,
				       std::string const &pattern) {
	switch (select) {
APT_IGNORE_DEPRECATED_PUSH
	case REGEX: showRegExSelection(pkg, pattern); break;
	case TASK: showTaskSelection(pkg, pattern); break;
	case FNMATCH: showFnmatchSelection(pkg, pattern); break;
APT_IGNORE_DEPRECATED_POP
	case PACKAGENAME: /* no surprises here */ break;
	case STRING: /* handled by the special cases */ break;
	case UNKNOWN: break;
	}
}
// showTaskSelection							/*{{{*/
APT_CONST void CacheSetHelper::showTaskSelection(pkgCache::PkgIterator const &/*pkg*/,
				       std::string const &/*pattern*/) {
}
									/*}}}*/
// showRegExSelection							/*{{{*/
APT_CONST void CacheSetHelper::showRegExSelection(pkgCache::PkgIterator const &/*pkg*/,
					std::string const &/*pattern*/) {
}
									/*}}}*/
// showFnmatchSelection							/*{{{*/
APT_CONST void CacheSetHelper::showFnmatchSelection(pkgCache::PkgIterator const &/*pkg*/,
                                         std::string const &/*pattern*/) {
}
									/*}}}*/
									/*}}}*/
// showVersionSelection							/*{{{*/
void CacheSetHelper::showVersionSelection(pkgCache::PkgIterator const &Pkg,
      pkgCache::VerIterator const &Ver, enum VerSelector const select, std::string const &pattern) {
	switch (select) {
APT_IGNORE_DEPRECATED_PUSH
	case RELEASE:
		showSelectedVersion(Pkg, Ver, pattern, true);
		break;
	case VERSIONNUMBER:
		showSelectedVersion(Pkg, Ver, pattern, false);
		break;
APT_IGNORE_DEPRECATED_POP
	case NEWEST:
	case CANDIDATE:
	case INSTALLED:
	case CANDINST:
	case INSTCAND:
	case ALL:
	case CANDANDINST:
		// not really surprises, but in fact: just not implemented
		break;
	}
}
APT_CONST void CacheSetHelper::showSelectedVersion(pkgCache::PkgIterator const &/*Pkg*/,
					 pkgCache::VerIterator const /*Ver*/,
					 std::string const &/*ver*/,
					 bool const /*verIsRel*/) {
}
									/*}}}*/

CacheSetHelper::CacheSetHelper(bool const ShowError, GlobalError::MsgType ErrorType) :
   ShowError(ShowError), ErrorType(ErrorType), d(NULL) {}
CacheSetHelper::~CacheSetHelper() {}

PackageContainerInterface::PackageContainerInterface() : ConstructedBy(CacheSetHelper::UNKNOWN), d(NULL) {}
PackageContainerInterface::PackageContainerInterface(CacheSetHelper::PkgSelector const by) : ConstructedBy(by), d(NULL) {}
PackageContainerInterface& PackageContainerInterface::operator=(PackageContainerInterface const &other) {
   if (this != &other)
      this->ConstructedBy = other.ConstructedBy;
   return *this;
}
PackageContainerInterface::~PackageContainerInterface() {}

PackageUniverse::PackageUniverse(pkgCache * const Owner) : _cont(Owner), d(NULL) {}
PackageUniverse::PackageUniverse(pkgCacheFile * const Owner) : _cont(Owner->GetPkgCache()), d(NULL) {}
PackageUniverse::~PackageUniverse() {}

VersionContainerInterface::VersionContainerInterface() : d(NULL) {}
VersionContainerInterface& VersionContainerInterface::operator=(VersionContainerInterface const &) {
   return *this;
}

VersionContainerInterface::~VersionContainerInterface() {}
}

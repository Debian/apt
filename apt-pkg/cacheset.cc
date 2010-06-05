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
#include <apt-pkg/error.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/versionmatch.h>

#include <apti18n.h>

#include <vector>

#include <regex.h>
									/*}}}*/
namespace APT {
// FromRegEx - Return all packages in the cache matching a pattern	/*{{{*/
PackageSet PackageSet::FromRegEx(pkgCacheFile &Cache, std::string pattern, std::ostream &out) {
	PackageSet pkgset;
	std::string arch = "native";
	static const char * const isregex = ".?+*|[^$";

	if (pattern.find_first_of(isregex) == std::string::npos)
		return pkgset;

	size_t archfound = pattern.find_last_of(':');
	if (archfound != std::string::npos) {
		arch = pattern.substr(archfound+1);
		if (arch.find_first_of(isregex) == std::string::npos)
			pattern.erase(archfound);
		else
			arch = "native";
	}

	regex_t Pattern;
	int Res;
	if ((Res = regcomp(&Pattern, pattern.c_str() , REG_EXTENDED | REG_ICASE | REG_NOSUB)) != 0) {
		char Error[300];
		regerror(Res, &Pattern, Error, sizeof(Error));
		_error->Error(_("Regex compilation error - %s"), Error);
		return pkgset;
	}

	for (pkgCache::GrpIterator Grp = Cache.GetPkgCache()->GrpBegin(); Grp.end() == false; ++Grp)
	{
		if (regexec(&Pattern, Grp.Name(), 0, 0, 0) != 0)
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

		ioprintf(out, _("Note, selecting %s for regex '%s'\n"),
			 Pkg.FullName(true).c_str(), pattern.c_str());

		pkgset.insert(Pkg);
	}

	regfree(&Pattern);

	return pkgset;
}
									/*}}}*/
// FromCommandLine - Return all packages specified on commandline	/*{{{*/
PackageSet PackageSet::FromCommandLine(pkgCacheFile &Cache, const char **cmdline, std::ostream &out) {
	PackageSet pkgset;
	for (const char **I = cmdline; *I != 0; ++I) {
		PackageSet pset = FromString(Cache, *I, out);
		pkgset.insert(pset.begin(), pset.end());
	}
	return pkgset;
}
									/*}}}*/
// FromString - Return all packages matching a specific string		/*{{{*/
PackageSet PackageSet::FromString(pkgCacheFile &Cache, std::string const &str, std::ostream &out) {
	std::string pkg = str;
	size_t archfound = pkg.find_last_of(':');
	std::string arch;
	if (archfound != std::string::npos) {
		arch = pkg.substr(archfound+1);
		pkg.erase(archfound);
	}

	pkgCache::PkgIterator Pkg;
	if (arch.empty() == true) {
		pkgCache::GrpIterator Grp = Cache.GetPkgCache()->FindGrp(pkg);
		if (Grp.end() == false)
			Pkg = Grp.FindPreferredPkg();
	} else
		Pkg = Cache.GetPkgCache()->FindPkg(pkg, arch);

	if (Pkg.end() == false) {
		PackageSet pkgset;
		pkgset.insert(Pkg);
		return pkgset;
	}
	PackageSet regex = FromRegEx(Cache, str, out);
	if (regex.empty() == true)
		_error->Warning(_("Unable to locate package %s"), str.c_str());
	return regex;
}
									/*}}}*/
// FromCommandLine - Return all versions specified on commandline	/*{{{*/
APT::VersionSet VersionSet::FromCommandLine(pkgCacheFile &Cache, const char **cmdline,
		APT::VersionSet::Version const &fallback, std::ostream &out) {
	VersionSet verset;
	for (const char **I = cmdline; *I != 0; ++I) {
		std::string pkg = *I;
		std::string ver;
		bool verIsRel = false;
		size_t const vertag = pkg.find_last_of("/=");
		if (vertag != string::npos) {
			ver = pkg.substr(vertag+1);
			verIsRel = (pkg[vertag] == '/');
			pkg.erase(vertag);
		}
		PackageSet pkgset = PackageSet::FromString(Cache, pkg.c_str(), out);
		for (PackageSet::const_iterator P = pkgset.begin();
		     P != pkgset.end(); ++P) {
			if (vertag == string::npos) {
				AddSelectedVersion(Cache, verset, P, fallback);
				continue;
			}
			pkgCache::VerIterator V;
			if (ver == "installed")
				V = getInstalledVer(Cache, P);
			else if (ver == "candidate")
				V = getCandidateVer(Cache, P);
			else {
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
			if (ver == V.VerStr())
				ioprintf(out, _("Selected version '%s' (%s) for '%s'\n"),
					 V.VerStr(), V.RelStr().c_str(), P.FullName(true).c_str());
			verset.insert(V);
		}
	}
	return verset;
}
									/*}}}*/
// AddSelectedVersion - add version from package based on fallback	/*{{{*/
bool VersionSet::AddSelectedVersion(pkgCacheFile &Cache, VersionSet &verset,
		pkgCache::PkgIterator const &P, VersionSet::Version const &fallback,
		bool const &AllowError) {
	pkgCache::VerIterator V;
	switch(fallback) {
	case VersionSet::ALL:
		if (P->VersionList != 0)
			for (V = P.VersionList(); V.end() != true; ++V)
				verset.insert(V);
		else if (AllowError == false)
			return _error->Error(_("Can't select versions from package '%s' as it purely virtual"), P.FullName(true).c_str());
		else
			return false;
		break;
	case VersionSet::CANDANDINST:
		verset.insert(getInstalledVer(Cache, P, AllowError));
		verset.insert(getCandidateVer(Cache, P, AllowError));
		break;
	case VersionSet::CANDIDATE:
		verset.insert(getCandidateVer(Cache, P, AllowError));
		break;
	case VersionSet::INSTALLED:
		verset.insert(getInstalledVer(Cache, P, AllowError));
		break;
	case VersionSet::CANDINST:
		V = getCandidateVer(Cache, P, true);
		if (V.end() == true)
			V = getInstalledVer(Cache, P, true);
		if (V.end() == false)
			verset.insert(V);
		else if (AllowError == false)
			return _error->Error(_("Can't select installed nor candidate version from package '%s' as it has neither of them"), P.FullName(true).c_str());
		else
			return false;
		break;
	case VersionSet::INSTCAND:
		V = getInstalledVer(Cache, P, true);
		if (V.end() == true)
			V = getCandidateVer(Cache, P, true);
		if (V.end() == false)
			verset.insert(V);
		else if (AllowError == false)
			return _error->Error(_("Can't select installed nor candidate version from package '%s' as it has neither of them"), P.FullName(true).c_str());
		else
			return false;
		break;
	case VersionSet::NEWEST:
		if (P->VersionList != 0)
			verset.insert(P.VersionList());
		else if (AllowError == false)
			return _error->Error(_("Can't select newest version from package '%s' as it is purely virtual"), P.FullName(true).c_str());
		else
			return false;
		break;
	}
	return true;
}
									/*}}}*/
// getCandidateVer - Returns the candidate version of the given package	/*{{{*/
pkgCache::VerIterator VersionSet::getCandidateVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, bool const &AllowError) {
	if (unlikely(Cache.BuildDepCache() == false))
		return pkgCache::VerIterator(*Cache);
	pkgCache::VerIterator Cand = Cache[Pkg].CandidateVerIter(Cache);
	if (AllowError == false && Cand.end() == true)
		_error->Error(_("Can't select candidate version from package %s as it has no candidate"), Pkg.FullName(true).c_str());
	return Cand;
}
									/*}}}*/
// getInstalledVer - Returns the installed version of the given package	/*{{{*/
pkgCache::VerIterator VersionSet::getInstalledVer(pkgCacheFile &Cache,
		pkgCache::PkgIterator const &Pkg, bool const &AllowError) {
	if (AllowError == false && Pkg->CurrentVer == 0)
		_error->Error(_("Can't select installed version from package %s as it is not installed"), Pkg.FullName(true).c_str());
	return Pkg.CurrentVer();
}
									/*}}}*/
}

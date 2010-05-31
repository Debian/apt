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

#include <apti18n.h>

#include <vector>

#include <regex.h>
									/*}}}*/
namespace APT {
// FromRegEx - Return all packages in the cache matching a pattern	/*{{{*/
PackageSet PackageSet::FromRegEx(pkgCache &Cache, std::string pattern, std::ostream &out) {
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

	for (pkgCache::GrpIterator Grp = Cache.GrpBegin(); Grp.end() == false; ++Grp)
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
PackageSet PackageSet::FromCommandLine(pkgCache &Cache, const char **cmdline, std::ostream &out) {
	PackageSet pkgset;
	for (const char **I = cmdline + 1; *I != 0; I++) {
		pkgCache::PkgIterator Pkg = Cache.FindPkg(*I);
		if (Pkg.end() == true) {
			std::vector<std::string> archs = APT::Configuration::getArchitectures();
			for (std::vector<std::string>::const_iterator a = archs.begin();
			     a != archs.end() || Pkg.end() != true; ++a) {
				Pkg = Cache.FindPkg(*I, *a);
			}
			if (Pkg.end() == true) {
				PackageSet regex = FromRegEx(Cache, *I, out);
				if (regex.empty() == true)
					_error->Warning(_("Unable to locate package %s"),*I);
				else
					pkgset.insert(regex.begin(), regex.end());
				continue;
			}
		}
		pkgset.insert(Pkg);
	}
	return pkgset;
}
									/*}}}*/
}

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Simple wrapper around a std::set to provide a similar interface to
   a set of packages as to the complete set of all packages in the
   pkgCache.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/error.h>
#include <apt-pkg/packageset.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>

#include <regex.h>
									/*}}}*/
namespace APT {
// FromRegEx - Return all packages in the cache matching a pattern	/*{{{*/
PackageSet PackageSet::FromRegEx(pkgCache &Cache, const char * const pattern, std::ostream &out) {
	PackageSet pkgset;

	const char * I;
	for (I = pattern; *I != 0; I++)
		if (*I == '.' || *I == '?' || *I == '+' || *I == '*' ||
		    *I == '|' || *I == '[' || *I == '^' || *I == '$')
			break;
	if (*I == 0)
		return pkgset;

	regex_t Pattern;
	int Res;
	if ((Res = regcomp(&Pattern, pattern , REG_EXTENDED | REG_ICASE | REG_NOSUB)) != 0) {
		char Error[300];
		regerror(Res, &Pattern, Error, sizeof(Error));
		_error->Error(_("Regex compilation error - %s"), Error);
		return pkgset;
	}

	for (pkgCache::GrpIterator Grp = Cache.GrpBegin(); Grp.end() == false; ++Grp)
	{
		if (regexec(&Pattern, Grp.Name(), 0, 0, 0) != 0)
			continue;
		pkgCache::PkgIterator Pkg = Grp.FindPkg("native");
		if (unlikely(Pkg.end() == true))
			// FIXME: Fallback to different architectures here?
			continue;

		ioprintf(out, _("Note, selecting %s for regex '%s'\n"),
			 Pkg.Name(), pattern);

		pkgset.insert(Pkg);
	}

	regfree(&Pattern);

	return pkgset;
}
									/*}}}*/
}

#include <apt-pkg/deblistparser.h>
#include <apt-pkg/configuration.h>

#include "assert.h"

int main(int argc,char *argv[]) {
	string Package;
	string Version;
	unsigned int Op = 5;
	unsigned int Null = 0;
	bool StripMultiArch = true;
	bool ParseArchFlags = false;
	_config->Set("APT::Architecture","dsk");

	const char* Depends =
		"debhelper:any (>= 5.0), "
		"libdb-dev:any, "
		"gettext:native (<= 0.12), "
		"libcurl4-gnutls-dev:native | libcurl3-gnutls-dev (>> 7.15.5), "
		"debiandoc-sgml, "
		"apt (>= 0.7.25), "
		"not-for-me [ !dsk ], "
		"only-for-me [ dsk ], "
		"any-for-me [ any ], "
		"not-for-darwin [ !darwin-any ], "
		"cpu-for-me [ any-dsk ], "
		"os-for-me [ linux-any ], "
		"cpu-not-for-me [ any-amd64 ], "
		"os-not-for-me [ kfreebsd-any ], "
		"overlord-dev:any (= 7.15.3~) | overlord-dev:native (>> 7.15.5), "
	;

	unsigned short runner = 0;
test:
// 	std::clog << (StripMultiArch ? "NO-Multi" : "Multi") << " " << (ParseArchFlags ? "Flags" : "NO-Flags") << std::endl;

	// Stripping MultiArch is currently the default setting to not confuse
	// non-MultiArch capable users of the library with "strange" extensions.
	const char* Start = Depends;
	const char* End = Depends + strlen(Depends);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("debhelper", Package);
	else
		equals("debhelper:any", Package);
	equals("5.0", Version);
	equals(Null | pkgCache::Dep::GreaterEq, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("libdb-dev", Package);
	else
		equals("libdb-dev:any", Package);
	equals("", Version);
	equals(Null | pkgCache::Dep::NoOp, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("gettext", Package);
	else
		equals("gettext:native", Package);
	equals("0.12", Version);
	equals(Null | pkgCache::Dep::LessEq, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("libcurl4-gnutls-dev", Package);
	else
		equals("libcurl4-gnutls-dev:native", Package);
	equals("", Version);
	equals(Null | pkgCache::Dep::Or, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	equals("libcurl3-gnutls-dev", Package);
	equals("7.15.5", Version);
	equals(Null | pkgCache::Dep::Greater, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	equals("debiandoc-sgml", Package);
	equals("", Version);
	equals(Null | pkgCache::Dep::NoOp, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	equals("apt", Package);
	equals("0.7.25", Version);
	equals(Null | pkgCache::Dep::GreaterEq, Op);

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("", Package); // not-for-me
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("only-for-me", Package);
		equals("", Version);
		equals(Null | pkgCache::Dep::NoOp, Op);
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("any-for-me", Package);
		equals("", Version);
		equals(Null | pkgCache::Dep::NoOp, Op);
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("not-for-darwin", Package);
		equals("", Version);
		equals(Null | pkgCache::Dep::NoOp, Op);
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("cpu-for-me", Package);
		equals("", Version);
		equals(Null | pkgCache::Dep::NoOp, Op);
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("os-for-me", Package);
		equals("", Version);
		equals(Null | pkgCache::Dep::NoOp, Op);
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("", Package); // cpu-not-for-me
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	if (ParseArchFlags == true) {
		Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
		equals("", Package); // os-not-for-me
	} else {
		equals(true, 0 == debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch));
		Start = strstr(Start, ",");
		Start++;
	}

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("overlord-dev", Package);
	else
		equals("overlord-dev:any", Package);
	equals("7.15.3~", Version);
	equals(Null | pkgCache::Dep::Equals | pkgCache::Dep::Or, Op);

	Start = debListParser::ParseDepends(Start, End, Package, Version, Op, ParseArchFlags, StripMultiArch);
	if (StripMultiArch == true)
		equals("overlord-dev", Package);
	else
		equals("overlord-dev:native", Package);
	equals("7.15.5", Version);
	equals(Null | pkgCache::Dep::Greater, Op);

	if (StripMultiArch == false)
		ParseArchFlags = true;
	StripMultiArch = !StripMultiArch;

	runner++;
	if (runner < 4)
		goto test; // this is the prove: tests are really evil ;)

	return 0;
}

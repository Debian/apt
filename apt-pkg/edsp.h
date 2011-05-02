// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSP_H
#define PKGLIB_EDSP_H

#include <apt-pkg/depcache.h>
#include <apt-pkg/cacheset.h>

#include <string>

class EDSP								/*{{{*/
{
	// we could use pkgCache::DepType and ::Priority, but these would be localized strings…
	static const char * const PrioMap[];
	static const char * const DepMap[];

	bool static ReadLine(int const input, std::string &line);
	bool static StringToBool(char const *answer, bool const defValue);

	void static WriteScenarioVersion(pkgDepCache &Cache, FILE* output,
					 pkgCache::PkgIterator const &Pkg,
					 pkgCache::VerIterator const &Ver);
	void static WriteScenarioDependency(pkgDepCache &Cache, FILE* output,
					    pkgCache::PkgIterator const &Pkg,
					    pkgCache::VerIterator const &Ver);
	void static WriteScenarioLimitedDependency(pkgDepCache &Cache, FILE* output,
						   pkgCache::PkgIterator const &Pkg,
						   pkgCache::VerIterator const &Ver,
						   APT::PackageSet const &pkgset);
public:
	bool static WriteRequest(pkgDepCache &Cache, FILE* output,
				 bool const upgrade = false,
				 bool const distUpgrade = false,
				 bool const autoRemove = false);
	bool static WriteScenario(pkgDepCache &Cache, FILE* output);
	bool static WriteLimitedScenario(pkgDepCache &Cache, FILE* output,
					 APT::PackageSet const &pkgset);
	bool static ReadResponse(int const input, pkgDepCache &Cache);

	// ReadScenario is provided by the listparser infrastructure
	bool static ReadRequest(int const input, std::list<std::string> &install,
			std::list<std::string> &remove, bool &upgrade,
			bool &distUpgrade, bool &autoRemove);
	bool static ApplyRequest(std::list<std::string> const &install,
				 std::list<std::string> const &remove,
				 pkgDepCache &Cache);
	bool static WriteSolution(pkgDepCache &Cache, FILE* output);
	bool static WriteProgress(unsigned short const percent, const char* const message, FILE* output);
	bool static WriteError(std::string const &message, FILE* output);

};
									/*}}}*/
#endif

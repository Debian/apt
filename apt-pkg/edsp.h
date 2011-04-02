// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSP_H
#define PKGLIB_EDSP_H

#include <apt-pkg/depcache.h>

#include <string>

class EDSP								/*{{{*/
{
	bool static ReadLine(int const input, std::string &line);
	bool static StringToBool(char const *answer, bool const defValue);

public:
	bool static WriteRequest(pkgDepCache &Cache, FILE* output,
				 bool const upgrade = false,
				 bool const distUpgrade = false,
				 bool const autoRemove = false);
	bool static WriteScenario(pkgDepCache &Cache, FILE* output);
	bool static ReadResponse(int const input, pkgDepCache &Cache);

	// ReadScenario is provided by the listparser infrastructure
	bool static ReadRequest(int const input, std::list<std::string> &install,
			std::list<std::string> &remove, bool &upgrade,
			bool &distUpgrade, bool &autoRemove);
	bool static ApplyRequest(std::list<std::string> const &install,
				 std::list<std::string> const &remove,
				 pkgDepCache &Cache);
	bool static WriteSolution(pkgDepCache &Cache, FILE* output);
	bool static WriteError(std::string const &message, FILE* output);

};
									/*}}}*/
#endif

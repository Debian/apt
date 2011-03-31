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
public:
	bool static WriteRequest(pkgDepCache &Cache, FILE* output);
	bool static WriteScenario(pkgDepCache &Cache, FILE* output);
	bool static ReadResponse(FILE* input, pkgDepCache &Cache);

	// ReadScenario is provided by the listparser infrastructure
	bool static ReadRequest(FILE* input, std::list<std::string> &install,
				std::list<std::string> &remove);
	bool static ApplyRequest(std::list<std::string> const &install,
				 std::list<std::string> const &remove,
				 pkgDepCache &Cache);
	bool static WriteSolution(pkgDepCache &Cache, FILE* output);
	bool static WriteError(std::string const &message, FILE* output);

};
									/*}}}*/
#endif

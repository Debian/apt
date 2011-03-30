// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   Set of methods to help writing and reading everything needed for EDSP
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPWRITER_H
#define PKGLIB_EDSPWRITER_H

#include <apt-pkg/depcache.h>

class edspWriter								/*{{{*/
{
public:
	bool static WriteUniverse(pkgDepCache &Cache, FILE* output);
	bool static WriteRequest(pkgDepCache &Cache, FILE* output);
};
									/*}}}*/
#endif

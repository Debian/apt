// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Update - ListUpdate related code

   ##################################################################### */
									/*}}}*/

#ifndef PKGLIB_UPDATE_H
#define PKGLIB_UPDATE_H

class pkgAcquireStatus;
class pkgSourceList;
class pkgAcquire;

bool ListUpdate(pkgAcquireStatus &progress, pkgSourceList &List, int PulseInterval=0);
bool AcquireUpdate(pkgAcquire &Fetcher, int const PulseInterval = 0,
		   bool const RunUpdateScripts = true, bool const ListCleanup = true);


#endif

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

APT_PUBLIC bool ListUpdate(pkgAcquireStatus &progress, pkgSourceList &List, int PulseInterval=0);
APT_PUBLIC bool AcquireUpdate(pkgAcquire &Fetcher, int const PulseInterval = 0,
		   bool const RunUpdateScripts = true, bool const ListCleanup = true);


#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: version.h,v 1.4 1998/07/19 21:24:19 jgg Exp $
/* ######################################################################

   Version - Version comparison routines
   
   These routines provide some means to compare versions and check
   dependencies.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_VERSION_H
#define PKGLIB_VERSION_H

#ifdef __GNUG__
#pragma interface "apt-pkg/version.h"
#endif 

#include <string>

int pkgVersionCompare(const char *A, const char *B);
int pkgVersionCompare(const char *A, const char *AEnd, const char *B, 
		   const char *BEnd);
int pkgVersionCompare(string A,string B);
bool pkgCheckDep(const char *DepVer,const char *PkgVer,int Op);

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: version.h,v 1.2 1998/07/07 04:17:09 jgg Exp $
/* ######################################################################

   Version - Version string 
   
   This class implements storage and operators for version strings.

   The client is responsible for stripping epochs should it be desired.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_VERSION_H
#define PKGLIB_VERSION_H

#ifdef __GNUG__
#pragma interface "pkglib/version.h"
#endif 

#include <string>

class pkgVersion
{
   string Value;
   
   public:

   inline operator string () const {return Value;};

   // Assignmnet
   void operator =(string rhs) {Value = rhs;};
   
   // Comparitors. STL will provide the rest
   bool operator ==(const pkgVersion &rhs) const;
   bool operator <(const pkgVersion &rhs) const;
   
   pkgVersion();
   pkgVersion(string Version) : Value(Version) {};
};

int pkgVersionCompare(const char *A, const char *B);
int pkgVersionCompare(const char *A, const char *AEnd, const char *B, 
		   const char *BEnd);
int pkgVersionCompare(string A,string B);
bool pkgCheckDep(const char *DepVer,const char *PkgVer,int Op);

#endif

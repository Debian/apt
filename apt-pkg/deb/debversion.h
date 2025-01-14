// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Debian Version - Versioning system for Debian

   This implements the standard Debian versioning system.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBVERSION_H
#define PKGLIB_DEBVERSION_H

#include <apt-pkg/version.h>

#include <string>

class APT_PUBLIC debVersioningSystem : public pkgVersioningSystem
{
   public:

   static int CmpFragment(const char *A, const char *AEnd, const char *B,
			  const char *BEnd) APT_PURE;

   // Compare versions..
   int DoCmpVersion(const char *A, const char *Aend,
		    const char *B, const char *Bend) override APT_PURE;
   bool CheckDep(const char *PkgVer, int Op, const char *DepVer) override APT_PURE;
   int DoCmpReleaseVer(const char *A, const char *Aend,
		       const char *B, const char *Bend) override APT_PURE
   {
      return DoCmpVersion(A,Aend,B,Bend);
   }
   std::string UpstreamVersion(const char *A) override;

   debVersioningSystem();
};

extern APT_PUBLIC debVersioningSystem debVS;

#endif

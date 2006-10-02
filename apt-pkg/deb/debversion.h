// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debversion.h,v 1.3 2001/05/03 05:25:04 jgg Exp $
/* ######################################################################

   Debian Version - Versioning system for Debian

   This implements the standard Debian versioning system.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBVERSION_H
#define PKGLIB_DEBVERSION_H



#include <apt-pkg/version.h>
    
class debVersioningSystem : public pkgVersioningSystem
{     
   public:
   
   static int CmpFragment(const char *A, const char *AEnd, const char *B,
			  const char *BEnd);
   
   // Compare versions..
   virtual int DoCmpVersion(const char *A,const char *Aend,
			  const char *B,const char *Bend);
   virtual bool CheckDep(const char *PkgVer,int Op,const char *DepVer);
   virtual int DoCmpReleaseVer(const char *A,const char *Aend,
			     const char *B,const char *Bend)
   {
      return DoCmpVersion(A,Aend,B,Bend);
   }   
   virtual string UpstreamVersion(const char *A);

   debVersioningSystem();
};

extern debVersioningSystem debVS;

#ifdef APT_COMPATIBILITY
#if APT_COMPATIBILITY != 986
#warning "Using APT_COMPATIBILITY"
#endif

inline int pkgVersionCompare(const char *A, const char *B)
{
   return debVS.CmpVersion(A,B);
}
inline int pkgVersionCompare(const char *A, const char *AEnd, 
			     const char *B, const char *BEnd)
{
   return debVS.DoCmpVersion(A,AEnd,B,BEnd);
}
inline int pkgVersionCompare(string A,string B)
{
   return debVS.CmpVersion(A,B);
}
inline bool pkgCheckDep(const char *DepVer,const char *PkgVer,int Op)
{
   return debVS.CheckDep(PkgVer,Op,DepVer);
}
inline string pkgBaseVersion(const char *Ver)
{
   return debVS.UpstreamVersion(Ver);
}
#endif

#endif

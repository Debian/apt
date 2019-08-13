// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Version - Versioning system..

   The versioning system represents how versions are compared, represented
   and how dependencies are evaluated. As a general rule versioning
   systems are not compatible unless specifically allowed by the 
   TestCompatibility query.
   
   The versions are stored in a global list of versions, but that is just
   so that they can be queried when someone does 'apt-get -v'. 
   pkgSystem provides the proper means to access the VS for the active
   system.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_VERSION_H
#define PKGLIB_VERSION_H

#include <apt-pkg/strutl.h>
#include <string>


class APT_PUBLIC pkgVersioningSystem
{
   public:
   // Global list of VS's
   static pkgVersioningSystem **GlobalList;
   static unsigned long GlobalListLen;
   static pkgVersioningSystem *GetVS(const char *Label) APT_PURE;
   
   const char *Label;
   
   // Compare versions..
   virtual int DoCmpVersion(const char *A,const char *Aend,
			  const char *B,const char *Bend) = 0;   

   virtual bool CheckDep(const char *PkgVer,int Op,const char *DepVer) = 0;
   virtual int DoCmpReleaseVer(const char *A,const char *Aend,
			       const char *B,const char *Bend) = 0;
   virtual std::string UpstreamVersion(const char *A) = 0;
   
   // See if the given VS is compatible with this one.. 
   virtual bool TestCompatibility(pkgVersioningSystem const &Against) 
                {return this == &Against;};

   // Shortcuts
   APT_MKSTRCMP(CmpVersion,DoCmpVersion);
   APT_MKSTRCMP(CmpReleaseVer,DoCmpReleaseVer);
   
   pkgVersioningSystem();
   virtual ~pkgVersioningSystem();
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.h,v 1.7 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   DPKG Package Manager - Provide an interface to dpkg
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DPKGPM_H
#define PKGLIB_DPKGPM_H

#ifdef __GNUG__
#pragma interface "apt-pkg/dpkgpm.h"
#endif

#include <apt-pkg/packagemanager.h>
#include <vector>
#include <stdio.h>

class pkgDPkgPM : public pkgPackageManager
{
   protected:
   
   struct Item
   {
      enum Ops {Install, Configure, Remove, Purge} Op;
      string File;
      PkgIterator Pkg;
      Item(Ops Op,PkgIterator Pkg,string File = "") : Op(Op),
            File(File), Pkg(Pkg) {};
      Item() {};
      
   };
   vector<Item> List;

   // Helpers
   bool RunScripts(const char *Cnf);
   bool RunScriptsWithPkgs(const char *Cnf);
   bool SendV2Pkgs(FILE *F);
   
   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg,bool Purge = false);
   virtual bool Go();
   virtual void Reset();
   
   public:

   pkgDPkgPM(pkgDepCache *Cache);
   virtual ~pkgDPkgPM();
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkgpm.h,v 1.2 1998/11/23 07:03:12 jgg Exp $
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

class pkgDPkgPM : public pkgPackageManager
{
   protected:
   
   struct Item
   {
      enum Ops {Install, Configure, Remove} Op;
      string File;
      PkgIterator Pkg;
      Item(Ops Op,PkgIterator Pkg,string File = "") : Op(Op), 
            File(File), Pkg(Pkg) {};
      Item() {};
      
   };
   vector<Item> List;
      
   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,string File);
   virtual bool Configure(PkgIterator Pkg);
   virtual bool Remove(PkgIterator Pkg);
   virtual bool Go();
   
   public:

   pkgDPkgPM(pkgDepCache &Cache);
   virtual ~pkgDPkgPM();
};

#endif

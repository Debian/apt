// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkginit.h,v 1.1 1998/11/23 07:03:11 jgg Exp $
/* ######################################################################

   DPKG init - Initialize the dpkg stuff
   
   This basically gets a lock in /var/lib/dpkg and checks the updates
   directory
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DPKGINIT_H
#define PKGLIB_DPKGINIT_H

#ifdef __GNUG__
#pragma interface "apt-pkg/dpkginit.h"
#endif

class pkgDpkgLock
{
   int LockFD;
      
   public:
   
   bool GetLock();
   void Close();
   
   pkgDpkgLock();
   ~pkgDpkgLock();
};

#endif

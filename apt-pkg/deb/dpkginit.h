// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: dpkginit.h,v 1.2 1999/07/26 17:46:08 jgg Exp $
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
   
   bool CheckUpdates();
   bool GetLock(bool WithUpdates);
   void Close();

   pkgDpkgLock(bool WithUpdates = true);
   ~pkgDpkgLock();
};

#endif

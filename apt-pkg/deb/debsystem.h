// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsystem.h,v 1.2 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   System - Debian version of the  System Class

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSYSTEM_H
#define PKGLIB_DEBSYSTEM_H

#ifdef __GNUG__
#pragma interface "apt-pkg/debsystem.h"
#endif

#include <apt-pkg/pkgsystem.h>
    
class debSystem : public pkgSystem
{
   // For locking support
   int LockFD;
   unsigned LockCount;
   bool CheckUpdates();
   
   public:

   virtual bool Lock();
   virtual bool UnLock(bool NoErrors = false);   
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const;
   virtual bool Initialize(Configuration &Cnf);
   virtual bool ArchiveSupported(const char *Type);
   virtual signed Score(Configuration const &Cnf);
   virtual bool AddStatusFiles(vector<pkgIndexFile *> &List);

   debSystem();
};

extern debSystem debSys;

#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsystem.h,v 1.4 2003/01/11 07:16:33 jgg Exp $
/* ######################################################################

   System - Debian version of the  System Class

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSYSTEM_H
#define PKGLIB_DEBSYSTEM_H

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/pkgcache.h>

class debSystemPrivate;
class debStatusIndex;
class pkgDepCache;

class debSystem : public pkgSystem
{
   // private d-pointer
   debSystemPrivate *d;
   bool CheckUpdates();

   public:

   virtual bool Lock();
   virtual bool UnLock(bool NoErrors = false);   
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const;
   virtual bool Initialize(Configuration &Cnf);
   virtual bool ArchiveSupported(const char *Type);
   virtual signed Score(Configuration const &Cnf);
   virtual bool AddStatusFiles(std::vector<pkgIndexFile *> &List);
   virtual bool FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const;

   debSystem();
   virtual ~debSystem();
};

extern debSystem debSys;

#endif

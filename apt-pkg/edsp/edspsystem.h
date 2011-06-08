// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debsystem.h,v 1.4 2003/01/11 07:16:33 jgg Exp $
/* ######################################################################

   System - Debian version of the  System Class

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPSYSTEM_H
#define PKGLIB_EDSPSYSTEM_H

#include <apt-pkg/pkgsystem.h>

class edspIndex;
class edspSystem : public pkgSystem
{
   /** \brief dpointer placeholder (for later in case we need it) */
   void *d;

   edspIndex *StatusFile;

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

   edspSystem();
   ~edspSystem();
};

extern edspSystem edspSys;

#endif

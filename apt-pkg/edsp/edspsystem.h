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
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/pkgcache.h>

#include <memory>
#include <vector>

#include <apt-pkg/macros.h>

class Configuration;
class pkgDepCache;
class pkgIndexFile;
class pkgPackageManager;

class APT_HIDDEN edspLikeSystem : public pkgSystem
{
protected:
   std::unique_ptr<pkgIndexFile> StatusFile;

public:
   virtual bool Lock() APT_OVERRIDE APT_CONST;
   virtual bool UnLock(bool NoErrors = false) APT_OVERRIDE APT_CONST;
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const APT_OVERRIDE APT_CONST;
   virtual bool Initialize(Configuration &Cnf) APT_OVERRIDE;
   virtual bool ArchiveSupported(const char *Type) APT_OVERRIDE APT_CONST;
   virtual signed Score(Configuration const &Cnf) APT_OVERRIDE;
   virtual bool FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const APT_OVERRIDE;

   edspLikeSystem(char const * const Label);
   virtual ~edspLikeSystem();
};

class APT_HIDDEN edspSystem : public edspLikeSystem
{
   std::string tempDir;
   std::string tempStatesFile;
   std::string tempPrefsFile;

public:
   virtual bool Initialize(Configuration &Cnf) APT_OVERRIDE;
   virtual bool AddStatusFiles(std::vector<pkgIndexFile *> &List) APT_OVERRIDE;

   edspSystem();
   virtual ~edspSystem();
};

class APT_HIDDEN eippSystem : public edspLikeSystem
{
   public:
   virtual bool AddStatusFiles(std::vector<pkgIndexFile *> &List) APT_OVERRIDE;

   eippSystem();
   virtual ~eippSystem();
};

#endif

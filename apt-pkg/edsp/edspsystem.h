// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   System - Debian version of the  System Class

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_EDSPSYSTEM_H
#define PKGLIB_EDSPSYSTEM_H

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/error.h>

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
   virtual bool Lock(OpProgress * const Progress) APT_OVERRIDE APT_PURE;
   virtual bool UnLock(bool NoErrors = false) APT_OVERRIDE APT_PURE;
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const APT_OVERRIDE APT_PURE;
   virtual bool Initialize(Configuration &Cnf) APT_OVERRIDE;
   virtual bool ArchiveSupported(const char *Type) APT_OVERRIDE APT_PURE;
   virtual signed Score(Configuration const &Cnf) APT_OVERRIDE;
   virtual bool FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const APT_OVERRIDE;

   bool MultiArchSupported() const override { return true; }
   std::vector<std::string> ArchitecturesSupported() const override { return {}; };

   bool LockInner(OpProgress * const, int) override { return _error->Error("LockInner is not implemented"); };
   bool UnLockInner(bool) override { return _error->Error("UnLockInner is not implemented"); };
   bool IsLocked() override { return true; };

   explicit edspLikeSystem(char const * const Label);
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

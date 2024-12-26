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
   bool Lock(OpProgress *Progress) override APT_PURE;
   bool UnLock(bool NoErrors = false) override APT_PURE;
   pkgPackageManager *CreatePM(pkgDepCache *Cache) const override APT_PURE;
   bool Initialize(Configuration &Cnf) override;
   bool ArchiveSupported(const char *Type) override APT_PURE;
   signed Score(Configuration const &Cnf) override;
   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const override;

   [[nodiscard]] bool MultiArchSupported() const override { return true; }
   [[nodiscard]] std::vector<std::string> ArchitecturesSupported() const override { return {}; };

   bool LockInner(OpProgress * const /*Progress*/, int /*timeOutSec*/) override { return _error->Error("LockInner is not implemented"); };
   bool UnLockInner(bool /*NoErrors*/) override { return _error->Error("UnLockInner is not implemented"); };
   bool IsLocked() override { return true; };

   explicit edspLikeSystem(char const * const Label);
   ~edspLikeSystem() override;
};

class APT_HIDDEN edspSystem : public edspLikeSystem
{
   std::string tempDir;
   std::string tempStatesFile;
   std::string tempPrefsFile;

public:
   bool Initialize(Configuration &Cnf) override;
   bool AddStatusFiles(std::vector<pkgIndexFile *> &List) override;

   edspSystem();
   ~edspSystem() override;
};

class APT_HIDDEN eippSystem : public edspLikeSystem
{
   public:
   bool AddStatusFiles(std::vector<pkgIndexFile *> &List) override;

   eippSystem();
   ~eippSystem() override;
};

#endif

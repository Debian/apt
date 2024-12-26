// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   System - Debian version of the  System Class

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_DEBSYSTEM_H
#define PKGLIB_DEBSYSTEM_H

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>

#include <vector>
class Configuration;
class pkgIndexFile;
class pkgPackageManager;
class debSystemPrivate;
class pkgDepCache;


class debSystem : public pkgSystem
{
   // private d-pointer
   debSystemPrivate * const d;
   APT_HIDDEN bool CheckUpdates();

   public:
   bool Lock(OpProgress *Progress) override;
   bool UnLock(bool NoErrors = false) override;
   pkgPackageManager *CreatePM(pkgDepCache *Cache) const override;
   bool Initialize(Configuration &Cnf) override;
   bool ArchiveSupported(const char *Type) override;
   signed Score(Configuration const &Cnf) override;
   bool AddStatusFiles(std::vector<pkgIndexFile *> &List) override;
   bool FindIndex(pkgCache::PkgFileIterator File,
		  pkgIndexFile *&Found) const override;

   debSystem();
   ~debSystem() override;

   APT_HIDDEN static std::string GetDpkgExecutable();
   APT_HIDDEN static std::vector<std::string> GetDpkgBaseCommand();
   APT_HIDDEN static void DpkgChrootDirectory();
   APT_HIDDEN static std::string StripDpkgChrootDirectory(std::string const &File);
   APT_HIDDEN static pid_t ExecDpkg(std::vector<std::string> const &sArgs, int * const inputFd, int * const outputFd, bool const DiscardOutput);
   bool MultiArchSupported() const override;
   static bool AssertFeature(std::string const &Feature);
   std::vector<std::string> ArchitecturesSupported() const override;

   bool LockInner(OpProgress *const Progress, int timeoutSec) override;
   bool UnLockInner(bool NoErrors=false) override;
   bool IsLocked() override;
};

extern debSystem debSys;

#endif

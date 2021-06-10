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
   virtual bool Lock(OpProgress *const Progress) APT_OVERRIDE;
   virtual bool UnLock(bool NoErrors = false) APT_OVERRIDE;
   virtual pkgPackageManager *CreatePM(pkgDepCache *Cache) const APT_OVERRIDE;
   virtual bool Initialize(Configuration &Cnf) APT_OVERRIDE;
   virtual bool ArchiveSupported(const char *Type) APT_OVERRIDE;
   virtual signed Score(Configuration const &Cnf) APT_OVERRIDE;
   virtual bool AddStatusFiles(std::vector<pkgIndexFile *> &List) APT_OVERRIDE;
   virtual bool FindIndex(pkgCache::PkgFileIterator File,
			  pkgIndexFile *&Found) const APT_OVERRIDE;

   debSystem();
   virtual ~debSystem();

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

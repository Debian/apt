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
#include <apt-pkg/cacheiterators.h>

#include <vector>
class Configuration;
class pkgIndexFile;
class pkgPackageManager;
class debSystemPrivate;
class pkgDepCache;

#ifndef APT_10_CLEANER_HEADERS
class debStatusIndex;
#endif

class debSystem : public pkgSystem
{
   // private d-pointer
   debSystemPrivate * const d;
   APT_HIDDEN bool CheckUpdates();

   public:

   virtual bool Lock() APT_OVERRIDE;
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
   APT_HIDDEN static pid_t ExecDpkg(std::vector<std::string> const &sArgs, int * const inputFd, int * const outputFd, bool const DiscardOutput);
   APT_HIDDEN static bool SupportsMultiArch();
   APT_HIDDEN static std::vector<std::string> SupportedArchitectures();
};

extern debSystem debSys;

#endif

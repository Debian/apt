#ifndef APT_PRIVATE_INSTALL_H
#define APT_PRIVATE_INSTALL_H

#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/macros.h>

#include <list>
#include <string>
#include <utility>

class CacheFile;
class CommandLine;
class pkgProblemResolver;

#define RAMFS_MAGIC     0x858458f6

APT_PUBLIC bool DoInstall(CommandLine &Cmd);

bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, CacheFile &Cache,
                                        std::map<unsigned short, APT::VersionSet> &verset, int UpgradeMode);
bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, CacheFile &Cache, int UpgradeMode);

APT_PUBLIC bool InstallPackages(CacheFile &Cache,bool ShwKept,bool Ask = true,
                        bool Safety = true);


// TryToInstall - Mark a package for installation			/*{{{*/
struct APT_PUBLIC TryToInstall {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool FixBroken;
   unsigned long AutoMarkChanged;
   APT::PackageSet doAutoInstallLater;

   TryToInstall(pkgCacheFile &Cache, pkgProblemResolver *PM, bool const FixBroken) : Cache(&Cache), Fix(PM),
			FixBroken(FixBroken), AutoMarkChanged(0) {};

   void operator() (pkgCache::VerIterator const &Ver);
   bool propergateReleaseCandiateSwitching(std::list<std::pair<pkgCache::VerIterator, std::string> > const &start, std::ostream &out);
   void doAutoInstall();
};
									/*}}}*/
// TryToRemove - Mark a package for removal				/*{{{*/
struct APT_PUBLIC TryToRemove {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool PurgePkgs;

   TryToRemove(pkgCacheFile &Cache, pkgProblemResolver *PM) : Cache(&Cache), Fix(PM),
				PurgePkgs(_config->FindB("APT::Get::Purge", false)) {};

   void operator() (pkgCache::VerIterator const &Ver);
};
									/*}}}*/


#endif

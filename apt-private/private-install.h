#ifndef APT_PRIVATE_INSTALL_H
#define APT_PRIVATE_INSTALL_H

#include <apt-pkg/depcache.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <list>
#include <string>
#include <utility>

class CacheFile;
class CommandLine;
class pkgProblemResolver;

APT_PUBLIC bool DoInstall(CommandLine &Cmd);

struct PseudoPkg
{
   std::string name;
   std::string arch;
   std::string release;
   ssize_t index;
   PseudoPkg(std::string const &n, std::string const &a, std::string const &r) : name(n), arch(a), release(r), index(-1) {}
   PseudoPkg(std::string const &n, std::string const &a, std::string const &r, ssize_t i) : name(n), arch(a), release(r), index(i) {}
};
std::vector<PseudoPkg> GetAllPackagesAsPseudo(pkgSourceList *const SL, CommandLine &CmdL, bool (*Add)(pkgSourceList *const, PseudoPkg &&, std::vector<PseudoPkg> &), std::string const &pseudoArch);
std::vector<PseudoPkg> GetPseudoPackages(pkgSourceList *const SL, CommandLine &CmdL, bool (*Add)(pkgSourceList *const, PseudoPkg &&, std::vector<PseudoPkg> &), std::string const &pseudoArch);
bool AddVolatileBinaryFile(pkgSourceList *const SL, PseudoPkg &&pkg, std::vector<PseudoPkg> &VolatileCmdL);
bool AddVolatileSourceFile(pkgSourceList *const SL, PseudoPkg &&pkg, std::vector<PseudoPkg> &VolatileCmdL);

bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, std::vector<PseudoPkg> &VolatileCmdL, CacheFile &Cache,
					std::map<unsigned short, APT::VersionSet> &verset, int UpgradeMode,
					std::set<std::string> &UnknownPackages, APT::PackageVector &HeldBackPackages);
bool DoCacheManipulationFromCommandLine(CommandLine &CmdL, std::vector<PseudoPkg> &VolatileCmdL, CacheFile &Cache, int UpgradeMode,
					APT::PackageVector &HeldBackPackages);

APT_PUBLIC bool InstallPackages(CacheFile &Cache,
				APT::PackageVector &HeldBackPackages,
				bool ShwKept, bool Ask = true,
				bool Safety = true,
				std::string const &Hook = "",
				CommandLine const &CmdL = {});

bool CheckNothingBroken(CacheFile &Cache);
bool DoAutomaticRemove(CacheFile &Cache);

// TryToInstall - Mark a package for installation			/*{{{*/
struct TryToInstall {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool FixBroken;
   unsigned long AutoMarkChanged;
   APT::PackageSet doAutoInstallLater;

   TryToInstall(pkgCacheFile &Cache, pkgProblemResolver *PM, bool const FixBroken) : Cache(&Cache), Fix(PM),
			FixBroken(FixBroken), AutoMarkChanged(0) {};

   void operator() (pkgCache::VerIterator const &Ver);
   bool propagateReleaseCandidateSwitching(std::list<std::pair<pkgCache::VerIterator, std::string> > const &start, std::ostream &out);
   void doAutoInstall();
};
									/*}}}*/
// TryToRemove - Mark a package for removal				/*{{{*/
struct TryToRemove {
   pkgCacheFile* Cache;
   pkgProblemResolver* Fix;
   bool PurgePkgs;

   TryToRemove(pkgCacheFile &Cache, pkgProblemResolver *PM) : Cache(&Cache), Fix(PM),
				PurgePkgs(_config->FindB("APT::Get::Purge", false)) {};

   void operator() (pkgCache::VerIterator const &Ver);
};
									/*}}}*/


#endif

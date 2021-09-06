#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-private/private-cmndline.h>

#include <iostream>

static bool ShowHelp(CommandLine &)					/*{{{*/
{
   std::cout <<
    "Usage: longest-dependecy-chain [options]\n"
      "\n"
      "Tries to find the longest dependency chain available in the data\n"
      "assuming an empty status file, no conflicts, all or-group members\n"
      "are followed and discovery order matters. In other words:\n"
      "The found length might very well be too short and not realistic.\n"
      "It is also not implemented very intelligently, so it runs forever.\n";
   return true;
}
									/*}}}*/
static std::vector<aptDispatchWithHelp> GetCommands()			/*{{{*/
{
   return {
      {nullptr, nullptr, nullptr}
   };
}
									/*}}}*/
static size_t findLongestInstallChain(pkgDepCache &Cache, pkgCache::PkgIterator const &Pkg, std::vector<bool> &installed)/*{{{*/
{
   if (installed[Pkg->ID])
      return 0;
   installed[Pkg->ID] = true;

   auto const Ver = Cache.GetCandidateVersion(Pkg);
   if (Ver.end())
      return 0;

   size_t maxdepth = 0;
   for (auto D = Ver.DependsList(); not D.end(); ++D)
      if (D->Type == pkgCache::Dep::Depends ||
	    D->Type == pkgCache::Dep::PreDepends ||
	    D->Type == pkgCache::Dep::Recommends ||
	    D->Type == pkgCache::Dep::Suggests)
	 maxdepth = std::max(maxdepth, findLongestInstallChain(Cache, D.TargetPkg(), installed));
   return maxdepth + 1;
}
									/*}}}*/
int main(int argc,const char *argv[])					/*{{{*/
{
   CommandLine CmdL;
   auto const Cmds = ParseCommandLine(CmdL, APT_CMD::APT_SORTPKG, &_config, &_system, argc, argv, &ShowHelp, &GetCommands);
   _config->Set("dir::state::status", "/dev/null");

   pkgCacheFile CacheFile;
   CacheFile.InhibitActionGroups(true);
   pkgDepCache * const Cache = CacheFile.GetDepCache();
   if (unlikely(Cache == nullptr))
      return DispatchCommandLine(CmdL, Cmds);

   size_t maxdepth = 0;
   for (auto P = Cache->PkgBegin(); not P.end(); ++P)
   {
      std::vector<bool> installed(Cache->Head().PackageCount, false);
      auto const depth = findLongestInstallChain(*Cache, P, installed);
      std::cout << depth << ' ' << P.FullName() << '\n';
      maxdepth = std::max(maxdepth, depth);
   }

   return 0;
}
									/*}}}*/

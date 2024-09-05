#include <config.h>

#include <apt-pkg/init.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/error.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/prettyprinters.h>

#include <iostream>
#include <string>

static bool test(const char *name, bool hold)
{
   pkgCacheFile cache;
   if (not cache.Open())
      return false;

   for (auto Pkg = cache->PkgBegin(); Pkg.end() == false; ++Pkg)
      std::cout << "A: " << APT::PrettyPkg(cache, Pkg) << std::endl;

   cache->MarkDelete(cache->FindPkg(name));
   if (hold)
      cache->MarkProtected(cache->FindPkg(name));

   for (auto Pkg = cache->PkgBegin(); Pkg.end() == false; ++Pkg)
      std::cout << "B: " << APT::PrettyPkg(cache, Pkg) << std::endl;

   pkgProblemResolver resolve(cache);
   bool res = resolve.ResolveByKeep();

   for (auto Pkg = cache->PkgBegin(); Pkg.end() == false; ++Pkg)
      std::cout << "C: " << APT::PrettyPkg(cache, Pkg) << std::endl;

   return res;
}

int main(int argc, const char *argv[])
{
   if (argc != 2 && argc != 3)
   {
      return _error->Error("Expected one argument");
      goto err;
   }
   if (not pkgInitConfig(*_config))
      goto err;
   if (not pkgInitSystem(*_config, _system))
      goto err;
   if (not test(argv[1], argc > 2 && strcmp(argv[2], "--hold") == 0))
      goto err;

   return 0;
err:
   _error->DumpErrors();
   return 1;
}


#ifndef APT_PRIVATE_SEARCH_H
#define APT_PRIVATE_SEARCH_H

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoSearch(CommandLine &CmdL);
APT_PUBLIC void LocalitySort(pkgCache::VerFile ** const begin, unsigned long long const Count,size_t const Size);

#endif

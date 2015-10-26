#ifndef APT_PRIVATE_SHOW_H
#define APT_PRIVATE_SHOW_H

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>

#include <iostream>

class CommandLine;
class pkgCacheFile;

APT_PUBLIC bool ShowPackage(CommandLine &CmdL);
APT_PUBLIC bool DisplayRecordV1(pkgCacheFile &CacheFile, pkgCache::VerIterator const &V, std::ostream &out);
APT_PUBLIC bool ShowSrcPackage(CommandLine &CmdL);
APT_PUBLIC bool Policy(CommandLine &CmdL);

#endif

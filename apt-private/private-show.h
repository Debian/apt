#ifndef APT_PRIVATE_SHOW_H
#define APT_PRIVATE_SHOW_H

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>

#include <iostream>

class CommandLine;
class pkgCacheFile;

APT_PUBLIC bool ShowPackage(CommandLine &CmdL);
APT_PUBLIC bool ShowSrcPackage(CommandLine &CmdL);
APT_PUBLIC bool Policy(CommandLine &CmdL);

pkgRecords::Parser &LookupParser(pkgRecords &Recs, pkgCache::VerIterator const &V, pkgCache::VerFileIterator &Vf);
bool DisplayRecordV1(pkgCacheFile &CacheFile, pkgRecords &Recs,
		     pkgCache::VerIterator const &V, pkgCache::VerFileIterator const &Vf,
		     char const *Buffer, size_t const Length, std::ostream &out);

#endif

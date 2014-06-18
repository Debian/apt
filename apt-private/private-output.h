#ifndef APT_PRIVATE_OUTPUT_H
#define APT_PRIVATE_OUTPUT_H

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/macros.h>

#include <fstream>
#include <string>

// forward declaration
class pkgCacheFile;
class CacheFile;
class pkgDepCache;
class pkgRecords;


APT_PUBLIC extern std::ostream c0out;
APT_PUBLIC extern std::ostream c1out;
APT_PUBLIC extern std::ostream c2out;
APT_PUBLIC extern std::ofstream devnull;
APT_PUBLIC extern unsigned int ScreenWidth;

APT_PUBLIC bool InitOutput();

void ListSingleVersion(pkgCacheFile &CacheFile, pkgRecords &records,
                       pkgCache::VerIterator V, std::ostream &out,
                       bool include_summary=true);


// helper to describe global state
APT_PUBLIC void ShowBroken(std::ostream &out, CacheFile &Cache, bool const Now);
APT_PUBLIC void ShowBroken(std::ostream &out, pkgCacheFile &Cache, bool const Now);

APT_PUBLIC bool ShowList(std::ostream &out, std::string Title, std::string List,
              std::string VersionsList);
void ShowNew(std::ostream &out,CacheFile &Cache);
void ShowDel(std::ostream &out,CacheFile &Cache);
void ShowKept(std::ostream &out,CacheFile &Cache);
void ShowUpgraded(std::ostream &out,CacheFile &Cache);
bool ShowDowngraded(std::ostream &out,CacheFile &Cache);
bool ShowHold(std::ostream &out,CacheFile &Cache);

bool ShowEssential(std::ostream &out,CacheFile &Cache);

void Stats(std::ostream &out, pkgDepCache &Dep);

// prompting
bool YnPrompt(bool Default=true);
bool AnalPrompt(const char *Text);

#endif

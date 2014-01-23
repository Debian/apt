#ifndef APT_PRIVATE_OUTPUT_H
#define APT_PRIVATE_OUTPUT_H


#include <iostream>
#include <fstream>
#include <string>

#include "private-cachefile.h"

// forward declaration
class pkgCacheFile;
class CacheFile;
class pkgCache;
class pkgDepCache;
class pkgRecords;


extern std::ostream c0out;
extern std::ostream c1out;
extern std::ostream c2out;
extern std::ofstream devnull;
extern unsigned int ScreenWidth;

bool InitOutput();
void ListSingleVersion(pkgCacheFile &CacheFile, pkgRecords &records, 
                       pkgCache::VerIterator V, std::ostream &out,
                       bool include_summary=true);


// helper to describe global state
bool ShowList(std::ostream &out, std::string Title, std::string List,
              std::string VersionsList);
void ShowBroken(std::ostream &out,CacheFile &Cache,bool Now);
void ShowNew(std::ostream &out,CacheFile &Cache);
void ShowDel(std::ostream &out,CacheFile &Cache);
void ShowKept(std::ostream &out,CacheFile &Cache);
void ShowUpgraded(std::ostream &out,CacheFile &Cache);
bool ShowDowngraded(std::ostream &out,CacheFile &Cache);
bool ShowHold(std::ostream &out,CacheFile &Cache);

bool ShowEssential(std::ostream &out,CacheFile &Cache);

void Stats(std::ostream &out, pkgDepCache &Dep);

// helpers to display single package data
std::string
GetArchiveSuite(pkgCacheFile &CacheFile, pkgCache::VerIterator ver);

// prompting
bool YnPrompt(bool Default=true);
bool AnalPrompt(const char *Text);

#endif

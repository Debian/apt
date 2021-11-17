#ifndef APT_PRIVATE_OUTPUT_H
#define APT_PRIVATE_OUTPUT_H

#include <apt-pkg/cacheset.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <fstream>
#include <functional>
#include <iostream>
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

APT_PUBLIC bool InitOutput(std::basic_streambuf<char> * const out = std::cout.rdbuf());

void ListSingleVersion(pkgCacheFile &CacheFile, pkgRecords &records,
                       pkgCache::VerIterator const &V, std::ostream &out,
                       std::string const &format);


// helper to describe global state
APT_PUBLIC void ShowBroken(std::ostream &out, CacheFile &Cache, bool const Now);
APT_PUBLIC void ShowBroken(std::ostream &out, pkgCacheFile &Cache, bool const Now);

template<class Container, class PredicateC, class DisplayP, class DisplayV> bool ShowList(std::ostream &out, std::string const &Title,
      Container const &cont,
      PredicateC Predicate,
      DisplayP PkgDisplay,
      DisplayV VerboseDisplay)
{
   size_t const ScreenWidth = (::ScreenWidth > 3) ? ::ScreenWidth - 3 : 0;
   int ScreenUsed = 0;
   bool const ShowVersions = _config->FindB("APT::Get::Show-Versions", false);
   bool printedTitle = false;

   for (auto const &Pkg: cont)
   {
      if (Predicate(Pkg) == false)
	 continue;

      if (printedTitle == false)
      {
	 out << Title;
	 printedTitle = true;
      }

      if (ShowVersions == true)
      {
	 out << std::endl << "   " << PkgDisplay(Pkg);
	 std::string const verbose = VerboseDisplay(Pkg);
	 if (verbose.empty() == false)
	    out << " (" << verbose << ")";
      }
      else
      {
	 std::string const PkgName = PkgDisplay(Pkg);
	 if (ScreenUsed == 0 || (ScreenUsed + PkgName.length()) >= ScreenWidth)
	 {
	    out << std::endl << "  ";
	    ScreenUsed = 0;
	 }
	 else if (ScreenUsed != 0)
	 {
	    out << " ";
	    ++ScreenUsed;
	 }
	 out << PkgName;
	 ScreenUsed += PkgName.length();
      }
   }

   if (printedTitle == true)
   {
      out << std::endl;
      return false;
   }
   return true;
}

void ShowNew(std::ostream &out,CacheFile &Cache);
void ShowDel(std::ostream &out,CacheFile &Cache);
void ShowKept(std::ostream &out,CacheFile &Cache, APT::PackageVector const &HeldBackPackages);
void ShowUpgraded(std::ostream &out,CacheFile &Cache);
bool ShowDowngraded(std::ostream &out,CacheFile &Cache);
bool ShowHold(std::ostream &out,CacheFile &Cache);

bool ShowEssential(std::ostream &out,CacheFile &Cache);

void Stats(std::ostream &out, pkgDepCache &Dep, APT::PackageVector const &HeldBackPackages);

// prompting
APT_PUBLIC bool YnPrompt(char const *const Question, bool Default = true);
bool YnPrompt(char const * const Question, bool const Default, bool const ShowGlobalErrors, std::ostream &c1o, std::ostream &c2o);

APT_PUBLIC std::string PrettyFullName(pkgCache::PkgIterator const &Pkg);
std::string CandidateVersion(pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg);
std::function<std::string(pkgCache::PkgIterator const &)> CandidateVersion(pkgCacheFile * const Cache);
std::string CurrentToCandidateVersion(pkgCacheFile * const Cache, pkgCache::PkgIterator const &Pkg);
std::function<std::string(pkgCache::PkgIterator const &)> CurrentToCandidateVersion(pkgCacheFile * const Cache);
std::string EmptyString(pkgCache::PkgIterator const &);
bool AlwaysTrue(pkgCache::PkgIterator const &);

#endif

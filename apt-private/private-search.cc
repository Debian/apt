// Includes                      /*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-output.h>
#include <apt-private/private-search.h>
#include <apt-private/private-package-info.h>

#include <string.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <utility>
#include <apti18n.h>
                           /*}}}*/

bool FullTextSearch(CommandLine &CmdL)             /*{{{*/
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   // Make sure there is at least one argument
   unsigned int const NumPatterns = CmdL.FileSize() -1;
   if (NumPatterns < 1)
      return _error->Error(_("You must give at least one search pattern"));

   #define APT_FREE_PATTERNS() for (std::vector<regex_t>::iterator P = Patterns.begin(); \
      P != Patterns.end(); ++P) { regfree(&(*P)); }

   // Compile the regex pattern
   std::vector<regex_t> Patterns;
   for (unsigned int I = 0; I != NumPatterns; ++I)
   {
      regex_t pattern;
      if (regcomp(&pattern, CmdL.FileList[I + 1], REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0)
      {
         APT_FREE_PATTERNS();
         return _error->Error("Regex compilation error");
      }
      Patterns.push_back(pattern);
   }

   bool const NamesOnly = _config->FindB("APT::Cache::NamesOnly", false);

   std::vector<PackageInfo> outputVector;

   LocalitySortedVersionSet bag;
   OpTextProgress progress(*_config);
   progress.OverallProgress(0, 100, 50,  _("Sorting"));
   GetLocalitySortedVersionSet(CacheFile, &bag, &progress);

   progress.OverallProgress(50, 100, 50,  _("Full Text Search"));
   progress.SubProgress(bag.size());
   pkgRecords records(CacheFile);

   std::string format = "${color:highlight}${Package}${color:neutral}/${Origin} ${Version} ${Architecture}${ }${apt:Status}\n";
   if (_config->FindB("APT::Cache::ShowFull",false) == false)
      format += "  ${Description}\n";
   else
      format += "  ${LongDescription}\n";

   int Done = 0;
   std::vector<bool> PkgsDone(Cache->Head().PackageCount, false);
   for(auto V:bag)
   {
      if (Done%500 == 0)
         progress.Progress(Done);
      ++Done;

      // we want to list each package only once
      pkgCache::PkgIterator const P = V.ParentPkg();
      if (PkgsDone[P->ID] == true)
         continue;

      char const * const PkgName = P.Name();
      pkgCache::DescIterator Desc = V.TranslatedDescription();
      pkgRecords::Parser &parser = records.Lookup(Desc.FileList());
      std::string const LongDesc = parser.LongDesc();

      bool all_found = true;
      for (std::vector<regex_t>::const_iterator pattern = Patterns.begin();
       pattern != Patterns.end(); ++pattern)
      {
         if (regexec(&(*pattern), PkgName, 0, 0, 0) == 0)
            continue;
         else if (NamesOnly == false && regexec(&(*pattern), LongDesc.c_str(), 0, 0, 0) == 0)
          continue;
         // search patterns are AND, so one failing fails all
         all_found = false;
         break;
      }
      if (all_found == true)
      {
         PkgsDone[P->ID] = true;
         std::stringstream outs;
         ListSingleVersion(CacheFile, records, V, outs, format);
         PackageInfo pkg(CacheFile, records, V, outs.str());
         outputVector.push_back(pkg);
      }
   }
   if (_config->FindB("APT::Cache::OrderByStatus",0))
      std::sort(outputVector.begin(), outputVector.end(), OrderByStatus);
   else if (_config->FindB("APT::Cache::OrderByVersion",0))
      std::sort(outputVector.begin(), outputVector.end(), OrderByVersion);
   else if (_config->FindB("APT::Cache::OrderByReverseAlphabetic",0))
      std::sort(outputVector.begin(), outputVector.end(), OrderByReverseAlphabetic);
   else
      std::sort(outputVector.begin(), outputVector.end(), OrderByAlphabetic);

   APT_FREE_PATTERNS();
   progress.Done();
   
   // output the sorted vector
   for(auto k:outputVector)
      std::cout << k.formated_output() << std::endl;

   return true;
}
                           /*}}}*/


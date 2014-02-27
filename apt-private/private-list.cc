// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/init.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/version.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/metaindex.h>

#include <sstream>
#include <vector>
#include <utility>
#include <cassert>
#include <locale.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <algorithm>

#include "private-cmndline.h"
#include "private-list.h"
#include "private-output.h"
#include "private-cacheset.h"

#include <apti18n.h>
									/*}}}*/

struct PackageSortAlphabetic						/*{{{*/
{
   bool operator () (const pkgCache::PkgIterator &p_lhs, 
                     const pkgCache::PkgIterator &p_rhs)
    {
       const std::string &l_name = p_lhs.FullName(true);
       const std::string &r_name = p_rhs.FullName(true);
       return (l_name < r_name);
    }
};
									/*}}}*/
class PackageNameMatcher : public Matcher				/*{{{*/
{
#ifdef PACKAGE_MATCHER_ABI_COMPAT
#define PackageMatcher PackageNameMatchesFnmatch
#endif
  public:
   PackageNameMatcher(const char **patterns)
   {
      for(int i=0; patterns[i] != NULL; ++i)
      {
         std::string pattern = patterns[i];
#ifdef PACKAGE_MATCHER_ABI_COMPAT
            APT::CacheFilter::PackageNameMatchesFnmatch *cachefilter = NULL;
            cachefilter = new APT::CacheFilter::PackageNameMatchesFnmatch(pattern);
#else
         APT::CacheFilter::PackageMatcher *cachefilter = NULL;
         if(_config->FindB("APT::Cmd::Use-Regexp", false) == true)
            cachefilter = new APT::CacheFilter::PackageNameMatchesRegEx(pattern);
         else
            cachefilter = new APT::CacheFilter::PackageNameMatchesFnmatch(pattern);
#endif
         filters.push_back(cachefilter);
      }
   }
   virtual ~PackageNameMatcher()
   {
      for(J=filters.begin(); J != filters.end(); ++J)
         delete *J;
   }
   virtual bool operator () (const pkgCache::PkgIterator &P) 
   {
      for(J=filters.begin(); J != filters.end(); ++J)
      {
         APT::CacheFilter::PackageMatcher *cachefilter = *J;
         if((*cachefilter)(P)) 
            return true;
      }
      return false;
   }

private:
   std::vector<APT::CacheFilter::PackageMatcher*> filters;   
   std::vector<APT::CacheFilter::PackageMatcher*>::const_iterator J;
   #undef PackageMatcher
};
									/*}}}*/
static void ListAllVersions(pkgCacheFile &CacheFile, pkgRecords &records,/*{{{*/
                     pkgCache::PkgIterator P,    
                     std::ostream &outs,
                     bool include_summary=true)
{
   for (pkgCache::VerIterator Ver = P.VersionList();
        Ver.end() == false; ++Ver)
   {
      ListSingleVersion(CacheFile, records, Ver, outs, include_summary);
      outs << "\n";
   }
}
									/*}}}*/
// list - list package based on criteria        			/*{{{*/
// ---------------------------------------------------------------------
bool List(CommandLine &Cmd)
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgRecords records(CacheFile);

   if (unlikely(Cache == NULL))
      return false;

   const char **patterns;
   const char *all_pattern[] = { "*", NULL};

   if (strv_length(Cmd.FileList + 1) == 0)
   {
      patterns = all_pattern;
   } else {
      patterns = Cmd.FileList + 1;
   }

   std::map<std::string, std::string> output_map;
   std::map<std::string, std::string>::const_iterator K;

   bool includeSummary = _config->FindB("APT::Cmd::List-Include-Summary");

   PackageNameMatcher matcher(patterns);
   LocalitySortedVersionSet bag;
   OpTextProgress progress(*_config);
   progress.OverallProgress(0,
                            Cache->Head().PackageCount, 
                            Cache->Head().PackageCount,
                            _("Listing"));
   GetLocalitySortedVersionSet(CacheFile, bag, matcher, progress);
   for (LocalitySortedVersionSet::iterator V = bag.begin(); V != bag.end(); ++V)
   {
      std::stringstream outs;
      if(_config->FindB("APT::Cmd::All-Versions", false) == true)
      {
         ListAllVersions(CacheFile, records, V.ParentPkg(), outs, includeSummary);
         output_map.insert(std::make_pair<std::string, std::string>(
            V.ParentPkg().Name(), outs.str()));
      } else {
         ListSingleVersion(CacheFile, records, V, outs, includeSummary);
         output_map.insert(std::make_pair<std::string, std::string>(
                           V.ParentPkg().Name(), outs.str()));
      }
   }

   // FIXME: SORT! and make sorting flexible (alphabetic, by pkg status)
   // output the sorted map
   for (K = output_map.begin(); K != output_map.end(); ++K)
      std::cout << (*K).second << std::endl;


   return true;
}


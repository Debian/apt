// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cachefilter.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/strutl.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-list.h>
#include <apt-private/private-output.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

class PackageNameMatcher : public Matcher
{
   static constexpr const char *const isfnmatch_strict = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.:*";
   pkgCacheFile &cacheFile;
  public:
   explicit PackageNameMatcher(pkgCacheFile &cacheFile, const char **patterns)
   : cacheFile(cacheFile)
   {
      for(int i=0; patterns[i] != NULL; ++i)
      {
         std::string pattern = patterns[i];
         APT::CacheFilter::Matcher *cachefilter = NULL;
         if(_config->FindB("APT::Cmd::Use-Regexp", false) == true)
            cachefilter = new APT::CacheFilter::PackageNameMatchesRegEx(pattern);
         else if (pattern.find_first_not_of(isfnmatch_strict) == std::string::npos)
            cachefilter = new APT::CacheFilter::PackageNameMatchesFnmatch(pattern);
	 else
	    cachefilter = APT::CacheFilter::ParsePattern(pattern, &cacheFile).release();

         if (cachefilter == nullptr) {
            return;
            filters.clear();
         }
         filters.push_back(cachefilter);
      }
   }
   virtual ~PackageNameMatcher()
   {
      for(J=filters.begin(); J != filters.end(); ++J)
         delete *J;
   }
   virtual bool operator () (const pkgCache::PkgIterator &P) APT_OVERRIDE
   {
      for(J=filters.begin(); J != filters.end(); ++J)
      {
         APT::CacheFilter::Matcher *cachefilter = *J;
         if((*cachefilter)(P)) 
            return true;
      }
      return false;
   }

private:
   std::vector<APT::CacheFilter::Matcher*> filters;
   std::vector<APT::CacheFilter::Matcher*>::const_iterator J;
   #undef PackageMatcher
};
									/*}}}*/
static void ListAllVersions(pkgCacheFile &CacheFile, pkgRecords &records,/*{{{*/
                     pkgCache::PkgIterator const &P, std::ostream &outs,
                     std::string const &format)
{
   for (pkgCache::VerIterator Ver = P.VersionList();
        Ver.end() == false; ++Ver)
   {
      ListSingleVersion(CacheFile, records, Ver, outs, format);
      outs << std::endl;
   }
}
									/*}}}*/
// list - list package based on criteria        			/*{{{*/
// ---------------------------------------------------------------------
bool DoList(CommandLine &Cmd)
{
   pkgCacheFile CacheFile;
   pkgCache * const Cache = CacheFile.GetPkgCache();
   if (unlikely(Cache == nullptr || CacheFile.GetDepCache() == nullptr))
      return false;
   pkgRecords records(CacheFile);

   const char **patterns;
   const char *all_pattern[] = { "*", NULL};

   if (strv_length(Cmd.FileList + 1) == 0)
   {
      patterns = all_pattern;
   } else {
      patterns = Cmd.FileList + 1;
   }

   std::string format = "${color:highlight}${Package}${color:neutral}/${Origin} ${Version} ${Architecture}${ }${apt:Status}";
   if (_config->FindB("APT::Cmd::List-Include-Summary", false) == true)
      format += "\n  ${Description}\n";

   PackageNameMatcher matcher(CacheFile, patterns);
   LocalitySortedVersionSet bag;
   OpTextProgress progress(*_config);
   progress.OverallProgress(0,
                            Cache->Head().PackageCount, 
                            Cache->Head().PackageCount,
                            _("Listing"));
   GetLocalitySortedVersionSet(CacheFile, &bag, matcher, &progress);
   bool const ShowAllVersions = _config->FindB("APT::Cmd::All-Versions", false);
   std::map<std::string, std::string> output_map;
   for (LocalitySortedVersionSet::iterator V = bag.begin(); V != bag.end(); ++V)
   {
      std::stringstream outs;
      if(ShowAllVersions == true)
         ListAllVersions(CacheFile, records, V.ParentPkg(), outs, format);
      else
         ListSingleVersion(CacheFile, records, V, outs, format);
      output_map.insert(std::make_pair<std::string, std::string>(
	       V.ParentPkg().FullName(), outs.str()));
   }

   // FIXME: SORT! and make sorting flexible (alphabetic, by pkg status)
   // output the sorted map
   std::map<std::string, std::string>::const_iterator K;
   for (K = output_map.begin(); K != output_map.end(); ++K)
      std::cout << (*K).second << std::endl;

   // be nice and tell the user if there is more to see
   if (bag.size() == 1 && ShowAllVersions == false)
   {
      // start with -1 as we already displayed one version
      int versions = -1;
      pkgCache::VerIterator Ver = *bag.begin();
      for ( ; Ver.end() == false; ++Ver)
         ++versions;
      if (versions > 0)
         _error->Notice(P_("There is %i additional version. Please use the '-a' switch to see it", "There are %i additional versions. Please use the '-a' switch to see them.", versions), versions);
   }

   return true;
}


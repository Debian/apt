// Includes								/*{{{*/
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
#include <utility>
#include <cassert>
#include <locale.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <iomanip>
#include <algorithm>
#include <map>

#include "private-search.h"
#include "private-cacheset.h"
									/*}}}*/

bool FullTextSearch(CommandLine &CmdL)					/*{{{*/
{
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   pkgRecords records(CacheFile);
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   const char **patterns;
   patterns = CmdL.FileList + 1;

   std::map<std::string, std::string> output_map;
   std::map<std::string, std::string>::const_iterator K;

   LocalitySortedVersionSet bag;
   OpTextProgress progress;
   progress.OverallProgress(0, 100, 50,  _("Sorting"));
   GetLocalitySortedVersionSet(CacheFile, bag, progress);
   LocalitySortedVersionSet::iterator V = bag.begin();

   progress.OverallProgress(50, 100, 50,  _("Full Text Search"));
   progress.SubProgress(bag.size());
   int Done = 0;
   for ( ;V != bag.end(); V++)
   {
      if (Done%500 == 0)
         progress.Progress(Done);
      Done++;
      
      int i;
      pkgCache::DescIterator Desc = V.TranslatedDescription();
      pkgRecords::Parser &parser = records.Lookup(Desc.FileList());
     
      bool all_found = true;
      for(i=0; patterns[i] != NULL; i++) 
      {
         // FIXME: use regexp instead of simple find()
         const char *pattern = patterns[i];
         all_found &=  (
            strstr(V.ParentPkg().Name(), pattern) != NULL ||
            parser.ShortDesc().find(pattern) != std::string::npos ||
            parser.LongDesc().find(pattern) != std::string::npos);
      }
      if (all_found)
      {
            std::stringstream outs;
            ListSingleVersion(CacheFile, records, V, outs);
            output_map.insert(std::make_pair<std::string, std::string>(
                                 V.ParentPkg().Name(), outs.str()));
      }
   }
   progress.Done();

   // FIXME: SORT! and make sorting flexible (alphabetic, by pkg status)
   // output the sorted map
   for (K = output_map.begin(); K != output_map.end(); K++)
      std::cout << (*K).second << std::endl;

   return true;
}
									/*}}}*/

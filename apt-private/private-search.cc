// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>

#include <apt-private/private-cachefile.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-json-hooks.h>
#include <apt-private/private-output.h>
#include <apt-private/private-search.h>
#include <apt-private/private-show.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <string.h>

#include <apti18n.h>
									/*}}}*/

static std::vector<pkgCache::DescIterator> const TranslatedDescriptionsList(pkgCache::VerIterator const &V) /*{{{*/
{
   std::vector<pkgCache::DescIterator> Descriptions;

   for (std::string const &lang: APT::Configuration::getLanguages())
   {
      pkgCache::DescIterator Desc = V.TranslatedDescriptionForLanguage(lang);
      if (Desc.IsGood())
         Descriptions.push_back(Desc);
   }

   if (Descriptions.empty() && V.TranslatedDescription().IsGood())
      Descriptions.push_back(V.TranslatedDescription());

   return Descriptions;
}

									/*}}}*/
static bool FullTextSearch(CommandLine &CmdL)				/*{{{*/
{

   CacheFile CacheFile;
   CacheFile.GetDepCache();
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   // Make sure there is at least one argument
   unsigned int const NumPatterns = CmdL.FileSize() -1;
   if (NumPatterns < 1)
      return _error->Error(_("You must give at least one search pattern"));

   RunJsonHook("AptCli::Hooks::Search", "org.debian.apt.hooks.search.pre", CmdL.FileList, CacheFile);

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

   std::map<std::string, std::string> output_map;

   LocalitySortedVersionSet bag;
   OpTextProgress progress(*_config);
   progress.OverallProgress(0, 100, 50,  _("Sorting"));
   GetLocalitySortedVersionSet(CacheFile, &bag, &progress);
   LocalitySortedVersionSet::iterator V = bag.begin();

   progress.OverallProgress(50, 100, 50,  _("Full Text Search"));
   progress.SubProgress(bag.size());
   pkgRecords records(CacheFile);

   std::string format = "${color:highlight}${Package}${color:neutral}/${Origin} ${Version} ${Architecture}${ }${apt:Status}\n";
   if (_config->FindB("APT::Cache::ShowFull",false) == false)
      format += "  ${Description}\n";
   else
      format += "  ${LongDescription}\n";

   bool const NamesOnly = _config->FindB("APT::Cache::NamesOnly", false);
   int Done = 0;
   std::vector<bool> PkgsDone(Cache->Head().PackageCount, false);
   for ( ;V != bag.end(); ++V)
   {
      if (Done%500 == 0)
         progress.Progress(Done);
      ++Done;

      // we want to list each package only once
      pkgCache::PkgIterator const P = V.ParentPkg();
      if (PkgsDone[P->ID] == true)
	 continue;

      std::vector<std::string> PkgDescriptions;
      if (not NamesOnly)
      {
         for (auto &Desc: TranslatedDescriptionsList(V))
         {
            pkgRecords::Parser &parser = records.Lookup(Desc.FileList());
            PkgDescriptions.push_back(parser.LongDesc());
         }
      }

      bool all_found = true;

      char const * const PkgName = P.Name();
      std::vector<bool> SkipDescription(PkgDescriptions.size(), false);
      for (std::vector<regex_t>::const_iterator pattern = Patterns.begin();
           pattern != Patterns.end(); ++pattern)
      {
         if (regexec(&(*pattern), PkgName, 0, 0, 0) == 0)
            continue;
         else if (not NamesOnly)
         {
            bool found = false;

            for (std::vector<std::string>::size_type i = 0; i < PkgDescriptions.size(); ++i)
            {
               if (not SkipDescription[i])
               {
                  if (regexec(&(*pattern), PkgDescriptions[i].c_str(), 0, 0, 0) == 0)
                     found = true;
                  else
                     SkipDescription[i] = true;
               }
            }

            if (found)
               continue;
         }

         // search patterns are AND, so one failing fails all
         all_found = false;
         break;
      }

      if (all_found == true)
      {
	 PkgsDone[P->ID] = true;
	 std::stringstream outs;
	 ListSingleVersion(CacheFile, records, V, outs, format);
	 output_map.insert(std::make_pair<std::string, std::string>(
		  PkgName, outs.str()));
      }
   }
   APT_FREE_PATTERNS();
   progress.Done();

   // FIXME: SORT! and make sorting flexible (alphabetic, by pkg status)
   // output the sorted map
   std::map<std::string, std::string>::const_iterator K;
   for (K = output_map.begin(); K != output_map.end(); ++K)
      std::cout << (*K).second << std::endl;

   if (output_map.empty())
      RunJsonHook("AptCli::Hooks::Search", "org.debian.apt.hooks.search.fail", CmdL.FileList, CacheFile);
   else
      RunJsonHook("AptCli::Hooks::Search", "org.debian.apt.hooks.search.post", CmdL.FileList, CacheFile);
   return true;
}
									/*}}}*/
// LocalitySort - Sort a version list by package file locality		/*{{{*/
static int LocalityCompare(const void * const a, const void * const b)
{
   pkgCache::VerFile const * const A = *static_cast<pkgCache::VerFile const * const *>(a);
   pkgCache::VerFile const * const B = *static_cast<pkgCache::VerFile const * const *>(b);

   if (A == 0 && B == 0)
      return 0;
   if (A == 0)
      return 1;
   if (B == 0)
      return -1;

   if (A->File == B->File)
      return A->Offset - B->Offset;
   return A->File - B->File;
}
void LocalitySort(pkgCache::VerFile ** const begin, unsigned long long const Count,size_t const Size)
{
   qsort(begin,Count,Size,LocalityCompare);
}
static void LocalitySort(pkgCache::DescFile ** const begin, unsigned long long const Count,size_t const Size)
{
   qsort(begin,Count,Size,LocalityCompare);
}
									/*}}}*/
// Search - Perform a search						/*{{{*/
// ---------------------------------------------------------------------
/* This searches the package names and package descriptions for a pattern */
struct ExDescFile
{
   pkgCache::DescFile *Df;
   pkgCache::VerIterator V;
   map_id_t ID;
   ExDescFile() : Df(nullptr), ID(0) {}
};
static bool Search(CommandLine &CmdL)
{
   bool const ShowFull = _config->FindB("APT::Cache::ShowFull",false);
   unsigned int const NumPatterns = CmdL.FileSize() -1;
   
   pkgCacheFile CacheFile;
   pkgCache *Cache = CacheFile.GetPkgCache();
   pkgDepCache::Policy *Plcy = CacheFile.GetPolicy();
   if (unlikely(Cache == NULL || Plcy == NULL))
      return false;

   // Make sure there is at least one argument
   if (NumPatterns < 1)
      return _error->Error(_("You must give at least one search pattern"));
   
   // Compile the regex pattern
   regex_t *Patterns = new regex_t[NumPatterns];
   memset(Patterns,0,sizeof(*Patterns)*NumPatterns);
   for (unsigned I = 0; I != NumPatterns; I++)
   {
      if (regcomp(&Patterns[I],CmdL.FileList[I+1],REG_EXTENDED | REG_ICASE | 
		  REG_NOSUB) != 0)
      {
	 for (; I != 0; I--)
	    regfree(&Patterns[I]);
	 return _error->Error("Regex compilation error");
      }      
   }
   
   if (_error->PendingError() == true)
   {
      for (unsigned I = 0; I != NumPatterns; I++)
	 regfree(&Patterns[I]);
      return false;
   }
   
   size_t const descCount = Cache->HeaderP->GroupCount + 1;
   ExDescFile *DFList = new ExDescFile[descCount];

   bool *PatternMatch = new bool[descCount * NumPatterns];
   memset(PatternMatch,false,sizeof(*PatternMatch) * descCount * NumPatterns);

   // Map versions that we want to write out onto the VerList array.
   bool const NamesOnly = _config->FindB("APT::Cache::NamesOnly",false);
   for (pkgCache::GrpIterator G = Cache->GrpBegin(); G.end() == false; ++G)
   {
      size_t const PatternOffset = G->ID * NumPatterns;
      size_t unmatched = 0, matched = 0;
      for (unsigned I = 0; I < NumPatterns; ++I)
      {
	 if (PatternMatch[PatternOffset + I] == true)
	    ++matched;
	 else if (regexec(&Patterns[I],G.Name(),0,0,0) == 0)
	    PatternMatch[PatternOffset + I] = true;
	 else
	    ++unmatched;
      }

      // already dealt with this package?
      if (matched == NumPatterns)
	 continue;

      // Doing names only, drop any that don't match..
      if (NamesOnly == true && unmatched == NumPatterns)
	 continue;

      // Find the proper version to use
      pkgCache::PkgIterator P = G.FindPreferredPkg();
      if (P.end() == true)
	 continue;
      pkgCache::VerIterator V = Plcy->GetCandidateVer(P);
      if (V.end() == false)
      {
	 pkgCache::DescIterator const D = V.TranslatedDescription();
	 //FIXME: packages without a description can't be found
	 if (D.end() == true)
	    continue;
	 DFList[G->ID].Df = D.FileList();
	 DFList[G->ID].V = V;
	 DFList[G->ID].ID = G->ID;
      }

      if (unmatched == NumPatterns)
	 continue;

      // Include all the packages that provide matching names too
      for (pkgCache::PrvIterator Prv = P.ProvidesList() ; Prv.end() == false; ++Prv)
      {
	 pkgCache::VerIterator V = Plcy->GetCandidateVer(Prv.OwnerPkg());
	 if (V.end() == true)
	    continue;

	 unsigned long id = Prv.OwnerPkg().Group()->ID;
	 pkgCache::DescIterator const D = V.TranslatedDescription();
	 //FIXME: packages without a description can't be found
	 if (D.end() == true)
	    continue;
	 DFList[id].Df = D.FileList();
	 DFList[id].V = V;
	 DFList[id].ID = id;

	 size_t const PrvPatternOffset = id * NumPatterns;
	 for (unsigned I = 0; I < NumPatterns; ++I)
	    PatternMatch[PrvPatternOffset + I] |= PatternMatch[PatternOffset + I];
      }
   }

   LocalitySort(&DFList->Df, Cache->HeaderP->GroupCount, sizeof(*DFList));

   // Create the text record parser
   pkgRecords Recs(*Cache);
   // Iterate over all the version records and check them
   for (ExDescFile *J = DFList; J->Df != 0; ++J)
   {
      size_t const PatternOffset = J->ID * NumPatterns;
      if (not NamesOnly)
      {
         std::vector<std::string> PkgDescriptions;
         for (auto &Desc: TranslatedDescriptionsList(J->V))
         {
            pkgRecords::Parser &parser = Recs.Lookup(Desc.FileList());
            PkgDescriptions.push_back(parser.LongDesc());
         }

         std::vector<bool> SkipDescription(PkgDescriptions.size(), false);
         for (unsigned I = 0; I < NumPatterns; ++I)
         {
            if (PatternMatch[PatternOffset + I])
               continue;
            else
            {
               bool found = false;

               for (std::vector<std::string>::size_type k = 0; k < PkgDescriptions.size(); ++k)
               {
                  if (not SkipDescription[k])
                  {
                     if (regexec(&Patterns[I], PkgDescriptions[k].c_str(), 0, 0, 0) == 0)
                     {
                        found = true;
                        PatternMatch[PatternOffset + I] = true;
                     }
                     else
                        SkipDescription[k] = true;
                  }
               }

               if (not found)
                  break;
            }
         }
      }

      bool matchedAll = true;
      for (unsigned I = 0; I < NumPatterns; ++I)
	 if (PatternMatch[PatternOffset + I] == false)
	 {
	    matchedAll = false;
	    break;
	 }

      if (matchedAll == true)
      {
	 if (ShowFull == true)
	 {
	    pkgCache::VerFileIterator Vf;
	    auto &Parser = LookupParser(Recs, J->V, Vf);
	    char const *Start, *Stop;
	    Parser.GetRec(Start, Stop);
	    size_t const Length = Stop - Start;
	    DisplayRecordV1(CacheFile, Recs, J->V, Vf, Start, Length, std::cout);
	 }
	 else
	 {
	    pkgRecords::Parser &P = Recs.Lookup(pkgCache::DescFileIterator(*Cache, J->Df));
	    printf("%s - %s\n", P.Name().c_str(), P.ShortDesc().c_str());
	 }
      }
   }
   
   delete [] DFList;
   delete [] PatternMatch;
   for (unsigned I = 0; I != NumPatterns; I++)
      regfree(&Patterns[I]);
   delete [] Patterns;
   if (ferror(stdout))
       return _error->Error("Write to stdout failed");
   return true;
}
									/*}}}*/
bool DoSearch(CommandLine &CmdL)					/*{{{*/
{
   int const ShowVersion = _config->FindI("APT::Cache::Search::Version", 1);
   if (ShowVersion <= 1)
      return Search(CmdL);
   return FullTextSearch(CmdL);
}


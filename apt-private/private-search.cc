// Includes								/*{{{*/
#include <config.h>

#include <apt-pkg/cachefile.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/progress.h>

#include <apt-private/private-cacheset.h>
#include <apt-private/private-search.h>
#include <apt-private/private-show.h>
#include <apt-private/private-package-info.h>

#include <sstream>
#include <vector>
#include <algorithm>

#define RABINKARPHASH(a, b, h, d) ((((h) - (a)*d) << 1) + (b))
									/*}}}*/

int RabinKarp(std::string StringInput, std::string Pattern) {
   std::transform(StringInput.begin(), StringInput.end(), StringInput.begin(), ::tolower);
   std::transform(Pattern.begin(), Pattern.end(), Pattern.begin(), ::tolower);
   int string_length = StringInput.length();
   int pattern_length = Pattern.length();

   int mask, hash_input=0, hash_pattern=0;

   /* Preprocessing */
   /* computes mask = 2^(string_length-1) with
      the left-shift operator */
   mask = (1<<(string_length-1));

   for (int i=0 ; i < string_length; ++i) {
      hash_input = ((hash_input<<1) + StringInput.c_str()[i]);
      hash_pattern = ((hash_pattern<<1) + Pattern.c_str()[i]);
   }

   /* Searching */
   for (int i =0; i <= pattern_length-string_length; ++i) {
      if (hash_input == hash_pattern && memcmp(StringInput.c_str(), Pattern.c_str() + i, string_length) == 0)
	return i;
      hash_pattern = RABINKARPHASH(Pattern.c_str()[i], Pattern.c_str()[i + string_length], hash_pattern, mask);
   }

   /* fould nothing*/
   return -1;
}

bool FullTextSearch(CommandLine &CmdL)					/*{{{*/
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
   if (_config->FindB("APT::Cache::UsingRegex",false))
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
   LocalitySortedVersionSet::iterator V = bag.begin();

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
   for ( ;V != bag.end(); ++V)
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
      if (_config->FindB("APT::Cache::UsingRegex",false))
      {
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
      }
      else
      {
	 for (unsigned int I = 0; I != NumPatterns; ++I)
	 {
	    if ((RabinKarp(CmdL.FileList[I + 1], P.Name()) >= 0) ) 
	    {
	       continue;
	    }
	    else if ( (NamesOnly == false) && (RabinKarp(CmdL.FileList[I + 1], LongDesc) >= 0))
	       continue;
	    all_found = false;
	    break;
	 }
      }
      if (all_found == true)
      {
	 PkgsDone[P->ID] = true;
	 std::stringstream outs;
	 ListSingleVersion(CacheFile, records, V, outs, format);
	 outputVector.emplace_back(CacheFile, records, V, outs.str());
      }
   }
   switch(PackageInfo::getOrderByOption())
   {
      case PackageInfo::REVERSEALPHABETIC:
	 std::sort(outputVector.rbegin(), outputVector.rend(), OrderByAlphabetic);
	 break;
      case PackageInfo::STATUS:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByStatus);
	 break;
      case PackageInfo::VERSION:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByVersion);
	 break;
      default:
	 std::sort(outputVector.begin(), outputVector.end(), OrderByAlphabetic);
	 break;
   }
   if (_config->FindB("APT::Cache::UsingRegex",false))
      APT_FREE_PATTERNS();
   progress.Done();

   // output the sorted vector
   for(auto k:outputVector)
      std::cout << k.formated_output() << std::endl;

   return true;
}
									/*}}}*/
// LocalitySort - Sort a version list by package file locality		/*{{{*/
static int LocalityCompare(const void * const a, const void * const b)
{
   pkgCache::VerFile const * const A = *(pkgCache::VerFile const * const * const)a;
   pkgCache::VerFile const * const B = *(pkgCache::VerFile const * const * const)b;

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
   memset(DFList,0,sizeof(*DFList) * descCount);

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
      pkgRecords::Parser &P = Recs.Lookup(pkgCache::DescFileIterator(*Cache,J->Df));
      size_t const PatternOffset = J->ID * NumPatterns;

      if (NamesOnly == false)
      {
	 std::string const LongDesc = P.LongDesc();
	 for (unsigned I = 0; I < NumPatterns; ++I)
	 {
	    if (PatternMatch[PatternOffset + I] == true)
	       continue;
	    else if (regexec(&Patterns[I],LongDesc.c_str(),0,0,0) == 0)
	       PatternMatch[PatternOffset + I] = true;
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
	    DisplayRecordV1(CacheFile, J->V, std::cout);
	 else
	    printf("%s - %s\n",P.Name().c_str(),P.ShortDesc().c_str());
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


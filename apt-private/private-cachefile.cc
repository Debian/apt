// Include files							/*{{{*/
#include<config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/upgrade.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <apt-private/private-output.h>
#include <apt-private/private-cachefile.h>

#include <string.h>
#include <ostream>
#include <cstdlib>

#include <apti18n.h>
									/*}}}*/

using namespace std;

// CacheFile::NameComp - QSort compare by name				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgCache *CacheFile::SortCache = 0;
int CacheFile::NameComp(const void *a,const void *b)
{
   if (*(pkgCache::Package **)a == 0 || *(pkgCache::Package **)b == 0)
      return *(pkgCache::Package **)a - *(pkgCache::Package **)b;
   
   const pkgCache::Package &A = **(pkgCache::Package **)a;
   const pkgCache::Package &B = **(pkgCache::Package **)b;

   return strcmp(SortCache->StrP + A.Name,SortCache->StrP + B.Name);
}
									/*}}}*/
// CacheFile::Sort - Sort by name					/*{{{*/
// ---------------------------------------------------------------------
/* */
void CacheFile::Sort()
{
   delete [] List;
   List = new pkgCache::Package *[Cache->Head().PackageCount];
   memset(List,0,sizeof(*List)*Cache->Head().PackageCount);
   pkgCache::PkgIterator I = Cache->PkgBegin();
   for (;I.end() != true; ++I)
      List[I->ID] = I;

   SortCache = *this;
   qsort(List,Cache->Head().PackageCount,sizeof(*List),NameComp);
}
									/*}}}*/
// CacheFile::CheckDeps - Open the cache file				/*{{{*/
// ---------------------------------------------------------------------
/* This routine generates the caches and then opens the dependency cache
   and verifies that the system is OK. */
bool CacheFile::CheckDeps(bool AllowBroken)
{
   bool FixBroken = _config->FindB("APT::Get::Fix-Broken",false);

   if (_error->PendingError() == true)
      return false;

   // Check that the system is OK
   if (DCache->DelCount() != 0 || DCache->InstCount() != 0)
      return _error->Error("Internal error, non-zero counts");
   
   // Apply corrections for half-installed packages
   if (pkgApplyStatus(*DCache) == false)
      return false;
   
   if (_config->FindB("APT::Get::Fix-Policy-Broken",false) == true)
   {
      FixBroken = true;
      if ((DCache->PolicyBrokenCount() > 0))
      {
	 // upgrade all policy-broken packages with ForceImportantDeps=True
	 for (pkgCache::PkgIterator I = Cache->PkgBegin(); !I.end(); ++I)
	    if ((*DCache)[I].NowPolicyBroken() == true) 
	       DCache->MarkInstall(I,true,0, false, true);
      }
   }

   // Nothing is broken
   if (DCache->BrokenCount() == 0 || AllowBroken == true)
      return true;

   // Attempt to fix broken things
   if (FixBroken == true)
   {
      c1out << _("Correcting dependencies...") << flush;
      if (pkgFixBroken(*DCache) == false || DCache->BrokenCount() != 0)
      {
	 c1out << _(" failed.") << endl;
	 ShowBroken(c1out,*this,true);

	 return _error->Error(_("Unable to correct dependencies"));
      }
      if (pkgMinimizeUpgrade(*DCache) == false)
	 return _error->Error(_("Unable to minimize the upgrade set"));
      
      c1out << _(" Done") << endl;
   }
   else
   {
      c1out << _("You might want to run 'apt-get -f install' to correct these.") << endl;
      ShowBroken(c1out,*this,true);

      return _error->Error(_("Unmet dependencies. Try using -f."));
   }
      
   return true;
}
									/*}}}*/

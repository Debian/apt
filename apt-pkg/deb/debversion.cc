// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Debian Version - Versioning system for Debian

   This implements the standard Debian versioning system.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/debversion.h>
#include <apt-pkg/pkgcache.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
									/*}}}*/

debVersioningSystem debVS;

// debVS::debVersioningSystem - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
debVersioningSystem::debVersioningSystem()
{
   Label = "Standard .deb";
}
									/*}}}*/

// debVS::CmpFragment - Compare versions			        /*{{{*/
// ---------------------------------------------------------------------
/* This compares a fragment of the version. This is a slightly adapted
   version of what dpkg uses in dpkg/lib/dpkg/version.c.
   In particular, the a | b = NULL check is removed as we check this in the
   caller, we use an explicit end for a | b strings and we check ~ explicit. */
static int order(char c)
{
   if (isdigit(c))
      return 0;
   else if (isalpha_ascii(c))
      return c;
   else if (c == '~')
      return -1;
   else if (c)
      return c + 256;
   else
      return 0;
}
int debVersioningSystem::CmpFragment(const char *A,const char *AEnd,
				     const char *B,const char *BEnd)
{
   /* Iterate over the whole string
      What this does is to split the whole string into groups of
      numeric and non numeric portions. For instance:
         a67bhgs89
      Has 4 portions 'a', '67', 'bhgs', '89'. A more normal:
         2.7.2-linux-1
      Has '2', '.', '7', '.' ,'-linux-','1' */
   const char *lhs = A;
   const char *rhs = B;
   while (lhs != AEnd && rhs != BEnd)
   {
      int first_diff = 0;

      while (lhs != AEnd && rhs != BEnd &&
	     (!isdigit(*lhs) || !isdigit(*rhs)))
      {
	 int vc = order(*lhs);
	 int rc = order(*rhs);
	 if (vc != rc)
	    return vc - rc;
	 ++lhs; ++rhs;
      }

      while (*lhs == '0')
	 ++lhs;
      while (*rhs == '0')
	 ++rhs;
      while (isdigit(*lhs) && isdigit(*rhs))
      {
	 if (!first_diff)
	    first_diff = *lhs - *rhs;
	 ++lhs;
	 ++rhs;
      }

      if (isdigit(*lhs))
	 return 1;
      if (isdigit(*rhs))
	 return -1;
      if (first_diff)
	 return first_diff;
   }

   // The strings must be equal
   if (lhs == AEnd && rhs == BEnd)
      return 0;

   // lhs is shorter
   if (lhs == AEnd)
   {
      if (*rhs == '~') return 1;
      return -1;
   }

   // rhs is shorter
   if (rhs == BEnd)
   {
      if (*lhs == '~') return -1;
      return 1;
   }

   // Shouldn't happen
   return 1;
}
									/*}}}*/
// debVS::CmpVersion - Comparison for versions				/*{{{*/
// ---------------------------------------------------------------------
/* This fragments the version into E:V-R triples and compares each 
   portion separately. */
int debVersioningSystem::DoCmpVersion(const char *A,const char *AEnd,
				      const char *B,const char *BEnd)
{
   // Strip off the epoch and compare it
   const char *lhs = (const char*) memchr(A, ':', AEnd - A);
   const char *rhs = (const char*) memchr(B, ':', BEnd - B);
   if (lhs == NULL)
      lhs = A;
   if (rhs == NULL)
      rhs = B;
   
   // Special case: a zero epoch is the same as no epoch,
   // so remove it.
   if (lhs != A)
   {
      for (; *A == '0'; ++A);
      if (A == lhs)
      {
	 ++A;
	 ++lhs;
      }
   }
   if (rhs != B)
   {
      for (; *B == '0'; ++B);
      if (B == rhs)
      {
	 ++B;
	 ++rhs;
      }
   }

   // Compare the epoch
   int Res = CmpFragment(A,lhs,B,rhs);
   if (Res != 0)
      return Res;

   // Skip the :
   if (lhs != A)
      lhs++;
   if (rhs != B)
      rhs++;
   
   // Find the last -
   const char *dlhs = (const char*) memrchr(lhs, '-', AEnd - lhs);
   const char *drhs = (const char*) memrchr(rhs, '-', BEnd - rhs);
   if (dlhs == NULL)
      dlhs = AEnd;
   if (drhs == NULL)
      drhs = BEnd;
   
   // Compare the main version
   Res = CmpFragment(lhs,dlhs,rhs,drhs);
   if (Res != 0)
      return Res;
   
   // Skip the -
   if (dlhs != lhs)
      dlhs++;
   if (drhs != rhs)
      drhs++;

   // no debian revision need to be treated like -0
   if (*(dlhs-1) == '-' && *(drhs-1) == '-')
      return CmpFragment(dlhs,AEnd,drhs,BEnd);
   else if (*(dlhs-1) == '-')
   {
      const char* null = "0";
      return CmpFragment(dlhs,AEnd,null, null+1);
   }
   else if (*(drhs-1) == '-')
   {
      const char* null = "0";
      return CmpFragment(null, null+1, drhs, BEnd);
   }
   else
      return 0;
}
									/*}}}*/
// debVS::CheckDep - Check a single dependency				/*{{{*/
// ---------------------------------------------------------------------
/* This simply performs the version comparison and switch based on 
   operator. If DepVer is 0 then we are comparing against a provides
   with no version. */
bool debVersioningSystem::CheckDep(const char *PkgVer,
				   int Op,const char *DepVer)
{
   if (DepVer == 0 || DepVer[0] == 0)
      return true;
   if (PkgVer == 0 || PkgVer[0] == 0)
      return false;
   Op &= 0x0F;

   // fast track for (equal) strings [by location] which are by definition equal versions
   if (PkgVer == DepVer)
      return Op == pkgCache::Dep::Equals || Op == pkgCache::Dep::LessEq || Op == pkgCache::Dep::GreaterEq;

   // Perform the actual comparison.
   int const Res = CmpVersion(PkgVer, DepVer);
   switch (Op)
   {
      case pkgCache::Dep::LessEq:
      if (Res <= 0)
	 return true;
      break;
      
      case pkgCache::Dep::GreaterEq:
      if (Res >= 0)
	 return true;
      break;
      
      case pkgCache::Dep::Less:
      if (Res < 0)
	 return true;
      break;
      
      case pkgCache::Dep::Greater:
      if (Res > 0)
	 return true;
      break;
      
      case pkgCache::Dep::Equals:
      if (Res == 0)
	 return true;
      break;
      
      case pkgCache::Dep::NotEquals:
      if (Res != 0)
	 return true;
      break;
   }

   return false;
}
									/*}}}*/
// debVS::UpstreamVersion - Return the upstream version string		/*{{{*/
// ---------------------------------------------------------------------
/* This strips all the debian specific information from the version number */
std::string debVersioningSystem::UpstreamVersion(const char *Ver)
{
   // Strip off the bit before the first colon
   const char *I = Ver;
   for (; *I != 0 && *I != ':'; I++);
   if (*I == ':')
      Ver = I + 1;
   
   // Chop off the trailing -
   I = Ver;
   unsigned Last = strlen(Ver);
   for (; *I != 0; I++)
      if (*I == '-')
	 Last = I - Ver;
   
   return std::string(Ver,Last);
}
									/*}}}*/

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debversion.cc,v 1.8 2003/09/10 23:39:49 mdz Exp $
/* ######################################################################

   Debian Version - Versioning system for Debian

   This implements the standard Debian versioning system.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#define APT_COMPATIBILITY 986

#include <apt-pkg/debversion.h>
#include <apt-pkg/pkgcache.h>

#include <stdlib.h>
#include <ctype.h>
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
   version of what dpkg uses. */
#define order(x) ((x) == '~' ? -1    \
		: isdigit((x)) ? 0   \
		: !(x) ? 0           \
		: isalpha((x)) ? (x) \
		: (x) + 256)
int debVersioningSystem::CmpFragment(const char *A,const char *AEnd,
				     const char *B,const char *BEnd)
{
   if (A >= AEnd && B >= BEnd)
      return 0;
   if (A >= AEnd)
   {
      if (*B == '~') return 1;
      return -1;
   }
   if (B >= BEnd)
   {
      if (*A == '~') return -1;
      return 1;
   }

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
	 lhs++; rhs++;
      }

      while (*lhs == '0')
	 lhs++;
      while (*rhs == '0')
	 rhs++;
      while (isdigit(*lhs) && isdigit(*rhs))
      {
	 if (!first_diff)
	    first_diff = *lhs - *rhs;
	 lhs++;
	 rhs++;
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

   // Shouldnt happen
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
   const char *lhs = A;
   const char *rhs = B;
   for (;lhs != AEnd && *lhs != ':'; lhs++);
   for (;rhs != BEnd && *rhs != ':'; rhs++);
   if (lhs == AEnd)
      lhs = A;
   if (rhs == BEnd)
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
   const char *dlhs = AEnd-1;
   const char *drhs = BEnd-1;
   for (;dlhs > lhs && *dlhs != '-'; dlhs--);
   for (;drhs > rhs && *drhs != '-'; drhs--);

   if (dlhs == lhs)
      dlhs = AEnd;
   if (drhs == rhs)
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
   
   return CmpFragment(dlhs,AEnd,drhs,BEnd);
}
									/*}}}*/
// debVS::CheckDep - Check a single dependency				/*{{{*/
// ---------------------------------------------------------------------
/* This simply preforms the version comparison and switch based on 
   operator. If DepVer is 0 then we are comparing against a provides
   with no version. */
bool debVersioningSystem::CheckDep(const char *PkgVer,
				   int Op,const char *DepVer)
{
   if (DepVer == 0 || DepVer[0] == 0)
      return true;
   if (PkgVer == 0 || PkgVer[0] == 0)
      return false;
   
   // Perform the actual comparision.
   int Res = CmpVersion(PkgVer,DepVer);
   switch (Op & 0x0F)
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
string debVersioningSystem::UpstreamVersion(const char *Ver)
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
   
   return string(Ver,Last);
}
									/*}}}*/

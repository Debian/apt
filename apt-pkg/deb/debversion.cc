// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: debversion.cc,v 1.2 2001/02/20 07:03:17 jgg Exp $
/* ######################################################################

   Debian Version - Versioning system for Debian

   This implements the standard Debian versioning system.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#define APT_COMPATIBILITY 986
#ifdef __GNUG__
#pragma implementation "apt-pkg/debversion.h"
#endif

#include <apt-pkg/debversion.h>
#include <apt-pkg/pkgcache.h>

#include <stdlib.h>
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
// StrToLong - Convert the string between two iterators to a long	/*{{{*/
// ---------------------------------------------------------------------
/* */
static unsigned long StrToLong(const char *begin,const char *end)
{
   char S[40];
   char *I = S;
   for (; begin != end && I < S + 40;)
      *I++ = *begin++;
   *I = 0;
   return strtoul(S,0,10);
}
									/*}}}*/
// debVS::CmpFragment - Compare versions			        /*{{{*/
// ---------------------------------------------------------------------
/* This compares a fragment of the version. Dpkg has a really short 
   version of this, but it is uh.. interesting to grok. */
int debVersioningSystem::CmpFragment(const char *A,const char *AEnd, 
				     const char *B,const char *BEnd)
{
   if (A >= AEnd && B >= BEnd)
      return 0;
   if (A >= AEnd)
      return -1;
   if (B >= BEnd)
      return 1;
   
   /* Iterate over the whole string
      What this does is to spilt the whole string into groups of 
      numeric and non numeric portions. For instance:
         a67bhgs89
      Has 4 portions 'a', '67', 'bhgs', '89'. A more normal:
         2.7.2-linux-1
      Has '2', '.', '7', '.' ,'-linux-','1' */
   const char *lhs = A;
   const char *rhs = B;
   while (lhs != AEnd && rhs != BEnd)
   {
      // Starting points
      const char *Slhs = lhs;
      const char *Srhs = rhs;
      
      // Compute ending points were we have passed over the portion
      bool Digit = (isdigit(*lhs) > 0?true:false);
      for (;lhs != AEnd && (isdigit(*lhs) > 0?true:false) == Digit; lhs++);
      for (;rhs != BEnd && (isdigit(*rhs) > 0?true:false) == Digit; rhs++);
      
      if (Digit == true)
      {
	 // If the lhs has a digit and the rhs does not then <
	 if (rhs - Srhs == 0)
	    return -1;
	 
	 // Generate integers from the strings.
	 unsigned long Ilhs = StrToLong(Slhs,lhs);
	 unsigned long Irhs = StrToLong(Srhs,rhs);
	 if (Ilhs != Irhs)
	 {
	    if (Ilhs > Irhs)
	       return 1;
	    return -1;
	 }
      }
      else
      {
	 // They are equal length so do a straight text compare
	 for (;Slhs != lhs && Srhs != rhs; Slhs++, Srhs++)
	 {
	    if (*Slhs != *Srhs)
	    {
	       /* We need to compare non alpha chars as higher than alpha
	          chars (a < !) */
	       int lc = *Slhs;
	       int rc = *Srhs;
	       if (isalpha(lc) == 0) lc += 256;
	       if (isalpha(rc) == 0) rc += 256;
	       if (lc > rc)
		  return 1;
	       return -1;
	    }
	 }

	 // If the lhs is shorter than the right it is 'less'
	 if (lhs - Slhs < rhs - Srhs)
	    return -1;

	 // If the lhs is longer than the right it is 'more'
	 if (lhs - Slhs > rhs - Srhs)
	    return 1;		 
      }      
   }

   // The strings must be equal
   if (lhs == AEnd && rhs == BEnd)
      return 0;

   // lhs is shorter
   if (lhs == AEnd)
      return -1;

   // rhs is shorter
   if (rhs == BEnd)
      return 1;
       
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

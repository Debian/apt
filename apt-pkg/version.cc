// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: version.cc,v 1.4 1998/07/12 23:58:42 jgg Exp $
/* ######################################################################

   Version - Version string 
   
   Version comparing is done using the == and < operators. STL's
   function.h provides the remaining set of comparitors. A directly
   callable non-string class version is provided for functions manipulating
   the cache file (esp the sort function).
 
   A version is defined to be equal if a case sensitive compare returns
   that the two strings are the same. For compatibility with the QSort
   function this version returns -1,0,1.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#ifdef __GNUG__
#pragma implementation "apt-pkg/version.h"
#endif 

#include <apt-pkg/version.h>
#include <apt-pkg/pkgcache.h>

#include <stdlib.h>
									/*}}}*/

// Version::pkgVersion - Default Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgVersion::pkgVersion()
{
}
									/*}}}*/
// Version::operator == - Checks if two versions are equal		/*{{{*/
// ---------------------------------------------------------------------
/* We can't simply perform a string compare because of epochs. */
bool pkgVersion::operator ==(const pkgVersion &Vrhs) const
{
   if (pkgVersionCompare(Value.begin(),Value.end(),
		     Vrhs.Value.begin(),Vrhs.Value.end()) == 0)
      return true;
   return false;
}
									/*}}}*/
// Version::operator < - Checks if this is less than another version	/*{{{*/
// ---------------------------------------------------------------------
/* All other forms of comparision can be built up from this single function.
    a > b -> b < a
    a <= b -> !(a > b) -> !(b < a)
    a >= b -> !(a < b) 
 */
bool pkgVersion::operator <(const pkgVersion &Vrhs) const
{
   if (pkgVersionCompare(Value.begin(),Value.end(),
		     Vrhs.Value.begin(),Vrhs.Value.end()) == -1)
      return true;
   return false;
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
// VersionCompare (op) - Greater than comparison for versions		/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgVersionCompare(const char *A, const char *B)
{
   return pkgVersionCompare(A,A + strlen(A),B,B + strlen(B));
}
int pkgVersionCompare(string A,string B)
{
   return pkgVersionCompare(A.begin(),A.end(),B.begin(),B.end());
}

									/*}}}*/
// VersionCompare - Greater than comparison for versions		/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgVersionCompare(const char *A, const char *AEnd, const char *B,
		       const char *BEnd)
{
   // lhs = left hand side, rhs = right hand side
   const char *lhs = A;
   const char *rhs = B;

   /* Consider epochs. They need special handling because an epoch 
      must not be compared against the first element of the real version.
      This works okay when both sides have an epoch but when only one
      does it must compare the missing epoch to 0 */
   for (;lhs != AEnd && *lhs != ':'; lhs++);
   for (;rhs != BEnd && *rhs != ':'; rhs++);

   // Parse the epoch out
   unsigned long lhsEpoch = 0;
   unsigned long rhsEpoch = 0;
   if (lhs != AEnd && *lhs == ':')
      lhsEpoch = StrToLong(A,lhs);
   if (rhs != BEnd && *rhs == ':')
      rhsEpoch = StrToLong(B,rhs);
   if (lhsEpoch != rhsEpoch)
   {
      if (lhsEpoch > rhsEpoch)
	 return 1;
      return -1;
   }
   
   /* Iterate over the whole string
      What this does is to spilt the whole string into groups of 
      numeric and non numeric portions. For instance:
         a67bhgs89
      Has 4 portions 'a', '67', 'bhgs', '89'. A more normal:
         2.7.2-linux-1
      Has '2', '.', '7', '.' ,'-linux-','1' */
   lhs = A;
   rhs = B;
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
	 // If the lhs has a digit and the rhs does not then true
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
	          chars (a < !) This is so things like  7.6p2-4 and 7.6-0 
		  compare higher as well as . and -. I am not sure how
		  the dpkg code manages to achive the != '-' test, but it
		  is necessary. */
	       int lc = *Slhs;
	       int rc = *Srhs;
	       if (isalpha(lc) == 0 && lc != '-') lc += 256;
	       if (isalpha(rc) == 0 && rc != '-') rc += 256;
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
// CheckDep - Check a single dependency					/*{{{*/
// ---------------------------------------------------------------------
/* This simply preforms the version comparison and switch based on 
   operator. */
bool pkgCheckDep(const char *DepVer,const char *PkgVer,int Op)
{
   if (DepVer == 0)
      return true;
   if (PkgVer == 0)
      return false;
   
   // Perform the actuall comparision.
   int Res = pkgVersionCompare(PkgVer,DepVer);
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


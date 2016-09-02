// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sha1.h,v 1.3 2001/05/07 05:05:47 jgg Exp $
/* ######################################################################

   SHA1SumValue - Storage for a SHA-1 hash.
   SHA1Summation - SHA-1 Secure Hash Algorithm.
   
   This is a C++ interface to a set of SHA1Sum functions, that mirrors
   the equivalent MD5 classes. 

   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_SHA1_H
#define APTPKG_SHA1_H

#include "hashsum_template.h"

#ifndef APT_10_CLEANER_HEADERS
#include <string>
#include <cstring>
#include <algorithm>
#endif
#ifndef APT_8_CLEANER_HEADERS
using std::string;
using std::min;
#endif

typedef  HashSumValue<160> SHA1SumValue;

class SHA1Summation : public SummationImplementation
{
   /* assumes 64-bit alignment just in case */
   unsigned char Buffer[64] __attribute__((aligned(8)));
   unsigned char State[5*4] __attribute__((aligned(8)));
   unsigned char Count[2*4] __attribute__((aligned(8)));
   bool Done;
   
   public:
   bool Add(const unsigned char *inbuf, unsigned long long inlen) APT_OVERRIDE APT_NONNULL(2);
   using SummationImplementation::Add;

   SHA1SumValue Result();
   
   SHA1Summation();
};

#endif

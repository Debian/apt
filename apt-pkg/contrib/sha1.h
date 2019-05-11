// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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

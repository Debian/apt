// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
/* ######################################################################

   SHA{512,256}SumValue - Storage for a SHA-{512,256} hash.
   SHA{512,256}Summation - SHA-{512,256} Secure Hash Algorithm.
   
   This is a C++ interface to a set of SHA{512,256}Sum functions, that mirrors
   the equivalent MD5 & SHA1 classes. 

   ##################################################################### */
                                                                        /*}}}*/
#ifndef APTPKG_SHA2_H
#define APTPKG_SHA2_H

#include <cstring>

#include "hashsum_template.h"
#include "sha2_internal.h"



typedef HashSumValue<512> SHA512SumValue;
typedef HashSumValue<256> SHA256SumValue;

class SHA2SummationBase : public SummationImplementation
{
 protected:
   bool Done;
 public:
   bool Add(const unsigned char *inbuf, unsigned long long len) APT_OVERRIDE APT_NONNULL(2) = 0;

   void Result();
};

class SHA256Summation : public SHA2SummationBase
{
   SHA256_CTX ctx;
   unsigned char Sum[32];

   public:
   bool Add(const unsigned char *inbuf, unsigned long long len) APT_OVERRIDE APT_NONNULL(2)
   {
      if (Done) 
         return false;
      SHA256_Update(&ctx, inbuf, len);
      return true;
   };
   using SummationImplementation::Add;

   SHA256SumValue Result()
   {
      if (!Done) {
         SHA256_Final(Sum, &ctx);
         Done = true;
      }
      SHA256SumValue res;
      res.Set(Sum);
      return res;
   };
   SHA256Summation()
   {
      SHA256_Init(&ctx);
      Done = false;
      memset(&Sum, 0, sizeof(Sum));
   };
};

class SHA512Summation : public SHA2SummationBase
{
   SHA512_CTX ctx;
   unsigned char Sum[64];

   public:
   bool Add(const unsigned char *inbuf, unsigned long long len) APT_OVERRIDE APT_NONNULL(2)
   {
      if (Done) 
         return false;
      SHA512_Update(&ctx, inbuf, len);
      return true;
   };
   using SummationImplementation::Add;

   SHA512SumValue Result()
   {
      if (!Done) {
         SHA512_Final(Sum, &ctx);
         Done = true;
      }
      SHA512SumValue res;
      res.Set(Sum);
      return res;
   };
   SHA512Summation()
   {
      SHA512_Init(&ctx);
      Done = false;
      memset(&Sum, 0, sizeof(Sum));
   };
};


#endif

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   MD5SumValue - Storage for a MD5Sum
   MD5Summation - MD5 Message Digest Algorithm.
   
   This is a C++ interface to a set of MD5Sum functions. The class can
   store a MD5Sum in 16 bytes of memory.
   
   A MD5Sum is used to generate a (hopefully) unique 16 byte number for a
   block of data. This can be used to guard against corruption of a file.
   MD5 should not be used for tamper protection, use SHA or something more
   secure.
   
   There are two classes because computing a MD5 is not a continual 
   operation unless 64 byte blocks are used. Also the summation requires an
   extra 18*4 bytes to operate.
   
   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_MD5_H
#define APTPKG_MD5_H

#include <stdint.h>

#include "hashsum_template.h"


typedef HashSumValue<128> MD5SumValue;

class MD5Summation : public SummationImplementation
{
   uint32_t Buf[4];
   unsigned char Bytes[2*4];
   unsigned char In[16*4];
   bool Done;

   public:

   bool Add(const unsigned char *inbuf, unsigned long long inlen) APT_OVERRIDE APT_NONNULL(2);
   using SummationImplementation::Add;

   MD5SumValue Result();

   MD5Summation();
};

#endif

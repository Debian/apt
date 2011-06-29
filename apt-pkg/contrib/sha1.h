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

#include <string>
#include <cstring>
#include <algorithm>

using std::string;
using std::min;

#include "hashsum_template.h"

class SHA1Summation;

typedef  HashSumValue<160> SHA1SumValue;

class SHA1Summation
{
   /* assumes 64-bit alignment just in case */
   unsigned char Buffer[64] __attribute__((aligned(8)));
   unsigned char State[5*4] __attribute__((aligned(8)));
   unsigned char Count[2*4] __attribute__((aligned(8)));
   bool Done;
   
   public:

   bool Add(const unsigned char *inbuf,unsigned long inlen);
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
   SHA1SumValue Result();
   
   SHA1Summation();
};

#endif

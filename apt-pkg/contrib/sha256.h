// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
// $Id: sha1.h,v 1.3 2001/05/07 05:05:47 jgg Exp $
/* ######################################################################

   SHA256SumValue - Storage for a SHA-256 hash.
   SHA256Summation - SHA-256 Secure Hash Algorithm.
   
   This is a C++ interface to a set of SHA256Sum functions, that mirrors
   the equivalent MD5 & SHA1 classes. 

   ##################################################################### */
                                                                        /*}}}*/
#ifndef APTPKG_SHA256_H
#define APTPKG_SHA256_H

#include <string>
#include <cstring>
#include <algorithm>
#include <stdint.h>

using std::string;
using std::min;

class SHA256Summation;

class SHA256SumValue
{
   friend class SHA256Summation;
   unsigned char Sum[32];
   
   public:

   // Accessors
   bool operator ==(const SHA256SumValue &rhs) const; 
   string Value() const;
   inline void Value(unsigned char S[32])
         {for (int I = 0; I != sizeof(Sum); I++) S[I] = Sum[I];};
   inline operator string() const {return Value();};
   bool Set(string Str);
   inline void Set(unsigned char S[32]) 
         {for (int I = 0; I != sizeof(Sum); I++) Sum[I] = S[I];};

   SHA256SumValue(string Str);
   SHA256SumValue();
};

struct sha256_ctx {
    uint32_t count[2];
    uint32_t state[8];
    uint8_t buf[128];
};

class SHA256Summation
{
   struct sha256_ctx Sum;

   bool Done;

   public:

   bool Add(const unsigned char *inbuf,unsigned long inlen);
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
   SHA256SumValue Result();
   
   SHA256Summation();
};

#endif

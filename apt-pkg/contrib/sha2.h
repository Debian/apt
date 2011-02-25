// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
// $Id: sha512.h,v 1.3 2001/05/07 05:05:47 jgg Exp $
/* ######################################################################

   SHA{512,256}SumValue - Storage for a SHA-{512,256} hash.
   SHA{512,256}Summation - SHA-{512,256} Secure Hash Algorithm.
   
   This is a C++ interface to a set of SHA{512,256}Sum functions, that mirrors
   the equivalent MD5 & SHA1 classes. 

   ##################################################################### */
                                                                        /*}}}*/
#ifndef APTPKG_SHA2_H
#define APTPKG_SHA2_H

#include <string>
#include <cstring>
#include <algorithm>
#include <stdint.h>

#include "sha2_internal.h"

using std::string;
using std::min;

// SHA512
class SHA512Summation;

class SHA512SumValue
{
   friend class SHA512Summation;
   unsigned char Sum[64];
   
   public:

   // Accessors
   bool operator ==(const SHA512SumValue &rhs) const; 
   string Value() const;
   inline void Value(unsigned char S[64])
         {for (int I = 0; I != sizeof(Sum); I++) S[I] = Sum[I];};
   inline operator string() const {return Value();};
   bool Set(string Str);
   inline void Set(unsigned char S[64]) 
         {for (int I = 0; I != sizeof(Sum); I++) Sum[I] = S[I];};

   SHA512SumValue(string Str);
   SHA512SumValue();
};

class SHA512Summation
{
   SHA512_CTX ctx;
   unsigned char Sum[64];
   bool Done;

   public:

   bool Add(const unsigned char *inbuf,unsigned long inlen);
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));};
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End) 
                  {return Add(Beg,End-Beg);};
   SHA512SumValue Result();
   
   SHA512Summation();
};

// SHA256
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

class SHA256Summation
{
   SHA256_CTX ctx;
   unsigned char Sum[32];
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

// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: sha1.h,v 1.1 2001/03/06 05:03:49 jgg Exp $
/* ######################################################################

   SHA1SumValue - Storage for a SHA-1 hash.
   SHA1Summation - SHA-1 Secure Hash Algorithm.
   
   This is a C++ interface to a set of SHA1Sum functions, that mirrors
   the equivalent MD5 classes. 

   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_SHA1_H
#define APTPKG_SHA1_H

#ifdef __GNUG__
#pragma interface "apt-pkg/sha1.h"
#endif 

#include <string>

class SHA1Summation;

class SHA1SumValue
{
   friend class SHA1Summation;
   unsigned char Sum[20];
   
   public:

   // Accessors
   bool operator ==(const SHA1SumValue &rhs) const; 
   string Value() const;
   inline void Value(unsigned char S[20])
         {for (int I = 0; I != sizeof(Sum); I++) S[I] = Sum[I];};
   inline operator string() const {return Value();};
   bool Set(string Str);
   inline void Set(unsigned char S[20]) 
         {for (int I = 0; I != sizeof(Sum); I++) Sum[I] = S[I];};

   SHA1SumValue(string Str);
   SHA1SumValue();
};

class SHA1Summation
{
   unsigned char Buffer[64];
   unsigned char State[5*4];
   unsigned char Count[2*4];
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

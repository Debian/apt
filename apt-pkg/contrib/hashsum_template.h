// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
// $Id: hashsum_template.h,v 1.3 2001/05/07 05:05:47 jgg Exp $
/* ######################################################################

   HashSumValueTemplate - Generic Storage for a hash value
   
   ##################################################################### */
                                                                        /*}}}*/
#ifndef APTPKG_HASHSUM_TEMPLATE_H
#define APTPKG_HASHSUM_TEMPLATE_H

#include <apt-pkg/fileutl.h>

#include <string>
#include <cstring>
#include <algorithm>
#include <stdint.h>

#include <apt-pkg/strutl.h>

#ifndef APT_8_CLEANER_HEADERS
using std::string;
using std::min;
#endif

template<int N>
class HashSumValue
{
   unsigned char Sum[N/8];
   
   public:

   // Accessors
   bool operator ==(const HashSumValue &rhs) const
   {
      return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
   };
   bool operator !=(const HashSumValue &rhs) const
   {
      return memcmp(Sum,rhs.Sum,sizeof(Sum)) != 0;
   };

   std::string Value() const
   {
      char Conv[16] =
      { '0','1','2','3','4','5','6','7','8','9','a','b',
        'c','d','e','f'
      };
      char Result[((N/8)*2)+1];
      Result[(N/8)*2] = 0;
      
      // Convert each char into two letters
      int J = 0;
      int I = 0;
      for (; I != (N/8)*2; J++,I += 2)
      {
         Result[I] = Conv[Sum[J] >> 4];
         Result[I + 1] = Conv[Sum[J] & 0xF];
      }
      return std::string(Result);
   };
   
   inline void Value(unsigned char S[N/8])
   {
      for (int I = 0; I != sizeof(Sum); I++) 
         S[I] = Sum[I];
   };

   inline operator std::string() const 
   {
      return Value();
   };

   bool Set(std::string Str) 
   {
      return Hex2Num(Str,Sum,sizeof(Sum));
   };

   inline void Set(unsigned char S[N/8]) 
   {
      for (int I = 0; I != sizeof(Sum); I++) 
         Sum[I] = S[I];
   };

   HashSumValue(std::string Str) 
   {
         memset(Sum,0,sizeof(Sum));
         Set(Str);
   }
   HashSumValue()
   {
      memset(Sum,0,sizeof(Sum));
   }
};

class SummationImplementation
{
   public:
   virtual bool Add(const unsigned char *inbuf, unsigned long long inlen) = 0;
   inline bool Add(const char *inbuf, unsigned long long const inlen)
   { return Add((unsigned char *)inbuf, inlen); };

   inline bool Add(const unsigned char *Data)
   { return Add(Data, strlen((const char *)Data)); };
   inline bool Add(const char *Data)
   { return Add((const unsigned char *)Data, strlen((const char *)Data)); };

   inline bool Add(const unsigned char *Beg, const unsigned char *End)
   { return Add(Beg, End - Beg); };
   inline bool Add(const char *Beg, const char *End)
   { return Add((const unsigned char *)Beg, End - Beg); };

   bool AddFD(int Fd, unsigned long long Size = 0);
   bool AddFD(FileFd &Fd, unsigned long long Size = 0);
};

#endif

// -*- mode: cpp; mode: fold -*-
// Description                                                          /*{{{*/
/* ######################################################################

   HashSumValueTemplate - Generic Storage for a hash value
   
   ##################################################################### */
                                                                        /*}}}*/
#ifndef APTPKG_HASHSUM_TEMPLATE_H
#define APTPKG_HASHSUM_TEMPLATE_H

#include <cstring>
#include <string>
#include <apt-pkg/string_view.h>

#include <apt-pkg/strutl.h>


class FileFd;

template<int N>
class HashSumValue
{
   unsigned char Sum[N/8];

   public:

   // Accessors
   bool operator ==(const HashSumValue &rhs) const
   {
      return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
   }
   bool operator !=(const HashSumValue &rhs) const
   {
      return memcmp(Sum,rhs.Sum,sizeof(Sum)) != 0;
   }

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
   }

   inline void Value(unsigned char S[N/8])
   {
      for (int I = 0; I != sizeof(Sum); ++I)
         S[I] = Sum[I];
   }

   inline operator std::string() const
   {
      return Value();
   }

   bool Set(APT::StringView Str)
   {
      return Hex2Num(Str,Sum,sizeof(Sum));
   }
   inline void Set(unsigned char S[N/8])
   {
      for (int I = 0; I != sizeof(Sum); ++I)
         Sum[I] = S[I];
   }

   explicit HashSumValue(std::string const &Str)
   {
         memset(Sum,0,sizeof(Sum));
         Set(Str);
   }
   explicit HashSumValue(APT::StringView const &Str)
   {
         memset(Sum,0,sizeof(Sum));
         Set(Str);
   }
   APT_HIDDEN explicit HashSumValue(const char *Str)
   {
         memset(Sum,0,sizeof(Sum));
         Set(Str);
   }
   HashSumValue()
   {
      memset(Sum,0,sizeof(Sum));
   }
};

#endif

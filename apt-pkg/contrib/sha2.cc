/*
 * Cryptographic API.							{{{
 *
 * SHA-512, as specified in
 * http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */									/*}}}*/

#ifdef __GNUG__
#pragma implementation "apt-pkg/2.h"
#endif

#include <apt-pkg/sha2.h>
#include <apt-pkg/strutl.h>

SHA512Summation::SHA512Summation()					/*{{{*/
{
   SHA512_Init(&ctx);
   Done = false;
}
									/*}}}*/
bool SHA512Summation::Add(const unsigned char *inbuf,unsigned long len) /*{{{*/
{
   if (Done) 
      return false;
   SHA512_Update(&ctx, inbuf, len);
   return true;
}
									/*}}}*/
SHA512SumValue SHA512Summation::Result()				/*{{{*/
{
   if (!Done) {
      SHA512_Final(Sum, &ctx);
      Done = true;
   }

   SHA512SumValue res;
   res.Set(Sum);
   return res;
}
									/*}}}*/
// SHA512SumValue::SHA512SumValue - Constructs the sum from a string   /*{{{*/
// ---------------------------------------------------------------------
/* The string form of a SHA512 is a 64 character hex number */
SHA512SumValue::SHA512SumValue(string Str)
{
   memset(Sum,0,sizeof(Sum));
   Set(Str);
}
                                                                       /*}}}*/
// SHA512SumValue::SHA512SumValue - Default constructor                /*{{{*/
// ---------------------------------------------------------------------
/* Sets the value to 0 */
SHA512SumValue::SHA512SumValue()
{
   memset(Sum,0,sizeof(Sum));
}
                                                                       /*}}}*/
// SHA512SumValue::Set - Set the sum from a string                     /*{{{*/
// ---------------------------------------------------------------------
/* Converts the hex string into a set of chars */
bool SHA512SumValue::Set(string Str)
{
   return Hex2Num(Str,Sum,sizeof(Sum));
}
                                                                       /*}}}*/
// SHA512SumValue::Value - Convert the number into a string            /*{{{*/
// ---------------------------------------------------------------------
/* Converts the set of chars into a hex string in lower case */
string SHA512SumValue::Value() const
{
   char Conv[16] =
      { '0','1','2','3','4','5','6','7','8','9','a','b',
      'c','d','e','f'
   };
   char Result[129];
   Result[128] = 0;

   // Convert each char into two letters
   int J = 0;
   int I = 0;
   for (; I != 128; J++,I += 2)
   {
      Result[I] = Conv[Sum[J] >> 4];
      Result[I + 1] = Conv[Sum[J] & 0xF];
   }

   return string(Result);
}
									/*}}}*/
// SHA512SumValue::operator == - Comparator                            /*{{{*/
// ---------------------------------------------------------------------
/* Call memcmp on the buffer */
bool SHA512SumValue::operator == (const SHA512SumValue & rhs) const
{
   return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
}
                                                                       /*}}}*/
// SHA512Summation::AddFD - Add content of file into the checksum      /*{{{*/
// ---------------------------------------------------------------------
/* */
bool SHA512Summation::AddFD(int Fd,unsigned long Size)
{
   unsigned char Buf[64 * 64];
   int Res = 0;
   int ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned n = sizeof(Buf);
      if (!ToEOF) n = min(Size,(unsigned long)n);
      Res = read(Fd,Buf,n);
      if (Res < 0 || (!ToEOF && (unsigned) Res != n)) // error, or short read
         return false;
      if (ToEOF && Res == 0) // EOF
         break;
      Size -= Res;
      Add(Buf,Res);
   }
   return true;
}
                                                                       /*}}}*/

SHA256Summation::SHA256Summation()					/*{{{*/
{
   SHA256_Init(&ctx);
   Done = false;
}
									/*}}}*/
bool SHA256Summation::Add(const unsigned char *inbuf,unsigned long len) /*{{{*/
{
   if (Done) 
      return false;
   SHA256_Update(&ctx, inbuf, len);
   return true;
}
									/*}}}*/
SHA256SumValue SHA256Summation::Result()				/*{{{*/
{
   if (!Done) {
      SHA256_Final(Sum, &ctx);
      Done = true;
   }

   SHA256SumValue res;
   res.Set(Sum);
   return res;
}
									/*}}}*/
// SHA256SumValue::SHA256SumValue - Constructs the sum from a string   /*{{{*/
// ---------------------------------------------------------------------
/* The string form of a SHA512 is a 64 character hex number */
SHA256SumValue::SHA256SumValue(string Str)
{
   memset(Sum,0,sizeof(Sum));
   Set(Str);
}
                                                                       /*}}}*/
// SHA256SumValue::SHA256SumValue - Default constructor                /*{{{*/
// ---------------------------------------------------------------------
/* Sets the value to 0 */
SHA256SumValue::SHA256SumValue()
{
   memset(Sum,0,sizeof(Sum));
}
                                                                       /*}}}*/
// SHA256SumValue::Set - Set the sum from a string                     /*{{{*/
// ---------------------------------------------------------------------
/* Converts the hex string into a set of chars */
bool SHA256SumValue::Set(string Str)
{
   return Hex2Num(Str,Sum,sizeof(Sum));
}
                                                                       /*}}}*/
// SHA256SumValue::Value - Convert the number into a string            /*{{{*/
// ---------------------------------------------------------------------
/* Converts the set of chars into a hex string in lower case */
string SHA256SumValue::Value() const
{
   char Conv[16] =
      { '0','1','2','3','4','5','6','7','8','9','a','b',
      'c','d','e','f'
   };
   char Result[129];
   Result[128] = 0;

   // Convert each char into two letters
   int J = 0;
   int I = 0;
   for (; I != 128; J++,I += 2)
   {
      Result[I] = Conv[Sum[J] >> 4];
      Result[I + 1] = Conv[Sum[J] & 0xF];
   }

   return string(Result);
}
									/*}}}*/
// SHA256SumValue::operator == - Comparator                            /*{{{*/
// ---------------------------------------------------------------------
/* Call memcmp on the buffer */
bool SHA256SumValue::operator == (const SHA256SumValue & rhs) const
{
   return memcmp(Sum,rhs.Sum,sizeof(Sum)) == 0;
}
                                                                       /*}}}*/
// SHA256Summation::AddFD - Add content of file into the checksum      /*{{{*/
// ---------------------------------------------------------------------
/* */
bool SHA256Summation::AddFD(int Fd,unsigned long Size)
{
   unsigned char Buf[64 * 64];
   int Res = 0;
   int ToEOF = (Size == 0);
   while (Size != 0 || ToEOF)
   {
      unsigned n = sizeof(Buf);
      if (!ToEOF) n = min(Size,(unsigned long)n);
      Res = read(Fd,Buf,n);
      if (Res < 0 || (!ToEOF && (unsigned) Res != n)) // error, or short read
         return false;
      if (ToEOF && Res == 0) // EOF
         break;
      Size -= Res;
      Add(Buf,Res);
   }
   return true;
}
                                                                       /*}}}*/


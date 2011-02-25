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
#pragma implementation "apt-pkg/sha2.h"
#endif

#include <apt-pkg/sha2.h>
#include <apt-pkg/strutl.h>




SHA512Summation::SHA512Summation()					/*{{{*/
{
   SHA512_Init(&ctx);
   Done = false;
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
bool SHA512Summation::Add(const unsigned char *inbuf,unsigned long len) /*{{{*/
{
   if (Done) 
      return false;
   SHA512_Update(&ctx, inbuf, len);
   return true;
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

